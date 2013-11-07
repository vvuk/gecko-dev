/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RefPtr.h"
#include "skia/GrGLInterface.h"
#include "skia/GrContext.h"

GrGLInterface* CreateGrGLInterfaceFromGLContext(mozilla::gl::GLContext* context);

namespace mozilla {
namespace gl {

class GLContextSkia : public GenericAtomicRefCounted
{
public:
    GLContextSkia(GLContext* context)
        : mGLContext(context)
    {
        SkAutoTUnref<GrGLInterface> i(CreateGrGLInterfaceFromGLContext(mGLContext));
        i->fCallbackData = reinterpret_cast<GrGLInterfaceCallbackData>(this);
        mGrGLInterface = i;
        SkAutoTUnref<GrContext> gr(GrContext::Create(kOpenGL_GrBackend, (GrBackendContext)mGrGLInterface.get()));

        mGrContext = gr;
    }

    GLContext* GetGLContext() const { return mGLContext.get(); }
    GrContext* GetGrContext() const { return mGrContext.get(); }
private:
    RefPtr<GLContext> mGLContext;
    SkRefPtr<GrGLInterface> mGrGLInterface;
    SkRefPtr<GrContext> mGrContext;
};

}
}
