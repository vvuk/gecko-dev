/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLCONTEXTUTILS_H_
#define WEBGLCONTEXTUTILS_H_

#include "WebGLContext.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla {

bool IsGLDepthFormat(GLenum internalFormat);
bool IsGLDepthStencilFormat(GLenum internalFormat);

template <typename WebGLObjectType>
JS::Value
WebGLContext::WebGLObjectAsJSValue(JSContext *cx, const WebGLObjectType *object, ErrorResult& rv) const
{
    if (!object) {
        return JS::NullValue();
    }
    MOZ_ASSERT(this == object->Context());
    JS::Rooted<JS::Value> v(cx);
    JS::Rooted<JSObject*> wrapper(cx, GetWrapper());
    JSAutoCompartment ac(cx, wrapper);
    if (!dom::WrapNewBindingObject(cx, wrapper, const_cast<WebGLObjectType*>(object), &v)) {
        rv.Throw(NS_ERROR_FAILURE);
        return JS::NullValue();
    }
    return v;
}

template <typename WebGLObjectType>
JSObject*
WebGLContext::WebGLObjectAsJSObject(JSContext *cx, const WebGLObjectType *object, ErrorResult& rv) const
{
    JS::Value v = WebGLObjectAsJSValue(cx, object, rv);
    if (v.isNull()) {
        return nullptr;
    }
    return &v.toObject();
}

bool
WebGLContext::CheckArrayBufferStartAndLength(const char *func, const ArrayBuffer& data, WebGLintptr start, WebGLsizeiptr length)
{
    if (start < 0) {
        ErrorInvalidValue("%s: start cannot be less than 0", func);
        return false;
    }

    CheckedInt<WebGLsizeiptr> checked_neededByteLength = CheckedInt<WebGLsizeiptr>(start) + length;
    if (!checked_neededByteLength.isValid()) {
        ErrorInvalidValue("%s: integer overflow computing the needed byte length", func);
        return false;
    }

    if (checked_neededByteLength.value() > data.Length()) {
        ErrorInvalidValue("%s: not enough data - requested start and length requires %d bytes, but buffer only has %d bytes",
                          func, checked_neededByteLength.value(), data.Length());
        return false;
    }

    return true;
}

} // namespace mozilla

#endif // WEBGLCONTEXTUTILS_H_
