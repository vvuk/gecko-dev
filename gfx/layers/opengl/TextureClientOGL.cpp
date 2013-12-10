/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/TextureClientOGL.h"
#include "GLContext.h"                  // for GLContext, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/layers/ISurfaceAllocator.h"
#include "nsSize.h"                     // for nsIntSize
#include "gfxPlatform.h"

#include "skia/SkGpuDevice.h"
#include "GLContextSkia.h"
#include "mozilla/gfx/2D.h"

using namespace mozilla::gl;

namespace mozilla {
namespace layers {

class CompositableForwarder;

SharedTextureClientOGL::SharedTextureClientOGL(TextureFlags aFlags)
  : TextureClient(aFlags)
  , mHandle(0)
  , mInverted(false)
{
  // SharedTextureClient is always owned externally.
  mFlags |= TEXTURE_DEALLOCATE_CLIENT;
}

SharedTextureClientOGL::~SharedTextureClientOGL()
{
  // the shared data is owned externally.
}


bool
SharedTextureClientOGL::ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor)
{
  MOZ_ASSERT(IsValid());
  if (!IsAllocated()) {
    return false;
  }
  nsIntSize nsSize(mSize.width, mSize.height);
  aOutDescriptor = SharedTextureDescriptor(mShareType, mHandle, nsSize, mInverted);
  return true;
}

void
SharedTextureClientOGL::InitWith(gl::SharedTextureHandle aHandle,
                                 gfx::IntSize aSize,
                                 gl::SharedTextureShareType aShareType,
                                 bool aInverted)
{
  MOZ_ASSERT(IsValid());
  MOZ_ASSERT(!IsAllocated());
  mHandle = aHandle;
  mSize = aSize;
  mShareType = aShareType;
  mInverted = aInverted;
  if (mInverted) {
    AddFlags(TEXTURE_NEEDS_Y_FLIP);
  }
}

bool
SharedTextureClientOGL::IsAllocated() const
{
  return mHandle != 0;
}

DeprecatedTextureClientSharedOGL::DeprecatedTextureClientSharedOGL(CompositableForwarder* aForwarder,
                                               const TextureInfo& aTextureInfo)
  : DeprecatedTextureClient(aForwarder, aTextureInfo)
  , mGL(nullptr)
{
}

void
DeprecatedTextureClientSharedOGL::ReleaseResources()
{
  if (!IsSurfaceDescriptorValid(mDescriptor)) {
    return;
  }
  MOZ_ASSERT(mDescriptor.type() == SurfaceDescriptor::TSharedTextureDescriptor);
  mDescriptor = SurfaceDescriptor();
  // It's important our handle gets released! SharedDeprecatedTextureHostOGL will take
  // care of this for us though.
}

bool
DeprecatedTextureClientSharedOGL::EnsureAllocated(gfx::IntSize aSize,
                                        gfxContentType aContentType)
{
  mSize = aSize;
  return true;
}

///

class SkiaGLTextureClientData : public TextureClientData {
public:
  SkiaGLTextureClientData() { }

  virtual void DeallocateSharedData(ISurfaceAllocator*) MOZ_OVERRIDE
  {
  }
};


SkiaGLSharedTextureClientOGL::SkiaGLSharedTextureClientOGL(gfx::SurfaceFormat aFormat,
                                                           TextureFlags aFlags)
  : TextureClient(aFlags /* | TEXTURE_DEALLOCATE_CLIENT*/),
    mFormat(aFormat),
    mSize(-1, -1),
    mGLTextureID(0)
{
}

SkiaGLSharedTextureClientOGL::~SkiaGLSharedTextureClientOGL()
{
}

bool
SkiaGLSharedTextureClientOGL::Lock(OpenMode aMode)
{
  MOZ_ASSERT(!mDrawTarget);

  // XXX lock global process-wide mutex for this

  return true;
}

void
SkiaGLSharedTextureClientOGL::Unlock()
{
  if (mDrawTarget) {
    mDrawTarget->Flush();
    GLContextSkia *cxskia = gfxPlatform::GetPlatform()->GetContentGLContextSkia();
    GLContext *gl = cxskia->GetGLContext();


    // XXX insert fence into stream and set the global fence id to it
    gl->MakeCurrent();
    gl->fFinish();

    mDrawTarget = nullptr;
  }

  // XXX unlock global process-wide mutex
}

bool
SkiaGLSharedTextureClientOGL::ToSurfaceDescriptor(SurfaceDescriptor& aDescriptor)
{
  aDescriptor = SurfaceDescriptorSharedGLTexture(mGLTextureID, mFormat,
                                                 mSize, true);
  return true;
}

TextureClientData*
SkiaGLSharedTextureClientOGL::DropTextureData()
{
  return nullptr;
}

TemporaryRef<gfx::DrawTarget>
SkiaGLSharedTextureClientOGL::GetAsDrawTarget()
{
  MOZ_ASSERT(mGLTextureID != 0);

  if (!mDrawTarget) {
    GLContextSkia *cxskia = gfxPlatform::GetPlatform()->GetContentGLContextSkia();
    mDrawTarget = mozilla::gfx::Factory::CreateDrawTargetSkiaWithGLContextSkia(cxskia,
                                                                               cxskia->GetGrContext(),
                                                                               mSize,
                                                                               mFormat,
                                                                               mGLTextureID);
  }

  return mDrawTarget;
}

bool
SkiaGLSharedTextureClientOGL::AllocateForSurface(gfx::IntSize aSize,
                                                 TextureAllocationFlags flags)
{
  MOZ_ASSERT(!IsAllocated());

  mSize = aSize;

  GLContextSkia *cxskia = gfxPlatform::GetPlatform()->GetContentGLContextSkia();
  GLContext *gl = cxskia->GetGLContext();

  gl->MakeCurrent();
  GLuint tex = 0;
  gl->fGenTextures(1, &tex);
  // XXX do we need to save/restore the texture binding?
  gl->fBindTexture(LOCAL_GL_TEXTURE_2D, tex);

  // hacky, but this needs to match what HelpersSkia does and what skia does internally
  GLenum iformat, eformat, etype;
  switch (mFormat) {
  case FORMAT_B8G8R8A8:
  case FORMAT_B8G8R8X8:
    // XXX we probably want BGRA somewhere here for efficiency?
    iformat = LOCAL_GL_RGBA;
    eformat = LOCAL_GL_RGBA;
    etype = LOCAL_GL_UNSIGNED_BYTE;
    break;
  case FORMAT_R5G6B5:
    iformat = LOCAL_GL_RGB;
    eformat = LOCAL_GL_RGB;
    etype = LOCAL_GL_UNSIGNED_SHORT_5_6_5;
    break;
  case FORMAT_A8:
    iformat = LOCAL_GL_ALPHA;
    eformat = LOCAL_GL_ALPHA;
    etype = LOCAL_GL_UNSIGNED_BYTE;
    break;
  default:
    // ???
    iformat = LOCAL_GL_RGBA;
    eformat = LOCAL_GL_RGBA;
    etype = LOCAL_GL_UNSIGNED_BYTE;
    break;
  }

  gl->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, iformat,
                  aSize.width, aSize.height, 0,
                  eformat, etype, nullptr);

  mGLTextureID = tex;

  return true;
}

} // namespace
} // namespace
