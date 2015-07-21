/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MainThreadUtils.h"
#include "VsyncDispatcher.h"
#include "VsyncSource.h"
#include "gfxPlatform.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/CompositorParent.h"

#ifdef MOZ_ENABLE_PROFILER_SPS
#include "GeckoProfiler.h"
#include "ProfilerMarkers.h"
#endif

namespace mozilla {

RefreshTimerVsyncDispatcher::RefreshTimerVsyncDispatcher()
  : mRefreshTimersLock("RefreshTimers lock")
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

RefreshTimerVsyncDispatcher::~RefreshTimerVsyncDispatcher()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
}

void
RefreshTimerVsyncDispatcher::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  MutexAutoLock lock(mRefreshTimersLock);

  for (size_t i = 0; i < mChildRefreshTimers.Length(); i++) {
    mChildRefreshTimers[i]->NotifyVsync(aVsyncTimestamp);
  }

  if (mParentRefreshTimer) {
    mParentRefreshTimer->NotifyVsync(aVsyncTimestamp);
  }
}

void
RefreshTimerVsyncDispatcher::SetParentRefreshTimer(VsyncObserver* aVsyncObserver)
{
  MOZ_ASSERT(NS_IsMainThread());
  { // lock scope because UpdateVsyncStatus runs on main thread and will deadlock
    MutexAutoLock lock(mRefreshTimersLock);
    mParentRefreshTimer = aVsyncObserver;
  }

  UpdateVsyncStatus();
}

void
RefreshTimerVsyncDispatcher::AddChildRefreshTimer(VsyncObserver* aVsyncObserver)
{
  { // scope lock - called on pbackground thread
    MutexAutoLock lock(mRefreshTimersLock);
    MOZ_ASSERT(aVsyncObserver);
    if (!mChildRefreshTimers.Contains(aVsyncObserver)) {
      mChildRefreshTimers.AppendElement(aVsyncObserver);
    }
  }

  UpdateVsyncStatus();
}

void
RefreshTimerVsyncDispatcher::RemoveChildRefreshTimer(VsyncObserver* aVsyncObserver)
{
  { // scope lock - called on pbackground thread
    MutexAutoLock lock(mRefreshTimersLock);
    MOZ_ASSERT(aVsyncObserver);
    mChildRefreshTimers.RemoveElement(aVsyncObserver);
  }

  UpdateVsyncStatus();
}

void
RefreshTimerVsyncDispatcher::UpdateVsyncStatus()
{
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIRunnable> vsyncControl = NS_NewRunnableMethod(this,
                                           &RefreshTimerVsyncDispatcher::UpdateVsyncStatus);
    NS_DispatchToMainThread(vsyncControl);
    return;
  }

  gfx::VsyncSource::Display& display = gfxPlatform::GetPlatform()->GetHardwareVsync()->GetGlobalDisplay();
  display.NotifyRefreshTimerVsyncStatus(NeedsVsync());
}

bool
RefreshTimerVsyncDispatcher::NeedsVsync()
{
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mRefreshTimersLock);
  return (mParentRefreshTimer != nullptr) || !mChildRefreshTimers.IsEmpty();
}

} // namespace mozilla
