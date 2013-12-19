/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContext.h"
#include "GLContextUtils.h"
#include "mozilla/gfx/2D.h"
#include "gfx2DGlue.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

TemporaryRef<gfx::DataSourceSurface>
ReadBackSurface(GLContext* aContext, GLuint aTexture, bool aYInvert, SurfaceFormat aFormat)
{
    nsRefPtr<gfxImageSurface> image = aContext->GetTexImage(aTexture, aYInvert, aFormat);
    RefPtr<gfx::DataSourceSurface> surf =
        Factory::CreateDataSourceSurface(gfx::ToIntSize(image->GetSize()), aFormat);

    if (!image->CopyTo(surf)) {
        return nullptr;
    }

    return surf.forget();
}

MainThreadSyncObject::MainThreadSyncObject(GLContext *cx)
    : mMainThreadGLContext(cx),
      mMutex("GLMainThreadSyncObject"),
      mSync(nullptr)
{
}

class MainThreadSyncObjectDeleter MOZ_FINAL : public nsRunnable
{
public:
    MainThreadSyncObjectDeleter(GLContext *cx, void *aSync) : mContext(cx), mSync(aSync) {}
    NS_IMETHOD Run() {
        if (!mContext->IsDestroyed())
            return NS_OK;
        mContext->MakeCurrent();
        mContext->fDeleteSync(static_cast<GLsync>(mSync));
        return NS_OK;
    }
    nsRefPtr<GLContext> mContext;
    void *mSync;
};

MainThreadSyncObject::~MainThreadSyncObject()
{
    if (!mSync)
        return;

    if (NS_IsMainThread()) {
        if (!mMainThreadGLContext->IsDestroyed())
            return;
        mMainThreadGLContext->MakeCurrent();
        mMainThreadGLContext->fDeleteSync(static_cast<GLsync>(mSync));
    } else {
        nsCOMPtr<nsIRunnable> runnable = new MainThreadSyncObjectDeleter(mMainThreadGLContext, mSync);
        NS_DispatchToMainThread(runnable);
    }
    mSync = nullptr;
}

void *
MainThreadSyncObject::AtomicExchangeSync(void *aNewSync)
{
    // uh, is there some kind of InterlockedExchange we could use here?
    void *oldSync;

    mMutex.Lock();

    oldSync = mSync;
    mSync = aNewSync;

    mMutex.Unlock();

    return oldSync;
}
    
bool
MainThreadSyncObject::FenceSync()
{
    MOZ_ASSERT(NS_IsMainThread());

    mMainThreadGLContext->MakeCurrent();
    void *newSync = mMainThreadGLContext->fFenceSync(LOCAL_GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    void *oldSync = AtomicExchangeSync(newSync);
    if (oldSync) {
        mMainThreadGLContext->fDeleteSync(static_cast<GLsync>(oldSync));
    }

    return true;
}

bool
MainThreadSyncObject::WaitSync(GLContext *waitCx)
{
    waitCx->MakeCurrent();

    {
        mozilla::MutexAutoLock lock(mMutex);
        if (!mSync)
            return true;

        waitCx->fWaitSync(static_cast<GLsync>(mSync), 0, LOCAL_GL_TIMEOUT_IGNORED);
    }

    return true;
}

bool
MainThreadSyncObject::ClientWaitSync(GLContext *waitCx, uint64_t timeout)
{
    waitCx->MakeCurrent();

    GLenum res;
    {
        mozilla::MutexAutoLock lock(mMutex);
        if (!mSync)
            return true;

        res = waitCx->fClientWaitSync(static_cast<GLsync>(mSync), 0, timeout);
    }

    return res == LOCAL_GL_ALREADY_SIGNALED || res == LOCAL_GL_CONDITION_SATISFIED;
}

} /* namespace gl */
} /* namespace mozilla */
