/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VSYNCSOURCE_H
#define GFX_VSYNCSOURCE_H

#include "nsTArray.h"
#include "mozilla/nsRefPtr.h"
#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"
#include "nsISupportsImpl.h"
#include "nsID.h"

namespace mozilla {
class VsyncObserver;

namespace gfx {

class VsyncDisplay;

// An observer class, with a single notify method that is called when
// vsync occurs.
class VsyncObserver
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncObserver)

public:
  // The method called when a vsync occurs. Return true if some work was done.
  // In general, this vsync notification will occur on the hardware vsync
  // thread from VsyncSource. But it might also be called on PVsync ipc thread
  // if this notification is cross process. Thus all observer should check the
  // thread model before handling the real task.
  virtual bool NotifyVsync(TimeStamp aVsyncTimestamp) = 0;

protected:
  VsyncObserver() {}
  virtual ~VsyncObserver() {}
}; // VsyncObserver

// Controls how and when to enable/disable vsync. Lives as long as the
// gfxPlatform does on the parent process
class VsyncSource
{
  friend class VsyncDisplay;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncSource)

public:
  // the GUID the global display must use
  static const nsID kGlobalDisplayID;

  VsyncSource();
  
  virtual void RegisterDisplay(VsyncDisplay* aDisplay);
  virtual void UnregisterDisplay(const nsID& aDisplayID);
  virtual already_AddRefed<VsyncDisplay> GetDisplay(const nsID& aDisplayID);

  already_AddRefed<VsyncDisplay> GetGlobalDisplay() {
    return GetDisplay(kGlobalDisplayID);
  }

protected:
  virtual ~VsyncSource() {}

  Monitor mDisplaysMonitor;
  nsTArray<nsRefPtr<VsyncDisplay>> mDisplays;
};

// Controls vsync unique to each display and unique on each platform
class VsyncDisplay {
  friend class VysncSource;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VsyncDisplay)

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

protected:
  // constructs a new Display using the given identifier
  VsyncDisplay(const nsID& aID);
  virtual ~VsyncDisplay();

  nsID mID;

private:
  void UpdateVsyncStatus();

  Monitor mObserversMonitor;
  nsTArray<nsRefPtr<VsyncObserver>> mVsyncObservers;
};

} // namespace gfx
} // namespace mozilla

#endif /* GFX_VSYNCSOURCE_H */
