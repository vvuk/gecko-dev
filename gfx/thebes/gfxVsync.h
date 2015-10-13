/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VSYNC_H
#define GFX_VSYNC_H

#include "nsTArray.h"
#include "mozilla/nsRefPtr.h"
#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"
#include "nsISupportsImpl.h"
#include "nsID.h"

class CancelableTask;
namespace base {
class Thread;
}

namespace mozilla {
namespace gfx {

class VsyncSource;

// An observer class, with a single notify method that is called when
// vsync occurs.
class VsyncObserver
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncObserver)

public:
  // The method called when a vsync occurs. Return true if some work was done.
  // In general, this vsync notification will occur on the hardware vsync
  // thread from VsyncManager. But it might also be called on PVsync ipc thread
  // if this notification is cross process. Thus all observer should check the
  // thread model before handling the real task.
  virtual bool NotifyVsync(TimeStamp aVsyncTimestamp) = 0;

protected:
  VsyncObserver() {}
  virtual ~VsyncObserver() {}
}; // VsyncObserver

// Controls how and when to enable/disable vsync. Lives as long as the
// gfxPlatform does on the parent process
class VsyncManager
{
  friend class VsyncSource;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncManager)

public:
  // the GUID the global display source must use
  static const nsID kGlobalDisplaySourceID;

  VsyncManager();

  virtual void RegisterSource(VsyncSource* aSource);
  virtual void UnregisterSource(const nsID& aSourceID);
  virtual already_AddRefed<VsyncSource> GetSource(const nsID& aSourceID);
  virtual void GetSources(nsTArray<nsRefPtr<VsyncSource>>& aSources);

  already_AddRefed<VsyncSource> GetGlobalDisplaySource() {
    return GetSource(kGlobalDisplaySourceID);
  }

  virtual void Shutdown();

protected:
  virtual ~VsyncManager();

  // return index of aSourceID in mSources, or -1 if not found.
  // Assumes mSourcesMonitor is locked.
  int32_t GetSourceIndex(const nsID& aSourceID);

  Monitor mSourcesMonitor;
  nsTArray<nsRefPtr<VsyncSource>> mSources;
};

// Controls vsync unique to each display and unique on each platform
class VsyncSource {
  friend class VysncSource;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncSource)

public:
 // return a UUID that can be used to identify this display (during
 // this process lifetime only)
 const nsID& ID() const { return mID; }

  // Add or remove a vsync observer to be notified when vsync
  // happens.  This notification will happen *on the vsync
  // thread*, so if you expect to do any significant work, get off
  // that thread ASAP in the observer callback.
  void AddVsyncObserver(VsyncObserver *aObserver);
  bool RemoveVsyncObserver(VsyncObserver *aObserver);

  // Notified when this display's vsync occurs, on the vsync thread
  // The aVsyncTimestamp should normalize to the Vsync time that just occured
  // However, different platforms give different vsync notification times.
  // b2g - The vsync timestamp of the previous frame that was just displayed
  // OSX - The vsync timestamp of the upcoming frame, in the future
  // TODO: Windows / Linux. DOCUMENT THIS WHEN IMPLEMENTING ON THOSE PLATFORMS
  // Android: TODO
  // All platforms should normalize to the vsync that just occured.
  // Large parts of Gecko assume TimeStamps should not be in the future such as animations
  virtual void OnVsync(TimeStamp aVsyncTimestamp);

  // These must only be called on the main thread.  You should not need
  // to call these directly; adding or removing a vsync observer will
  // enable or disable vsync as necessary.
  virtual void EnableVsync() = 0;
  virtual void DisableVsync() = 0;
  virtual bool IsVsyncEnabled() = 0;

  virtual void Shutdown() { }

protected:
  // constructs a new Display using the given identifier
  VsyncSource(const nsID& aID);
  virtual ~VsyncSource();

  nsID mID;

private:
  void UpdateVsyncStatus();

  Monitor mObserversMonitor;
  nsTArray<nsRefPtr<VsyncObserver>> mVsyncObservers;
};

/**
 * A pure software-timer driven vsync generator
 */
class SoftwareVsyncSource final : public VsyncSource
{
public:
  explicit SoftwareVsyncSource(const nsID& aSourceID,
                           double aInterval = 1000.0 / 60.0);
  virtual void EnableVsync() override;
  virtual void DisableVsync() override;
  virtual bool IsVsyncEnabled() override;
  bool IsInSoftwareVsyncThread();
  virtual void OnVsync(mozilla::TimeStamp aVsyncTimestamp) override;
  void ScheduleNextVsync(mozilla::TimeStamp aVsyncTimestamp);
  virtual void Shutdown() override;

protected:
  ~SoftwareVsyncSource();

  void InternalEnableVsync();
  void InternalDisableVsync();

  mozilla::TimeDuration mVsyncRate;
  // Use a chromium thread because nsITimers* fire on the main thread
  base::Thread* mVsyncThread;
  CancelableTask* mCurrentVsyncTask; // only access on vsync thread
  bool mVsyncEnabled; // Only access on main thread
}; // SoftwareVsyncSource

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VSYNC_H */
