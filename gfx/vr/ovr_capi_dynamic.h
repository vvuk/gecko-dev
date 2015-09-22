/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file contains just the needed struct definitions for
 * interacting with the Oculus VR C API, without needing to #include
 * OVR_CAPI.h directly.  Note that it uses the same type names as the
 * CAPI, and cannot be #included at the same time as OVR_CAPI.h.  It
 * does not include the entire C API, just want's needed.
 */

#ifdef OVR_CAPI_h
#ifdef _MSC_VER
#pragma message("ovr_capi_dyanmic.h: OVR_CAPI.h included before ovr_capi_dynamic.h, skipping this")
#else
#warning OVR_CAPI.h included before ovr_capi_dynamic.h, skipping this
#endif
#define mozilla_ovr_capi_dynamic_h_

#else

#ifndef mozilla_ovr_capi_dynamic_h_
#define mozilla_ovr_capi_dynamic_h_

#define OVR_CAPI_LIMITED_MOZILLA 1

#ifdef HAVE_64BIT_BUILD
#define OVR_PTR_SIZE 8
#define OVR_ON64(x)     x
#else
#define OVR_PTR_SIZE 4
#define OVR_ON64(x)     /**/
#endif

#if defined(_WIN32)
#define OVR_PFN __cdecl
#else
#define OVR_PFN
#endif

#if !defined(OVR_ALIGNAS)
#if defined(__GNUC__) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 408) && (defined(__GXX_EXPERIMENTAL_CXX0X__) || (__cplusplus >= 201103L))
#define OVR_ALIGNAS(n) alignas(n)
#elif defined(__clang__) && !defined(__APPLE__) && (((__clang_major__ * 100) + __clang_minor__) >= 300) && (__cplusplus >= 201103L)
#define OVR_ALIGNAS(n) alignas(n)
#elif defined(__clang__) && defined(__APPLE__) && (((__clang_major__ * 100) + __clang_minor__) >= 401) && (__cplusplus >= 201103L)
#define OVR_ALIGNAS(n) alignas(n)
#elif defined(_MSC_VER) && (_MSC_VER >= 1900)
#define OVR_ALIGNAS(n) alignas(n)
#elif defined(__EDG_VERSION__) && (__EDG_VERSION__ >= 408)
#define OVR_ALIGNAS(n) alignas(n)
#elif defined(__GNUC__) || defined(__clang__)
#define OVR_ALIGNAS(n) __attribute__((aligned(n)))
#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define OVR_ALIGNAS(n) __declspec(align(n))
#else
#error Need to define OVR_ALIGNAS
#endif
#endif

#ifdef __cplusplus 
extern "C" {
#endif

typedef int32_t ovrResult;
typedef char ovrBool;
typedef struct OVR_ALIGNAS(4) { int x, y; } ovrVector2i;
typedef struct OVR_ALIGNAS(4) { int w, h; } ovrSizei;
typedef struct OVR_ALIGNAS(4) { ovrVector2i Pos; ovrSizei Size; } ovrRecti;
typedef struct OVR_ALIGNAS(4) { float x, y, z, w; } ovrQuatf;
typedef struct OVR_ALIGNAS(4) { float x, y; } ovrVector2f;
typedef struct OVR_ALIGNAS(4) { float x, y, z; } ovrVector3f;
typedef struct OVR_ALIGNAS(4) { float M[4][4]; } ovrMatrix4f;

typedef struct OVR_ALIGNAS(4) {
  ovrQuatf Orientation;
  ovrVector3f Position;
} ovrPosef;

typedef struct OVR_ALIGNAS(8) {
  ovrPosef ThePose;
  ovrVector3f AngularVelocity;
  ovrVector3f LinearVelocity;
  ovrVector3f AngularAcceleration;
  ovrVector3f LinearAcceleration;
  float       Pad;
  double      TimeInSeconds;
} ovrPoseStatef;

typedef struct {
  float UpTan;
  float DownTan;
  float LeftTan;
  float RightTan;
} ovrFovPort;

typedef enum {
  ovrHmd_None      = 0,    
  ovrHmd_DK1       = 3,
  ovrHmd_DKHD      = 4,
  ovrHmd_DK2       = 6,
  ovrHmd_CB        = 8,
  ovrHmd_Other     = 9,
  ovrHmd_E3_2015   = 10,
  ovrHmd_ES06      = 11,
  ovrHmd_EnumSize = 0x7fffffff
} ovrHmdType;

typedef enum {
  ovrHmdCap_Writable_Mask     = 0x0000,
  ovrHmdCap_Service_Mask      = 0x0000,
  ovrHmdCap_DebugDevice       = 0x0010,
  ovrHmdCap_EnumSize          = 0x7fffffff
} ovrHmdCaps;

typedef enum
{
  ovrTrackingCap_Orientation      = 0x0010,
  ovrTrackingCap_MagYawCorrection = 0x0020,
  ovrTrackingCap_Position         = 0x0040,
  ovrTrackingCap_Idle             = 0x0100,
  ovrTrackingCap_EnumSize         = 0x7fffffff
} ovrTrackingCaps;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  char Reserved[8];
} ovrGraphicsLuid;

