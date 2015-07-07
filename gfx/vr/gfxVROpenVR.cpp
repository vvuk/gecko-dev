/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>

#include "prlink.h"
#include "prmem.h"
#include "prenv.h"
#include "gfxPrefs.h"
#include "nsString.h"
#include "mozilla/Preferences.h"

#include "mozilla/gfx/Quaternion.h"

#ifdef XP_WIN
#include "../layers/d3d11/CompositorD3D11.h"
#include "../layers/d3d11/TextureD3D11.h"

#if 0
#ifdef HAVE_64BIT_BUILD
#pragma comment(lib, "c:/proj/openvr/lib/win64/openvr_api.lib")
#else
#pragma comment(lib, "c:/proj/openvr/lib/win32/openvr_api.lib")
#endif
#endif
#endif

#include "gfxVROpenVR.h"

#include "nsServiceManagerUtils.h"
#include "nsIScreenManager.h"

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

using namespace mozilla::gfx;
using namespace mozilla::gfx::impl;

namespace {
extern "C" {
typedef vr::IVRSystem * (VR_CALLTYPE * pfn_VR_Init)(vr::HmdError *peError);
typedef void (VR_CALLTYPE * pfn_VR_Shutdown)();
typedef bool (VR_CALLTYPE * pfn_VR_IsHmdPresent)();
typedef const char * (VR_CALLTYPE * pfn_VR_GetStringForHmdError)(vr::HmdError error);
typedef void * (VR_CALLTYPE * pfn_VR_GetGenericInterface)(const char *pchInterfaceVersion, vr::HmdError *peError);
}

static pfn_VR_Init vr_Init = nullptr;
static pfn_VR_Shutdown vr_Shutdown = nullptr;
static pfn_VR_IsHmdPresent vr_IsHmdPresent = nullptr;
static pfn_VR_GetStringForHmdError vr_GetStringForHmdError = nullptr;
static pfn_VR_GetGenericInterface vr_GetGenericInterface = nullptr;

bool
LoadOpenVRRuntime()
{
  static PRLibrary *openvrLib = nullptr;
  
  nsAdoptingCString openvrPath = Preferences::GetCString("gfx.vr.openvr-runtime");
  if (!openvrPath)
    return false;

  openvrLib = PR_LoadLibrary(openvrPath.BeginReading());
  if (!openvrLib)
    return false;

#define REQUIRE_FUNCTION(_x) do {                                       \
    *(void **)&vr_##_x = (void *) PR_FindSymbol(openvrLib, "VR_" #_x);  \
    if (!vr_##_x) { printf_stderr("VR_" #_x " symbol missing\n"); goto fail; } \
  } while (0)

  REQUIRE_FUNCTION(Init);
  REQUIRE_FUNCTION(Shutdown);
  REQUIRE_FUNCTION(IsHmdPresent);
  REQUIRE_FUNCTION(GetStringForHmdError);
  REQUIRE_FUNCTION(GetGenericInterface);

#undef REQUIRE_FUNCTION

  return true;

 fail:

  return false;
}

}

HMDInfoOpenVR::HMDInfoOpenVR(vr::IVRSystem *aVRSystem, vr::IVRCompositor *aVRCompositor)
  : VRHMDInfo(VRHMDType::OpenVR)
  , mVRSystem(aVRSystem)
  , mVRCompositor(aVRCompositor)
{
  MOZ_COUNT_CTOR_INHERITED(HMDInfoOpenVR, VRHMDInfo);

  mDeviceName.AssignLiteral("OpenVR HMD");
  mSupportedSensorBits = State_Orientation | State_Position;

  mVRCompositor->SetTrackingSpace(vr::TrackingUniverseSeated);

  // SteamVR gives the application a single FOV to use; it's not configurable as with Oculus
  for (uint32_t eye = 0; eye < 2; ++eye) {
    // get l/r/t/b clip plane coordinates
    float l, r, t, b;
    mVRSystem->GetProjectionRaw(static_cast<vr::Hmd_Eye>(eye), &l, &r, &t, &b);
    mEyeFOV[eye].SetFromTanRadians(-t, r, b, -l);
    mRecommendedEyeFOV[eye] = mMaximumEyeFOV[eye] = mEyeFOV[eye];
  }

  SetFOV(mEyeFOV[Eye_Left], mEyeFOV[Eye_Right], 0.01, 10000.0);

  uint32_t xcoord = 0;
  uint32_t w = 2160, h = 1200;
  mScreen = VRHMDManager::MakeFakeScreen(xcoord, 0, std::max(w, h), std::min(w, h));
}

