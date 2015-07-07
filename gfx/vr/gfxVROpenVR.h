/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_OPENVR_H
#define GFX_VR_OPENVR_H

#include "nsTArray.h"
#include "nsIScreen.h"
#include "nsCOMPtr.h"
#include "nsRefPtr.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/EnumeratedArray.h"

#include "gfxVR.h"

#include "openvr/openvr.h"

namespace mozilla {
namespace gfx {
namespace impl {

class HMDInfoOpenVR : public VRHMDInfo, public VRHMDRenderingSupport {
public:
  explicit HMDInfoOpenVR(vr::IVRSystem *aVRSystem, vr::IVRCompositor *aVRCompositor);

  bool SetFOV(const VRFieldOfView& aFOVLeft, const VRFieldOfView& aFOVRight,
              double zNear, double zFar) override;

  bool StartSensorTracking() override;
  VRHMDSensorState GetSensorState(double timeOffset) override;
  void StopSensorTracking() override;
  void ZeroSensor() override;

  void FillDistortionConstants(uint32_t whichEye,
                               const IntSize& textureSize, const IntRect& eyeViewport,
                               const Size& destViewport, const Rect& destRect,
                               VRDistortionConstants& values) override;

  VRHMDRenderingSupport* GetRenderingSupport() override { return this; }
  
  void Destroy();

  /* VRHMDRenderingSupport */
  already_AddRefed<RenderTargetSet> CreateRenderTargetSet(layers::Compositor *aCompositor, const IntSize& aSize) override;
  void DestroyRenderTargetSet(RenderTargetSet *aRTSet) override;
  void SubmitFrame(RenderTargetSet *aRTSet) override;

  vr::IVRSystem *GetOpenVRSystem() const { return mVRSystem; }

protected:
  // must match the size of VRDistortionVertex
  struct DistortionVertex {
    float pos[2];
    float texR[2];
    float texG[2];
    float texB[2];
    float genericAttribs[4];
  };

  virtual ~HMDInfoOpenVR() {
      Destroy();
      MOZ_COUNT_DTOR_INHERITED(HMDInfoOpenVR, VRHMDInfo);
  }

  // not owned by us; global from OpenVR
  vr::IVRSystem *mVRSystem;
  vr::IVRCompositor *mVRCompositor;

  vr::TrackedDevicePose_t mLastTrackingPoses[vr::k_unMaxTrackedDeviceCount];
};

} // namespace impl

class VRHMDManagerOpenVR : public VRHMDManager
{
public:
  VRHMDManagerOpenVR()
    : mOpenVRInitialized(false), mOpenVRPlatformInitialized(false)
  { }

  virtual bool PlatformInit() override;
  virtual bool Init() override;
  virtual void Destroy() override;
  virtual void GetHMDs(nsTArray<nsRefPtr<VRHMDInfo> >& aHMDResult) override;
protected:
  // there can only be one
  nsRefPtr<impl::HMDInfoOpenVR> mOpenVRHMD;
  bool mOpenVRInitialized;
  bool mOpenVRPlatformInitialized;
};

} // namespace gfx
} // namespace mozilla


#endif /* GFX_VR_OPENVR_H */