typedef enum {
  ovrEye_Left  = 0,
  ovrEye_Right = 1,
  ovrEye_Count = 2,
  ovrEye_EnumSize = 0x7fffffff
} ovrEyeType;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrHmdType  Type;
  OVR_ON64(unsigned char pad0[4];)
  char ProductName[64];
  char Manufacturer[64];
  short VendorId;
  short ProductId;
  char SerialNumber[24];
  short FirmwareMajor;
  short FirmwareMinor;
  float CameraFrustumHFovInRadians;
  float CameraFrustumVFovInRadians;
  float CameraFrustumNearZInMeters;
  float CameraFrustumFarZInMeters;

  unsigned int AvailableHmdCaps;
  unsigned int DefaultHmdCaps;
  unsigned int AvailableTrackingCaps;
  unsigned int DefaultTrackingCaps;

  ovrFovPort  DefaultEyeFov[ovrEye_Count];
  ovrFovPort  MaxEyeFov[ovrEye_Count];
  ovrSizei    Resolution;
  float DisplayRefreshRate;
  OVR_ON64(unsigned char pad1[4];)
} ovrHmdDesc;

typedef struct ovrHmdStruct* ovrHmd;

typedef enum {
  ovrStatus_OrientationTracked    = 0x0001,
  ovrStatus_PositionTracked       = 0x0002,
  ovrStatus_CameraPoseTracked     = 0x0004,
  ovrStatus_PositionConnected     = 0x0020,
  ovrStatus_HmdConnected          = 0x0080,
  ovrStatus_EnumSize              = 0x7fffffff
} ovrStatusBits;

typedef struct OVR_ALIGNAS(4) {
  ovrVector3f    Accelerometer;
  ovrVector3f    Gyro;
  ovrVector3f    Magnetometer;
  float          Temperature;
  float          TimeInSeconds;
} ovrSensorData;


typedef struct OVR_ALIGNAS(8) {
  ovrPoseStatef HeadPose;
  ovrPosef CameraPose;
  ovrPosef LeveledCameraPose;
  ovrPoseStatef HandPoses[2];
  ovrSensorData RawSensorData;
  unsigned int StatusFlags;
  uint32_t LastCameraFrameCounter;
  unsigned char pad0[4];
} ovrTrackingState;

typedef struct OVR_ALIGNAS(8) {
  double DisplayMidpointSeconds;
  double FrameIntervalSeconds;
  unsigned AppFrameIndex;
  unsigned DisplayFrameIndex;
} ovrFrameTiming;

typedef struct OVR_ALIGNAS(4) {
  ovrEyeType  Eye;
  ovrFovPort  Fov;
  ovrRecti DistortedViewport;
  ovrVector2f PixelsPerTanAngleAtCenter;
  ovrVector3f HmdToEyeViewOffset;
} ovrEyeRenderDesc;

typedef struct OVR_ALIGNAS(4) {
  float Projection22;
  float Projection23;
  float Projection32;
} ovrTimewarpProjectionDesc;

