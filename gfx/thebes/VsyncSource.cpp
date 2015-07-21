/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

nsRefPtr<RefreshTimerVsyncDispatcher>
VsyncSource::GetRefreshTimerVsyncDispatcher()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  return GetGlobalDisplay().GetRefreshTimerVsyncDispatcher();
}

VsyncSource::Display::Display()
  : mDispatcherLock("display dispatcher lock")
  , mRefreshTimerNeedsVsync(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  mRefreshTimerVsyncDispatcher = new RefreshTimerVsyncDispatcher();
}

VsyncSource::Display::~Display()
{
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mDispatcherLock);
  mRefreshTimerVsyncDispatcher = nullptr;
}

void
VsyncSource::Display::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  // Called on the vsync thread
  MutexAutoLock lock(mDispatcherLock);

  mRefreshTimerVsyncDispatcher->NotifyVsync(aVsyncTimestamp);
}

void
VsyncSource::Display::NotifyRefreshTimerVsyncStatus(bool aEnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  mRefreshTimerNeedsVsync = aEnable;
  UpdateVsyncStatus();
}

void
VsyncSource::Display::UpdateVsyncStatus()
{
  MOZ_ASSERT(NS_IsMainThread());
  // WARNING: This function SHOULD NOT BE CALLED WHILE HOLDING LOCKS
  // NotifyVsync grabs a lock to dispatch vsync events
  // When disabling vsync, we wait for the underlying thread to stop on some platforms
  // We can deadlock if we wait for the underlying vsync thread to stop
  // while the vsync thread is in NotifyVsync.
  bool enableVsync = false;
  { // scope lock
    MutexAutoLock lock(mDispatcherLock);
    enableVsync = mRefreshTimerNeedsVsync;
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

nsRefPtr<RefreshTimerVsyncDispatcher>
VsyncSource::Display::GetRefreshTimerVsyncDispatcher()
{
  return mRefreshTimerVsyncDispatcher;
}

} //namespace gfx
} //namespace mozilla
