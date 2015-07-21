/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncSource.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/VsyncDispatcher.h"
#include "MainThreadUtils.h"

namespace mozilla {
namespace gfx {

VsyncSource::Display::Display()
  : mObserversMonitor("VsyncSource Display observers mutation monitor")
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncSource::Display::~Display()
{
  MOZ_ASSERT(NS_IsMainThread());
}

void
VsyncSource::Display::AddVsyncObserver(VsyncObserver *aObserver)
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
VsyncSource::Display::RemoveVsyncObserver(VsyncObserver *aObserver)
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
VsyncSource::Display::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  // Called on the vsync thread
  MonitorAutoLock lock(mObserversMonitor);
  for (size_t i = 0; i < mVsyncObservers.Length(); ++i) {
    mVsyncObservers[i]->NotifyVsync(aVsyncTimestamp);
  }
}

void
VsyncSource::Display::UpdateVsyncStatus()
{
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

} //namespace gfx
} //namespace mozilla
