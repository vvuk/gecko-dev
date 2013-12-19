/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXTUTILS_H_
#define GLCONTEXTUTILS_H_

#include "GLContextTypes.h"
#include "GLContext.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/Mutex.h"

namespace mozilla {
namespace gfx {
  class DataSourceSurface;
}
}

namespace mozilla {
namespace gl {

TemporaryRef<gfx::DataSourceSurface>
ReadBackSurface(GLContext* aContext, GLuint aTexture, bool aYInvert, gfx::SurfaceFormat aFormat);

// A sync object that's only created and deleted on the main thread.  It can be waited
// on on any thread.
class MainThreadSyncObject : public AtomicRefCounted<MainThreadSyncObject>
{
public:
    // context for creation/deletion
    MainThreadSyncObject(GLContext *cx = nullptr);
    void SetMainThreadContext(GLContext *cx) {
        mMainThreadGLContext = cx;
    }

    virtual ~MainThreadSyncObject();

    bool FenceSync();

    // Waits on a sync object.  Note that this MainThreadSyncObject can't be reused/reset
    // until the wait is finished (FenceSync will block).  Returns true if signalled,
    // false if timeout or something else.
    bool WaitSync(GLContext *waitCx);
    bool ClientWaitSync(GLContext *waitCx, uint64_t timeout = LOCAL_GL_TIMEOUT_IGNORED);

protected:
    void *AtomicExchangeSync(void *newSync);
    nsRefPtr<GLContext> mMainThreadGLContext;
    Mutex mMutex;
    void *mSync;
};

} // namespace gl
} // namespace mozilla

#endif /* GLCONTEXTUTILS_H_ */
