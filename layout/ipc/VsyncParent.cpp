/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncParent.h"

#include "BackgroundParent.h"
#include "BackgroundParentImpl.h"
#include "gfxPlatform.h"
#include "mozilla/unused.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "gfxVsync.h"

namespace mozilla {

using namespace ipc;

namespace layout {

/*static*/ already_AddRefed<VsyncParent>
VsyncParent::Create(const nsID& aSourceID)
{
  AssertIsOnBackgroundThread();
  nsRefPtr<gfx::VsyncManager> vsyncManager = gfxPlatform::GetPlatform()->GetHardwareVsync();
  nsRefPtr<VsyncParent> vsyncParent = new VsyncParent();
  vsyncParent->mVsyncSource = vsyncManager->GetSource(aSourceID);
  if (!vsyncParent->mVsyncSource) {
    NS_WARNING("Couldn't obtain vsync source for given display ID, using global display instead!");
    vsyncParent->mVsyncSource = vsyncManager->GetGlobalDisplaySource();
  }
  return vsyncParent.forget();
}

VsyncParent::VsyncParent()
  : mObservingVsync(false)
  , mDestroyed(false)
  , mBackgroundThread(NS_GetCurrentThread())
{
  MOZ_ASSERT(mBackgroundThread);
  AssertIsOnBackgroundThread();
}

VsyncParent::~VsyncParent()
{
  // Since we use NS_INLINE_DECL_THREADSAFE_REFCOUNTING, we can't make sure
  // VsyncParent is always released on the background thread.
}

bool
VsyncParent::NotifyVsync(TimeStamp aTimeStamp)
{
  // Called on hardware vsync thread. We should post to current ipc thread.
  MOZ_ASSERT(!IsOnBackgroundThread());
  nsCOMPtr<nsIRunnable> vsyncEvent =
    NS_NewRunnableMethodWithArg<TimeStamp>(this,
                                           &VsyncParent::DispatchVsyncEvent,
                                           aTimeStamp);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(mBackgroundThread->Dispatch(vsyncEvent, NS_DISPATCH_NORMAL)));
  return true;
}

void
VsyncParent::DispatchVsyncEvent(TimeStamp aTimeStamp)
{
  AssertIsOnBackgroundThread();

  // If we call NotifyVsync() when we handle ActorDestroy() message, we might
  // still call DispatchVsyncEvent().
  // Similarly, we might also receive RecvUnobserveVsync() when call
  // NotifyVsync(). We use mObservingVsync and mDestroyed flags to skip this
  // notification.
  if (mObservingVsync && !mDestroyed) {
    unused << SendNotify(aTimeStamp);
  }
}

bool
VsyncParent::RecvObserve()
{
  AssertIsOnBackgroundThread();
  if (!mObservingVsync) {
    mVsyncSource->AddVsyncObserver(this);
    mObservingVsync = true;
    return true;
  }
  return false;
}

bool
VsyncParent::RecvUnobserve()
{
  AssertIsOnBackgroundThread();
  if (mObservingVsync) {
    mVsyncSource->RemoveVsyncObserver(this);
    mObservingVsync = false;
    return true;
  }
  return false;
}

void
VsyncParent::ActorDestroy(ActorDestroyReason aReason)
{
  MOZ_ASSERT(!mDestroyed);
  AssertIsOnBackgroundThread();
  if (mObservingVsync) {
    mVsyncSource->RemoveVsyncObserver(this);
  }
  mVsyncSource = nullptr;
  mDestroyed = true;
}

} // namespace layout
} // namespace mozilla
