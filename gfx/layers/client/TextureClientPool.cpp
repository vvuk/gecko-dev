/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClientPool.h"
#include "CompositableClient.h"
#include "mozilla/layers/ISurfaceAllocator.h"

#include "gfxPlatform.h"

#include "nsComponentManagerUtils.h"

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
  , mPendingShrink(false)
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
    return textureClient;
  }

  // No unused clients in the pool, create one
  if (gfxPlatform::GetPlatform()->GetPrefLayersForceShmemTiles()) {
    textureClient = TextureClient::CreateBufferTextureClient(mSurfaceAllocator, mFormat, TEXTURE_IMMEDIATE_UPLOAD);
  } else {
    textureClient = TextureClient::CreateTextureClientForDrawing(mSurfaceAllocator, mFormat, TEXTURE_IMMEDIATE_UPLOAD);
  }
  textureClient->AsTextureClientDrawTarget()->AllocateForSurface(mSize, ALLOC_DEFAULT);

  return textureClient;
}

void
TextureClientPool::ReturnTextureClient(TextureClient *aClient)
{
  if (!aClient) {
    return;
  }
  MOZ_ASSERT(mOutstandingClients);
  mOutstandingClients--;

  // Add the client to the pool if the pool is below the maximum size
  if (mTextureClients.size() + mOutstandingClients < sMaximumCacheSize) {
    mTextureClients.push(aClient);

    // Kick off the pool shrinking timer
    if (!mPendingShrink) {
      mTimer->InitWithFuncCallback(ShrinkCallback, this, sShrinkTimeout,
                                   nsITimer::TYPE_ONE_SHOT);
      mPendingShrink = true;
    }
  }
}

void
TextureClientPool::ReturnTextureClientDeferred(TextureClient *aClient)
{
  mTextureClientsDeferred.push(aClient);
}

void
TextureClientPool::ShrinkToMinimumSize()
{
  while (mTextureClients.size() > sMinimumCacheSize) {
    mTextureClients.pop();
  }

  mTimer->Cancel();
  mPendingShrink = false;
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
