/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLExtensions.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "GLContext.h"

using namespace mozilla;

WebGLExtensionArrayBufferData::WebGLExtensionArrayBufferData(WebGLContext* context)
  : WebGLExtensionBase(context)
{
}

WebGLExtensionArrayBufferData::~WebGLExtensionArrayBufferData()
{
}

void
WebGLExtensionArrayBufferData::BufferDataWEBGL(GLenum target,
                                               const dom::ArrayBuffer &data,
                                               WebGLintptr start, WebGLsizeiptr size,
                                               GLenum usage)
{
    mContext->BufferData(target, data, start, size, usage);
}

void
WebGLExtensionArrayBufferData::BufferSubDataWEBGL(GLenum target,
                                                  WebGLintptr offset,
                                                  const dom::ArrayBuffer &data,
                                                  WebGLintptr start, WebGLsizeiptr size)
{
    mContext->BufferSubData(target, offset, data, start, size);
}


void
WebGLExtensionArrayBufferData::CompressedTexImage2DWEBGL(GLenum target, GLint level, GLenum internalformat,
                                                         GLsizei width, GLsizei height, GLint border,
                                                         const dom::ArrayBuffer &data, WebGLintptr start, WebGLsizeiptr length)
{
    mContext->CompressedTexImage2D(target, level, internalformat,
                                   width, height, border,
                                   data, start, length);
}

void
WebGLExtensionArrayBufferData::CompressedTexSubImage2DWEBGL(GLenum target, GLint level,
                                                            GLint xoffset, GLint yoffset,
                                                            GLsizei width, GLsizei height, GLenum format,
                                                            const dom::ArrayBuffer &data, WebGLintptr start, WebGLsizeiptr length,
                                                            ErrorResult& rv)
{
    mContext->CompressedTexSubImage2D(target, level, xoffset, yoffset,
                                      width, height, format,
                                      data, start, length,
                                      rv);
}


void
WebGLExtensionArrayBufferData::readPixelsWEBGL(GLint x, GLint y, GLsizei width, GLsizei height, 
                                               GLenum format, GLenum type,
                                               const dom::ArrayBuffer &data, WebGLintptr start, WebGLsizeiptr length,
                                               ErrorResult& rv)
{
    mContext->ReadPixels(x, y, width, height, format, type, data, start, length, rv);
}

void
WebGLExtensionArrayBufferData::texImage2DWEBGL(GLenum target, GLint level, GLenum internalformat, 
                                               GLsizei width, GLsizei height, GLint border, GLenum format, 
                                               GLenum type,
                                               const dom::ArrayBuffer &data, WebGLintptr start, WebGLsizeiptr length,
                                               ErrorResult& rv)
{
    mContext->TexImage2D(target, level, internalformat, width, height, border, format, type, data, start, length, rv);
}

void
WebGLExtensionArrayBufferData::texSubImage2DWEBGL(GLenum target, GLint level, GLint xoffset, GLint yoffset, 
                                                  GLsizei width, GLsizei height, 
                                                  GLenum format, GLenum type,
                                                  const dom::ArrayBuffer &data, WebGLintptr start, WebGLsizeiptr length,
                                                  ErrorResult& rv)
{
    mContext->TexSubImage2D(target, leve, xoffset, yoffset, width, height, format, type, data, start, length, rv);
}