void
HMDInfoOpenVR::Destroy()
{
  mVRSystem = nullptr;
}

bool
HMDInfoOpenVR::SetFOV(const VRFieldOfView& aFOVLeft, const VRFieldOfView& aFOVRight,
                      double zNear, double zFar)
{
  // we can't configure this with OpenVR
  if (aFOVLeft != mRecommendedEyeFOV[Eye_Left] ||
      aFOVRight != mRecommendedEyeFOV[Eye_Right])
  {
    return false;
  }

  mZNear = zNear;
  mZFar = zFar;

  for (uint32_t eye = 0; eye < NumEyes; ++eye) {
    uint32_t w, h;
    mVRSystem->GetRecommendedRenderTargetSize(&w, &h);
    mEyeResolution.width = w;
    mEyeResolution.height = h;

    vr::HmdMatrix34_t eyeToHead = mVRSystem->GetEyeToHeadTransform(static_cast<vr::Hmd_Eye>(eye));

    // XXX should we change mEyeTranslation to just be an eye transform matrix?
    mEyeTranslation[eye].x = eyeToHead.m[0][4];
    mEyeTranslation[eye].y = eyeToHead.m[1][4];
    mEyeTranslation[eye].z = eyeToHead.m[2][4];

    printf_stderr("OPENVR eye %d translation: %f %f %f\n", mEyeTranslation[eye].x, mEyeTranslation[eye].y, mEyeTranslation[eye].z);
    
    mEyeProjectionMatrix[eye] = mEyeFOV[eye].ConstructProjectionMatrix(zNear, zFar, true);
  }

  mConfiguration.hmdType = mType;
  mConfiguration.value = 0;
  mConfiguration.fov[Eye_Left] = aFOVLeft;
  mConfiguration.fov[Eye_Right] = aFOVRight;

  return true;
}

void
HMDInfoOpenVR::FillDistortionConstants(uint32_t whichEye,
                                       const IntSize& textureSize,
                                       const IntRect& eyeViewport,
                                       const Size& destViewport,
                                       const Rect& destRect,
                                       VRDistortionConstants& values)
{
}

bool
HMDInfoOpenVR::StartSensorTracking()
{
  // tracking is always enabled
  return true;
}

void
HMDInfoOpenVR::StopSensorTracking()
{
}

void
HMDInfoOpenVR::ZeroSensor()
{
  mVRSystem->ResetSeatedZeroPose();
}

VRHMDSensorState
HMDInfoOpenVR::GetSensorState(double timeOffset)
{
  {
    vr::VREvent_t event;
    while (mVRSystem->PollNextEvent(&event)) {
      // ignore
    }
  }
  
  vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
  
#if 0
  // XXX do we need to be able to configure between Seated and
  // Standing?  It might not matter, since it just determines what the
  // pose results are relative to.  If we can give content the origin
  // seated pose, we should be able to track things properly.
  mVRSystem->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseSeated, timeOffset,
                                             poses, vr::k_unMaxTrackedDeviceCount);
#else
  // The universe origin is configured in the HMDInfoOpenVR constructor,
  // via SetTrackingSpace()
  // XXX it looks like we *must* call WaitGetPoses in order for any rendering to happen at all
  // XXX we should probably be passing back the rendering-predicted poses (e.g. instead of nullptr,0)
  // read those, and pass those back.
  mVRCompositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0);
