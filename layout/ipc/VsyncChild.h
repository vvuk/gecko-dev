/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layout_ipc_VsyncChild_h
#define mozilla_layout_ipc_VsyncChild_h

#include "mozilla/layout/PVsyncChild.h"
#include "nsISupportsImpl.h"
#include "mozilla/nsRefPtr.h"
#include "VsyncSource.h"

namespace mozilla {
namespace ipc {
class BackgroundChildImpl;
} // namespace ipc

namespace layout {

// The PVsyncChild actor receives a vsync event from the main process and
// delivers it to the child process. Currently this is restricted to the main
// thread only. The actor will stay alive until the process dies or its
// PVsyncParent actor dies.
class VsyncChild final :
    public PVsyncChild,
    public gfx::VsyncDisplay
{
  friend class mozilla::ipc::BackgroundChildImpl;

public:
  // VsyncDisplay implementation
  void EnableVsync() override;
  void DisableVsync() override;
  bool IsVsyncEnabled() override { return mObservingVsync; }

  const nsID& DisplayIdentifier() const { return mDisplayIdentifier; }

private:
  explicit VsyncChild(const nsID& aDisplayIdentifier);
  virtual ~VsyncChild();

  // Notify the other end whether we want to receive vsync events. We
  // add an flag mObservingVsync to handle the race problem of
  // unobserving vsync event.
  bool SendObserve();
  bool SendUnobserve();

  virtual bool RecvNotify(const TimeStamp& aVsyncTimestamp) override;
  virtual void ActorDestroy(ActorDestroyReason aActorDestroyReason) override;

  nsID mDisplayIdentifier;
  bool mObservingVsync;
  bool mIsShutdown;
};

} // namespace layout
} // namespace mozilla

#endif  // mozilla_layout_ipc_VsyncChild_h
