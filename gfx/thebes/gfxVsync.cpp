/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/unused.h"
#include "gfxVsync.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "MainThreadUtils.h"
#include "base/thread.h"

namespace mozilla {
namespace gfx {

/* static */ const nsID VsyncManager::kGlobalDisplaySourceID =
  { 0xd87dab51, 0x85cd, 0x475b, { 0x9a, 0x9c, 0xb7, 0x48, 0x3a, 0x46, 0x2c, 0xca } };

/**
 * base VsyncSource implementation
 */

VsyncSource::VsyncSource(const nsID& aID)
  : mID(aID)
  , mObserversMonitor("VsyncSource observers mutation monitor")
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncSource::~VsyncSource()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mVsyncObservers.IsEmpty());
}

void
VsyncSource::AddVsyncObserver(VsyncObserver *aObserver)
{
  { // scope lock
    MonitorAutoLock lock(mObserversMonitor);
    if (!mVsyncObservers.Contains(aObserver)) {
      mVsyncObservers.AppendElement(aObserver);
    }
  }

  UpdateVsyncStatus();
}

bool
VsyncSource::RemoveVsyncObserver(VsyncObserver *aObserver)
{
  bool found = false;
  { // scope lock
    MonitorAutoLock lock(mObserversMonitor);
    found = mVsyncObservers.RemoveElement(aObserver);
  }

  if (found)
    UpdateVsyncStatus();
  return found;
}

void
VsyncSource::OnVsync(TimeStamp aVsyncTimestamp)
{
  nsAutoTArray<nsRefPtr<VsyncObserver>,8> observers;

  // Called on the vsync thread
  {
    // make a copy of the observers, so that they can remove themselves
    // from the array if needed while being notified
    MonitorAutoLock lock(mObserversMonitor);
    observers.AppendElements(mVsyncObservers);
  }

  for (size_t i = 0; i < observers.Length(); ++i) {
    observers[i]->NotifyVsync(aVsyncTimestamp);
  }
}

void
VsyncSource::UpdateVsyncStatus()
{
  // always dispatch this to the Main thread, because
  // EnableVsync/DisableVsync can only be called on it
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NS_NewRunnableMethod(this, &VsyncSource::UpdateVsyncStatus));
    return;
  }

  // WARNING: This function SHOULD NOT BE CALLED WHILE HOLDING LOCKS
  // NotifyVsync grabs a lock to dispatch vsync events
  // When disabling vsync, we wait for the underlying thread to stop on some platforms
  // We can deadlock if we wait for the underlying vsync thread to stop
  // while the vsync thread is in NotifyVsync.
  bool enableVsync = false;
  { // scope lock
    MonitorAutoLock lock(mObserversMonitor);
    enableVsync = mVsyncObservers.Length() > 0;
  }

  if (enableVsync) {
    EnableVsync();
  } else {
    DisableVsync();
  }

  if (IsVsyncEnabled() != enableVsync) {
    NS_WARNING("Vsync status did not change.");
  }
}

/**
 * VsyncManager implementation
 */

VsyncManager::VsyncManager()
  : mSourcesMonitor("VsyncManager Sources mutation monitor")
{
}

VsyncManager::~VsyncManager()
{
  Shutdown();
}

int32_t
VsyncManager::GetSourceIndex(const nsID& aSourceID)
{
  mSourcesMonitor.AssertCurrentThreadOwns();

  for (size_t i = 0; i < mSources.Length(); ++i) {
    if (aSourceID == mSources[i]->ID()) {
      return int32_t(i);
    }
  }

  return -1;
}

void
VsyncManager::RegisterSource(VsyncSource* aSource)
{
  const nsID& id = aSource->ID();

  MonitorAutoLock lock(mSourcesMonitor);

  // ensure that it's not already registered
  int32_t existingSourceIndex = GetSourceIndex(id);
  unused << existingSourceIndex;
  MOZ_ASSERT(existingSourceIndex == -1);

  mSources.AppendElement(aSource);
}

void
VsyncManager::UnregisterSource(const nsID& aSourceID)
{
  MonitorAutoLock lock(mSourcesMonitor);

  // ensure that it is registered
  int32_t existingSourceIndex = GetSourceIndex(aSourceID);
  MOZ_ASSERT(existingSourceIndex != -1);

  mSources.RemoveElementAt(existingSourceIndex);
}

already_AddRefed<VsyncSource>
VsyncManager::GetSource(const nsID& aSourceID)
{
  MonitorAutoLock lock(mSourcesMonitor);

  int32_t existingSourceIndex = GetSourceIndex(aSourceID);
  if (existingSourceIndex == -1) {
    return nullptr;
  }

  nsRefPtr<VsyncSource> dpy = mSources[existingSourceIndex];
  return dpy.forget();
}