#endif

  memcpy(&mLastTrackingPoses, &poses, sizeof(vr::TrackedDevicePose_t) * vr::k_unMaxTrackedDeviceCount);

  VRHMDSensorState result;
  result.timestamp = PR_Now(); // XXX PR_Now? TimeStamp???

  if (poses[vr::k_unTrackedDeviceIndex_Hmd].bDeviceIsConnected &&
      poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid &&
      poses[vr::k_unTrackedDeviceIndex_Hmd].eTrackingResult == vr::TrackingResult_Running_OK)
  {
    const vr::TrackedDevicePose_t& pose = poses[vr::k_unTrackedDeviceIndex_Hmd];

    gfx::Matrix4x4 m;
    // NOTE! mDeviceToAbsoluteTracking is a 3x4 matrix, not 4x4.  But
    // because of its arrangement, we can copy the 12 elements in and
    // then transpose them to the right place.  We do this so we can
    // pull out a Quaternion.
    memcpy(&m._11, &pose.mDeviceToAbsoluteTracking, sizeof(float) * 12);
    m.Transpose();

    gfx::Quaternion rot;
    rot.SetFromRotationMatrix(m);
    rot.Invert();

    gfx::Point3D tx;
    tx.x = m._41;
    tx.y = m._42;
    tx.z = m._43;

    result.flags = State_Orientation | State_Position;
    result.orientation[0] = rot.x;
    result.orientation[1] = rot.y;
    result.orientation[2] = rot.z;
    result.orientation[3] = rot.w;
    result.angularVelocity[0] = pose.vAngularVelocity.v[0];
    result.angularVelocity[1] = pose.vAngularVelocity.v[1];
    result.angularVelocity[2] = pose.vAngularVelocity.v[2];

    result.position[0] = tx.x;
    result.position[1] = tx.y;
    result.position[2] = tx.z;
    result.linearVelocity[0] = pose.vVelocity.v[0];
    result.linearVelocity[1] = pose.vVelocity.v[1];
    result.linearVelocity[2] = pose.vVelocity.v[2];
  } else {
    result.flags = 0;
  }

  result.timestamp = PR_Now(); // XXX PR_Now? TimeStamp???

  return result;
}

struct RenderTargetSetOVR : public VRHMDRenderingSupport::RenderTargetSet
{
  RenderTargetSetOVR(layers::Compositor *aCompositor,
                     const IntSize& aSize,
                     vr::IVRSystem *aVRSystem)
    : mVRSystem(aVRSystem)
  {
    size = aSize;
    currentRenderTarget = 0;

    mCompositorBackend = aCompositor->GetBackendType();

    const uint32_t numTargets = 2;
    renderTargets.SetLength(numTargets);
    for (uint32_t i = 0; i < numTargets; ++i) {
      renderTargets[i] = aCompositor->CreateRenderTarget(gfx::IntRect(0, 0, aSize.width, aSize.height),
                                                         layers::INIT_MODE_NONE);
    }
  }

  bool Valid() const {
    for (uint32_t i = 0; i < renderTargets.Length(); ++i) {
      if (!renderTargets[i])
        return false;
    }
    return true;
  }

  already_AddRefed<layers::CompositingRenderTarget> GetNextRenderTarget() override {
    currentRenderTarget = (currentRenderTarget + 1) % renderTargets.Length();
    renderTargets[currentRenderTarget]->ClearOnBind();
    nsRefPtr<layers::CompositingRenderTarget> rt = renderTargets[currentRenderTarget];
    return rt.forget();
  }
  
  void Destroy() {
    if (!mVRSystem)
      return;

    mVRSystem = nullptr;
  }

  ~RenderTargetSetOVR() {
    Destroy();
  }

  layers::LayersBackend mCompositorBackend;
  vr::IVRSystem* mVRSystem;
};