typedef struct OVR_ALIGNAS(4) {
  ovrVector3f HmdToEyeViewOffset[ovrEye_Count];
  float HmdSpaceToWorldScaleInMeters;
} ovrViewScaleDesc;

typedef enum {
    ovrRenderAPI_None = 0,
    ovrRenderAPI_OpenGL = 1,
    ovrRenderAPI_Android_GLES = 2,
    ovrRenderAPI_D3D11 = 5,
    ovrRenderAPI_Count = 4,
    ovrRenderAPI_EnumSize = 0x7fffffff
} ovrRenderAPIType;

typedef struct OVR_ALIGNAS(4) {
  ovrRenderAPIType API;
  ovrSizei TextureSize;
} ovrTextureHeader;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrTextureHeader Header;
  OVR_ON64(unsigned char pad0[4];)
  uintptr_t PlatformData[8];
} ovrTexture;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
  ovrTexture* Textures;
  int TextureCount;
  int CurrentIndex;
} ovrSwapTextureSet;

typedef enum {
  ovrInit_Debug          = 0x00000001,
  ovrInit_ServerOptional = 0x00000002,
  ovrInit_RequestVersion = 0x00000004,
  ovrInit_EnumSize       = 0x7fffffff
} ovrInitFlags;

typedef enum {
  ovrLogLevel_Debug = 0,
  ovrLogLevel_Info  = 1,
  ovrLogLevel_Error = 2,
  ovrLogLevel_EnumSize = 0x7fffffff
} ovrLogLevel;

typedef enum {
  ovrLayerType_Disabled       = 0,
  ovrLayerType_EyeFov         = 1,
  ovrLayerType_EyeFovDepth    = 2,
  ovrLayerType_QuadInWorld    = 3,
  ovrLayerType_QuadHeadLocked = 4,
  ovrLayerType_Direct         = 6,
  ovrLayerType_EnumSize       = 0x7fffffff
} ovrLayerType;

typedef enum {
  ovrLayerFlag_HighQuality               = 0x01,
  ovrLayerFlag_TextureOriginAtBottomLeft = 0x02
} ovrLayerFlags;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
    ovrLayerType    Type;
    unsigned        Flags;
} ovrLayerHeader;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
    ovrLayerHeader      Header;
    ovrSwapTextureSet*  ColorTexture[ovrEye_Count];
    ovrRecti            Viewport[ovrEye_Count];
    ovrFovPort          Fov[ovrEye_Count];
    ovrPosef            RenderPose[ovrEye_Count];
} ovrLayerEyeFov;

typedef void (OVR_PFN *ovrLogCallback)(int level, const char* message);

typedef struct OVR_ALIGNAS(8) {
  uint32_t Flags;
  uint32_t RequestedMinorVersion;
  ovrLogCallback LogCallback;
  uintptr_t UserData;
  uint32_t ConnectionTimeoutMS;
  OVR_ON64(unsigned char pad0[4];)
} ovrInitParams;

#define OVR_PERF_HUD_MODE "PerfHudMode"

typedef enum {
  ovrPerfHud_Off = 0,
  ovrPerfHud_LatencyTiming = 1,
  ovrPerfHud_RenderTiming = 2,
  ovrPerfHud_PerfHeadroom = 3,
  ovrPerfHud_VersionInfo = 4,
  ovrPerfHud_Count,
  ovrPerfHud_EnumSize = 0x7fffffff
} ovrPerfHudMode;

enum {
  ovrSuccess = 0,

  ovrError_MemoryAllocationFailure = -1000,
  ovrError_SocketCreationFailure   = -1001,
  ovrError_InvalidHmd              = -1002,
  ovrError_Timeout                 = -1003,
  ovrError_NotInitialized          = -1004,
  ovrError_InvalidParameter        = -1005,
  ovrError_ServiceError            = -1006,
  ovrError_NoHmd                   = -1007,

  ovrError_AudioReservedBegin      = -2000,
  ovrError_AudioReservedEnd        = -2999,

