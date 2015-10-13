/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncChild.h"
#include "gfxVsync.h"

#include "nsThreadUtils.h"

extern PRLogModuleInfo *gVsyncLog;
#define VSYNC_LOG(...)  MOZ_LOG(gVsyncLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#define PARENT_STR (XRE_IsParentProcess() ? "Parent" : "Child")

namespace mozilla {
namespace layout {

VsyncChild::VsyncChild(const nsID& aDisplayIdentifier)
  : VsyncSource(aDisplayIdentifier)
  , mObservingVsync(false)
  , mIsShutdown(false)
{
  char idstr[NSID_LENGTH];
  mID.ToProvidedString(idstr);
  VSYNC_LOG("[%s]: VsyncChild %p created for display ID %s\n", PARENT_STR, this, idstr);

  MOZ_ASSERT(NS_IsMainThread());
}

VsyncChild::~VsyncChild()
{
  VSYNC_LOG("[%s]: VsyncChild %p destroyed\n", PARENT_STR, this);
  MOZ_ASSERT(NS_IsMainThread());
}

void
VsyncChild::EnableVsync()
{
  SendObserve();
}

void
VsyncChild::DisableVsync()
{
  SendUnobserve();
}

bool
VsyncChild::SendObserve()
{
  MOZ_ASSERT(NS_IsMainThread());
  VSYNC_LOG("[%s]: VsyncChild %p observing\n", PARENT_STR, this);
  if (!mObservingVsync && !mIsShutdown) {
    mObservingVsync = true;
    PVsyncChild::SendObserve();
  }
  return true;
}

bool
VsyncChild::SendUnobserve()
{
  MOZ_ASSERT(NS_IsMainThread());
  VSYNC_LOG("[%s]: VsyncChild %p unobserving\n", PARENT_STR, this);
  if (mObservingVsync && !mIsShutdown) {
    mObservingVsync = false;
    PVsyncChild::SendUnobserve();
  }
  return true;
}

void
VsyncChild::ActorDestroy(ActorDestroyReason aActorDestroyReason)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIsShutdown);
  mIsShutdown = true;
  VSYNC_LOG("[%s]: VsyncChild %p actor destroyed\n", PARENT_STR, this);
}

bool
VsyncChild::RecvNotify(const TimeStamp& aVsyncTimestamp)
{
  // XXX why is this on the main thread????
  // isn't this supposed to be on the PBackground thread?!
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mIsShutdown);
  if (mObservingVsync) {
    OnVsync(aVsyncTimestamp);
  }
  return true;
}

} // namespace layout
} // namespace mozilla