already_AddRefed<VRHMDRenderingSupport::RenderTargetSet>
HMDInfoOpenVR::CreateRenderTargetSet(layers::Compositor *aCompositor, const IntSize& aSize)
{
#ifdef XP_WIN
  if (aCompositor->GetBackendType() == layers::LayersBackend::LAYERS_D3D11)
  {
    layers::CompositorD3D11 *comp11 = static_cast<layers::CompositorD3D11*>(aCompositor);

    mVRCompositor->SetGraphicsDevice(vr::Compositor_DeviceType_D3D11, comp11->GetDevice());

    nsRefPtr<RenderTargetSetOVR> rts = new RenderTargetSetOVR(comp11, aSize, mVRSystem);
    if (!rts->Valid()) {
      return nullptr;
    }

    return rts.forget();
  }
#endif

  if (aCompositor->GetBackendType() == layers::LayersBackend::LAYERS_OPENGL) {
  }

  return nullptr;
}

void
HMDInfoOpenVR::DestroyRenderTargetSet(RenderTargetSet *aRTSet)
{
  RenderTargetSetOVR *rts = static_cast<RenderTargetSetOVR*>(aRTSet);
  rts->Destroy();
}

void
HMDInfoOpenVR::SubmitFrame(RenderTargetSet *aRTSet)
{
  RenderTargetSetOVR *rts = static_cast<RenderTargetSetOVR*>(aRTSet);
  MOZ_ASSERT(rts->mVRSystem != nullptr);
  MOZ_ASSERT(rts->mVRSystem == mVRSystem);

#ifdef XP_WIN
  if (rts->mCompositorBackend == layers::LayersBackend::LAYERS_D3D11) {
    layers::CompositingRenderTarget *rt = rts->renderTargets[rts->currentRenderTarget].get();
    layers::CompositingRenderTargetD3D11 *rt11 = static_cast<layers::CompositingRenderTargetD3D11*>(rt);
  
    vr::VRTextureBounds_t leftBounds = { 0.0f, 0.0f, 0.5f, 1.0f };
    vr::VRTextureBounds_t rightBounds = { 0.5f, 0.0f, 1.0f, 1.0f };
    mVRCompositor->Submit(vr::Eye_Left, rt11->AsSourceD3D11()->GetD3D11Texture(), &leftBounds);
    mVRCompositor->Submit(vr::Eye_Right, rt11->AsSourceD3D11()->GetD3D11Texture(), &rightBounds);
  }
#endif
}

bool
VRHMDManagerOpenVR::PlatformInit()
{
  if (mOpenVRPlatformInitialized)
    return true;

  if (!gfxPrefs::VREnabled())
    return false;

  if (!LoadOpenVRRuntime())
    return false;
  
  mOpenVRPlatformInitialized = true;
  return true;
}

bool
VRHMDManagerOpenVR::Init()
{
  if (mOpenVRInitialized)
    return true;

  if (!vr_IsHmdPresent())
    return false;

  vr::HmdError err;
  
  vr::IVRSystem *system = nullptr;
  vr::IVRCompositor *compositor = nullptr;

  system = vr_Init(&err);
  if (err || !system) {
    return false;
  }

  compositor = (vr::IVRCompositor*)vr_GetGenericInterface(vr::IVRCompositor_Version, &err);
  if (err || !compositor) {
    vr_Shutdown();
    return false;
  }

  nsRefPtr<HMDInfoOpenVR> hmd = new HMDInfoOpenVR(system, compositor);
  mOpenVRHMD = hmd;

  mOpenVRInitialized = true;
  return true;
}

void
VRHMDManagerOpenVR::Destroy()
{
  if (!mOpenVRInitialized)
    return;

  if (mOpenVRHMD)
    mOpenVRHMD->Destroy();

  mOpenVRHMD = nullptr;

  vr_Shutdown();
  mOpenVRInitialized = false;
}

void
VRHMDManagerOpenVR::GetHMDs(nsTArray<nsRefPtr<VRHMDInfo>>& aHMDResult)
{
  Init();
  if (mOpenVRHMD) {
    aHMDResult.AppendElement(mOpenVRHMD);
  }
}
