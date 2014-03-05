/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SimpleTextureClientPool.h"
#include "CompositableClient.h"
#include "mozilla/layers/ISurfaceAllocator.h"

#include "gfxPlatform.h"

#include "nsComponentManagerUtils.h"

#if 0
#define RECYCLE_LOG(...) printf_stderr(__VA_ARGS__)
#else
#define RECYCLE_LOG(...) do { } while (0)
#endif

namespace mozilla {
namespace layers {

using gfx::SurfaceFormat;

static void
ShrinkCallback(nsITimer *aTimer, void *aClosure)
{
  static_cast<SimpleTextureClientPool*>(aClosure)->ShrinkToMinimumSize();
}

static void
RecycleCallback(TextureClient* aClient, void* aClosure) {
  SimpleTextureClientPool* pool =
    reinterpret_cast<SimpleTextureClientPool*>(aClosure);

  aClient->ClearRecycleCallback();
  pool->ReturnTextureClient(aClient);
}

static void
WaitForCompositorRecycleCallback(TextureClient* aClient, void* aClosure) {
  // This will grab a reference that will be released once the compositor
  // acknowledges the remote recycle. Once it is received the object
  // will be fully recycled.
  aClient->WaitForCompositorRecycle();
  aClient->SetRecycleCallback(RecycleCallback, aClosure);
}

SimpleTextureClientPool::SimpleTextureClientPool(gfx::SurfaceFormat aFormat, gfx::IntSize aSize,
                                                 ISurfaceAllocator *aAllocator)
  : mFormat(aFormat)
  , mSize(aSize)
  , mSurfaceAllocator(aAllocator)
{
  mTimer = do_CreateInstance("@mozilla.org/timer;1");
}

TemporaryRef<TextureClient>
SimpleTextureClientPool::GetTextureClient(bool aAutoRecycle)
{
  // Try to fetch a client from the pool
  RefPtr<TextureClient> textureClient;
  if (mTextureClients.size()) {
    textureClient = mTextureClients.top();
    mTextureClients.pop();
    RECYCLE_LOG("%s Skip allocate (%i left), returning %p\n", (mFormat == SurfaceFormat::B8G8R8A8?"poolA":"poolX"), mTextureClients.size(), textureClient.g

  } else {
    // No unused clients in the pool, create one
    if (gfxPlatform::GetPlatform()->GetPrefLayersForceShmemTiles()) {
      textureClient = TextureClient::CreateBufferTextureClient(mSurfaceAllocator, mFormat, TEXTURE_IMMEDIATE_UPLOAD | TEXTURE_RECYCLE);
    } else {
      textureClient = TextureClient::CreateTextureClientForDrawing(mSurfaceAllocator, mFormat, TEXTURE_FLAGS_DEFAULT | TEXTURE_RECYCLE);
    }
    if (!textureClient->AsTextureClientDrawTarget()->AllocateForSurface(mSize, ALLOC_DEFAULT)) {
      NS_WARNING("TextureClient::AllocateForSurface failed!");
    }
    RECYCLE_LOG("%s Must allocate (0 left), returning %p\n", (mFormat == SurfaceFormat::B8G8R8A8?"poolA":"poolX"), textureClient.get());
  }

  if (aAutoRecycle) {
    mAutoRecycle.push_back(textureClient);
    textureClient->SetRecycleCallback(WaitForCompositorRecycleCallback, this);
  }

  return textureClient;
}

void
SimpleTextureClientPool::ReturnTextureClient(TextureClient *aClient)
{
  if (!aClient) {
    return;
  }

  // If we haven't hit our max cached client limit, add this one
  if (mTextureClients.size() < sMaxTextureClients) {
    mTextureClients.push(aClient);
    RECYCLE_LOG("%s recycled %p (have %d)\n", (mFormat == SurfaceFormat::B8G8R8A8?"poolA":"poolX"), aClient, mTextureClients.size());
  } else {
    RECYCLE_LOG("%s did not recycle %p (have %d)\n", (mFormat == SurfaceFormat::B8G8R8A8?"poolA":"poolX"), aClient, mTextureClients.size());
  }

  // Kick off the pool shrinking timer if there are still more unused texture
  // clients than our desired minimum cache size.
  if (mTextureClients.size() > sMinCacheSize) {
    mTimer->InitWithFuncCallback(ShrinkCallback, this, sShrinkTimeout,
                                 nsITimer::TYPE_ONE_SHOT);
  }

  mAutoRecycle.remove(aClient);
}

void
SimpleTextureClientPool::ShrinkToMinimumSize()
{
  RECYCLE_LOG("%s ShrinkToMinimumSize, removing %d clients", (mFormat == SurfaceFormat::B8G8R8A8?"poolA":"poolX"), mTextureClients.size() > sMinCacheSize ? mTextureClients.size() - sMinCacheSize : 0);

  mTimer->Cancel();

  while (mTextureClients.size() > sMinCacheSize) {
    mTextureClients.pop();
  }
}

void
SimpleTextureClientPool::Clear()
{
  while (!mTextureClients.empty()) {
    mTextureClients.pop();
  }
}

}
}