  ovrError_Initialize              = -3000,
  ovrError_LibLoad                 = -3001,
  ovrError_LibVersion              = -3002,
  ovrError_ServiceConnection       = -3003,
  ovrError_ServiceVersion          = -3004,
  ovrError_IncompatibleOS          = -3005,
  ovrError_DisplayInit             = -3006,
  ovrError_ServerStart             = -3007,
  ovrError_Reinitialization        = -3008,

  ovrError_InvalidBundleAdjustment = -4000,
  ovrError_USBBandwidth            = -4001
};

typedef ovrResult (OVR_PFN *pfn_ovr_Initialize)(ovrInitParams const* params);
typedef void (OVR_PFN *pfn_ovr_Shutdown)();
typedef double (OVR_PFN *pfn_ovr_GetTimeInSeconds)();
typedef ovrHmdDesc (OVR_PFN *pfn_ovr_GetHmdDesc)(ovrHmd hmd);
  
typedef ovrResult (OVR_PFN *pfn_ovr_Create)(ovrHmd*, ovrGraphicsLuid*);
typedef void (OVR_PFN *pfn_ovr_Destroy)(ovrHmd hmd);
  
typedef ovrResult (OVR_PFN *pfn_ovr_ConfigureTracking)(ovrHmd hmd, unsigned int requestedTrackingCaps, unsigned int requiredTrackingCaps); 
typedef void (OVR_PFN *pfn_ovr_RecenterPose)(ovrHmd hmd);
typedef ovrTrackingState (OVR_PFN *pfn_ovr_GetTrackingState)(ovrHmd hmd, double absTime);
typedef ovrSizei (OVR_PFN *pfn_ovr_GetFovTextureSize)(ovrHmd hmd, ovrEyeType eye, ovrFovPort fov, float pixelsPerDisplayPixel);
typedef ovrEyeRenderDesc (OVR_PFN *pfn_ovr_GetRenderDesc)(ovrHmd hmd, ovrEyeType eyeType, ovrFovPort fov);

typedef void (OVR_PFN *pfn_ovr_DestroySwapTextureSet)(ovrHmd hmd, ovrSwapTextureSet* textureSet);
typedef ovrResult (OVR_PFN *pfn_ovr_SubmitFrame)(ovrHmd hmd, unsigned int frameIndex,
                                                 const ovrViewScaleDesc* viewScaleDesc,
                                                 ovrLayerHeader const * const * layerPtrList, unsigned int layerCount);

typedef ovrBool (OVR_PFN *pfn_ovr_SetInt)(ovrHmd, const char*, int);

#ifdef XP_WIN
struct D3D11_TEXTURE2D_DESC;
struct ID3D11Device;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

typedef struct OVR_ALIGNAS(OVR_PTR_SIZE) {
    ovrTextureHeader          Header;
    OVR_ON64(uint32_t pad0;)
    ID3D11Texture2D*          pTexture;
    ID3D11ShaderResourceView* pSRView;
} ovrD3D11TextureData;

typedef union {
    ovrTexture          Texture;
    ovrD3D11TextureData D3D11;
} ovrD3D11Texture;

typedef enum {
    ovrSwapTextureSetD3D11_Typeless = 0x0001,
    ovrSwapTextureSetD3D11_EnumSize = 0x7fffffff
} ovrSwapTextureSetD3D11Flags;

typedef ovrResult (OVR_PFN *pfn_ovr_CreateSwapTextureSetD3D11)(ovrHmd hmd, ID3D11Device* device,
                                                               const D3D11_TEXTURE2D_DESC* desc,
                                                               unsigned int miscFlags,
                                                               ovrSwapTextureSet** outTextureSet);
#endif

typedef struct {
    ovrTextureHeader Header;
    uint32_t TexId;
} ovrGLTextureData;

typedef union {
    ovrTexture       Texture;
    ovrGLTextureData OGL;
} ovrGLTexture;

typedef ovrResult (OVR_PFN *pfn_ovr_CreateSwapTextureSetGL)(ovrHmd hmd, uint32_t format,
                                                            int width, int height,
                                                            ovrSwapTextureSet** outTextureSet);

#ifdef __cplusplus 
}
#endif

#endif /* mozilla_ovr_capi_dynamic_h_ */
#endif /* OVR_CAPI_h */
