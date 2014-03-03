/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClientPool.h"
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

static void
ShrinkCallback(nsITimer *aTimer, void *aClosure)
{
  static_cast<TextureClientPool*>(aClosure)->ShrinkToMinimumSize();
}

TextureClientPool::TextureClientPool(gfx::SurfaceFormat aFormat, gfx::IntSize aSize,
                                     ISurfaceAllocator *aAllocator)
  : mFormat(aFormat)
  , mSize(aSize)
  , mOutstandingClients(0)
  , mSurfaceAllocator(aAllocator)
{
  mTimer = do_CreateInstance("@mozilla.org/timer;1");
}

TemporaryRef<TextureClient>
TextureClientPool::GetTextureClient()
{
  mOutstandingClients++;

  // Try to fetch a client from the pool
  RefPtr<TextureClient> textureClient;
  if (mTextureClients.size()) {
    textureClient = mTextureClients.top();
    mTextureClients.pop();
    RECYCLE_LOG("Skip allocate (%i left)\n", mTextureClients.size());
    return textureClient;
  }

  // We're increasing the number of outstanding TextureClients without reusing a
  // client, we may need to free a deferred-return TextureClient.
  ShrinkToMaximumSize();

  // No unused clients in the pool, create one
  RECYCLE_LOG("Have to allocate (0 left)\n");
  if (gfxPlatform::GetPlatform()->GetPrefLayersForceShmemTiles()) {
    textureClient = TextureClient::CreateBufferTextureClient(mSurfaceAllocator, mFormat, TEXTURE_IMMEDIATE_UPLOAD | TEXTURE_RECYCLE);
  } else {
    textureClient = TextureClient::CreateTextureClientForDrawing(mSurfaceAllocator, mFormat, TEXTURE_FLAGS_DEFAULT | TEXTURE_RECYCLE);
  }
  textureClient->AsTextureClientDrawTarget()->AllocateForSurface(mSize, ALLOC_DEFAULT);
  RECYCLE_LOG("CAN-TILE: 1. Allocate %p\n", textureClient.get());

  return textureClient;
}

static void
RecycleCallback(TextureClient* aClient, void* aClosure) {
  TextureClientPool* pool =
    reinterpret_cast<TextureClientPool*>(aClosure);

  aClient->ClearRecycleCallback();
  pool->ReturnTextureClient(aClient);
}

static void
WaitForCompositorRecycleCallback(TextureClient* aClient, void* aClosure) {
  TextureClientPool* pool =
    reinterpret_cast<TextureClientPool*>(aClosure);

  // This will grab a reference that will be released once the compositor
  // acknowledges the remote recycle. Once it is received the object
  // will be fully recycled.
  aClient->WaitForCompositorRecycle();
  aClient->SetRecycleCallback(RecycleCallback, aClosure);
}

void
TextureClientPool::AutoRecycle(TextureClient *aClient)
{
  mAutoRecycle.push_back(aClient);
  aClient->SetRecycleCallback(WaitForCompositorRecycleCallback, this);
}

void
TextureClientPool::ReturnTextureClient(TextureClient *aClient)
{
  if (!aClient) {
    return;
  }
  MOZ_ASSERT(mOutstandingClients);
  mOutstandingClients--;

  // Add the client to the pool and shrink down if we're beyond our maximum size
  mTextureClients.push(aClient);
  ShrinkToMaximumSize();
  RECYCLE_LOG("recycled and shrunk (%i left)\n", mTextureClients.size());

  // Kick off the pool shrinking timer if there are still more unused texture
  // clients than our desired minimum cache size.
  if (mTextureClients.size() > sMinCacheSize) {
    mTimer->InitWithFuncCallback(ShrinkCallback, this, sShrinkTimeout,
                                 nsITimer::TYPE_ONE_SHOT);
  }

  RECYCLE_LOG("CAN-TILE: 4. Recycle %p\n", aClient);
  mAutoRecycle.remove(aClient);
}

void
TextureClientPool::ReturnTextureClientDeferred(TextureClient *aClient)
{
  mTextureClientsDeferred.push(aClient);
  ShrinkToMaximumSize();
}

void
TextureClientPool::ShrinkToMaximumSize()
{
  uint32_t totalClientsOutstanding =
    mTextureClients.size() + mTextureClientsDeferred.size() + mOutstandingClients;

  // We're over our desired maximum size, immediately shrink down to the
  // maximum, or zero if we have too many outstanding texture clients.
  // We cull from the deferred TextureClients first, as we can't reuse those
  // until they get returned.
  while (totalClientsOutstanding > sMaxTextureClients) {
    if (mTextureClientsDeferred.size()) {
      mOutstandingClients--;
      mTextureClientsDeferred.pop();
    } else {
      if (!mTextureClients.size()) {
        // Getting here means we're over our desired number of TextureClients
        // with none in the pool. This can happen for pathological cases, or
        // it could mean that sMaxTextureClients needs adjusting for whatever
        // device we're running on.
        break;
      }
      mTextureClients.pop();
    }
    totalClientsOutstanding--;
  }
}

void
TextureClientPool::ShrinkToMinimumSize()
{
  RECYCLE_LOG("TextureClientPool: ShrinkToMinimumSize, removing %d clients", mTextureClients.size() - sMinimumCacheSize);
  while (mTextureClients.size() > sMinimumCacheSize) {
    mTextureClients.pop();
  }
}

void
TextureClientPool::ReturnDeferredClients()
{
  while (!mTextureClientsDeferred.empty()) {
    ReturnTextureClient(mTextureClientsDeferred.top());
    mTextureClientsDeferred.pop();
  }
}

void
TextureClientPool::Clear()
{
  while (!mTextureClients.empty()) {
    mTextureClients.pop();
  }
  while (!mTextureClientsDeferred.empty()) {
    mOutstandingClients--;
    mTextureClientsDeferred.pop();
  }
}

}
}
