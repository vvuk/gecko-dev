/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncSource.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "MainThreadUtils.h"

namespace mozilla {
namespace gfx {

/* static */ const nsID VsyncSource::kGlobalDisplayID =
  { 0xd87dab51, 0x85cd, 0x475b, { 0x9a, 0x9c, 0xb7, 0x48, 0x3a, 0x46, 0x2c, 0xca } };

VsyncSource::VsyncSource()
  : mDisplaysMonitor("VsyncSource displays monitor")
{
}

VsyncDisplay::VsyncDisplay(const nsID& aID)
  : mID(aID)
  , mObserversMonitor("VsyncSource Display observers mutation monitor")
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncDisplay::~VsyncDisplay()
{
  MOZ_ASSERT(NS_IsMainThread());
}

void
VsyncDisplay::AddVsyncObserver(VsyncObserver *aObserver)
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
VsyncDisplay::RemoveVsyncObserver(VsyncObserver *aObserver)
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
VsyncDisplay::OnVsync(TimeStamp aVsyncTimestamp)
{
  // Called on the vsync thread
  MonitorAutoLock lock(mObserversMonitor);
  for (size_t i = 0; i < mVsyncObservers.Length(); ++i) {
    mVsyncObservers[i]->NotifyVsync(aVsyncTimestamp);
  }
}

void
VsyncDisplay::UpdateVsyncStatus()
{
  // always dispatch this to the Main thread, because
  // EnableVsync/DisableVsync can only be called on it
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NS_NewRunnableMethod(this, &VsyncDisplay::UpdateVsyncStatus));
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

void
VsyncSource::RegisterDisplay(VsyncDisplay* aDisplay)
{
  const nsID& id = aDisplay->ID();
  
  MonitorAutoLock lock(mDisplaysMonitor);

  // check if it already exists
  for (size_t i = 0; i < mDisplays.Length(); ++i) {
    if (id == mDisplays[i]->ID()) {
      MOZ_ASSERT(aDisplay == mDisplays[i], "RegisterDisplay: different display already registered with same ID!");

      NS_WARNING("RegisterDisplay: display already registered!");
      break;
    }
  }

  mDisplays.AppendElement(aDisplay);
}

void
VsyncSource::UnregisterDisplay(const nsID& aDisplayID)
{
  MonitorAutoLock lock(mDisplaysMonitor);

  for (size_t i = 0; i < mDisplays.Length(); ++i) {
    if (aDisplayID == mDisplays[i]->ID()) {
      mDisplays.RemoveElementAt(i);
      return;
    }
  }
  
  NS_WARNING("UnregisterDisplay: tried to unregister display that wasn't registered!");
}

already_AddRefed<VsyncDisplay>
VsyncSource::GetDisplay(const nsID& aDisplayID)
{
  MonitorAutoLock lock(mDisplaysMonitor);

  for (size_t i = 0; i < mDisplays.Length(); ++i) {
    if (aDisplayID == mDisplays[i]->ID()) {
      nsRefPtr<VsyncDisplay> dpy = mDisplays[i];
      return dpy.forget();
    }
  }

  return nullptr;
}

void
VsyncSource::GetDisplays(nsTArray<nsRefPtr<VsyncDisplay>>& aDisplays)
{
  MonitorAutoLock lock(mDisplaysMonitor);

  for (size_t i = 0; i < mDisplays.Length(); ++i) {
    aDisplays.AppendElement(mDisplays[i]);
  }
}

} //namespace gfx
} //namespace mozilla