void
VsyncManager::GetSources(nsTArray<nsRefPtr<VsyncSource>>& aSources)
{
  MonitorAutoLock lock(mSourcesMonitor);

  for (size_t i = 0; i < mSources.Length(); ++i) {
    aSources.AppendElement(mSources[i]);
  }
}

void
VsyncManager::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MonitorAutoLock lock(mSourcesMonitor);

  for (size_t i = 0; i < mSources.Length(); ++i) {
    mSources[i]->Shutdown();
  }

  mSources.Clear();
}

/**
 * SoftwareVsyncSource implementation
 */

SoftwareVsyncSource::SoftwareVsyncSource(const nsID& aSourceID, double aInterval)
  : VsyncSource(aSourceID)
  , mCurrentVsyncTask(nullptr)
  , mVsyncEnabled(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  mVsyncRate = mozilla::TimeDuration::FromMilliseconds(aInterval);
  mVsyncThread = new base::Thread("SoftwareVsyncThread");
  MOZ_RELEASE_ASSERT(mVsyncThread->Start(), "Could not start software vsync thread");
}

SoftwareVsyncSource::~SoftwareVsyncSource()
{
}

void
SoftwareVsyncSource::EnableVsync()
{
  MOZ_ASSERT(mVsyncThread->IsRunning());
  MOZ_ASSERT(NS_IsMainThread());

  if (mVsyncEnabled) {
    return;
  }

  mVsyncEnabled = true;
  mVsyncThread->message_loop()->PostTask(FROM_HERE,
    NewRunnableMethod(this, &SoftwareVsyncSource::InternalEnableVsync));
}

void
SoftwareVsyncSource::InternalEnableVsync()
{
  MOZ_ASSERT(IsInSoftwareVsyncThread());
  OnVsync(mozilla::TimeStamp::Now());
}

void
SoftwareVsyncSource::DisableVsync()
{
  MOZ_ASSERT(mVsyncThread->IsRunning());
  MOZ_ASSERT(NS_IsMainThread());

  if (!mVsyncEnabled) {
    return;
  }

  mVsyncEnabled = false;
  mVsyncThread->message_loop()->PostTask(FROM_HERE,
    NewRunnableMethod(this, &SoftwareVsyncSource::InternalDisableVsync));
}

void
SoftwareVsyncSource::InternalDisableVsync()
{
  MOZ_ASSERT(IsInSoftwareVsyncThread());
  if (mCurrentVsyncTask) {
    mCurrentVsyncTask->Cancel();
    mCurrentVsyncTask = nullptr;
  }
}

bool
SoftwareVsyncSource::IsVsyncEnabled()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mVsyncEnabled;
}

bool
SoftwareVsyncSource::IsInSoftwareVsyncThread()
{
  return mVsyncThread->thread_id() == PlatformThread::CurrentId();
}

void
SoftwareVsyncSource::OnVsync(mozilla::TimeStamp aVsyncTimestamp)
{
  MOZ_ASSERT(IsInSoftwareVsyncThread());

  mozilla::TimeStamp displayVsyncTime = aVsyncTimestamp;
  mozilla::TimeStamp now = mozilla::TimeStamp::Now();
  // Posted tasks can only have integer millisecond delays
  // whereas TimeDurations can have floating point delays.
  // Thus the vsync timestamp can be in the future, which large parts
  // of the system can't handle, including animations. Force the timestamp to be now.
  if (aVsyncTimestamp > now) {
    displayVsyncTime = now;
  }

  VsyncSource::OnVsync(displayVsyncTime);

  // Prevent skew by still scheduling based on the original
  // vsync timestamp
  ScheduleNextVsync(aVsyncTimestamp);
}

void
SoftwareVsyncSource::ScheduleNextVsync(mozilla::TimeStamp aVsyncTimestamp)
{
  MOZ_ASSERT(IsInSoftwareVsyncThread());
  mozilla::TimeStamp nextVsync = aVsyncTimestamp + mVsyncRate;
  mozilla::TimeDuration delay = nextVsync - mozilla::TimeStamp::Now();
  if (delay.ToMilliseconds() < 0) {
    delay = mozilla::TimeDuration::FromMilliseconds(0);
    nextVsync = mozilla::TimeStamp::Now();
  }

  mCurrentVsyncTask = NewRunnableMethod(this,
      &SoftwareVsyncSource::OnVsync,
      nextVsync);

  mVsyncThread->message_loop()->PostDelayedTask(FROM_HERE,
      mCurrentVsyncTask,
      delay.ToMilliseconds());
}

void
SoftwareVsyncSource::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mVsyncThread) {
    DisableVsync();
    mVsyncThread->Stop();
    delete mVsyncThread;
    mVsyncThread = nullptr;
  }
}

} //namespace gfx
} //namespace mozilla
