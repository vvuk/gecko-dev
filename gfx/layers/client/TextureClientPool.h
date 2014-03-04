/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENTPOOL_H
#define MOZILLA_GFX_TEXTURECLIENTPOOL_H

#include "mozilla/gfx/Types.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/RefPtr.h"
#include "TextureClient.h"
#include "nsITimer.h"
#include <stack>

namespace mozilla {
namespace layers {

class ISurfaceAllocator;

class TextureClientPool : public RefCounted<TextureClientPool>
{
public:
  TextureClientPool(gfx::SurfaceFormat aFormat, gfx::IntSize aSize,
                    ISurfaceAllocator *aAllocator);

  TemporaryRef<TextureClient> GetTextureClient();

  void ReturnTextureClient(TextureClient *aClient);
  void ReturnTextureClientDeferred(TextureClient *aClient);

  void ShrinkToMaximumSize();
  void ShrinkToMinimumSize();
  void ReturnDeferredClients();
  // This means a texture client that was taken from the pool will never be
  // reused again.
  void ReportClientLost() { mOutstandingClients--; }

  void Clear();

private:
  // The time in milliseconds before the pool will be shrunk to the minimum
  // size after returning a client.
  static const uint32_t sShrinkTimeout = 1000;

  // The minimum size of the pool (the number of tiles that will be kept after
  // shrinking).
  static const uint32_t sMinCacheSize = 0;

  // This is the number of texture clients we don't want to exceed, including
  // outstanding TextureClients (from GetTextureClient()), cached
  // TextureClients and deferred-return TextureClients.
  static const uint32_t sMaxTextureClients = 50;

  gfx::SurfaceFormat mFormat;
  gfx::IntSize mSize;

  uint32_t mOutstandingClients;

  std::stack<RefPtr<TextureClient> > mTextureClients;
  std::stack<RefPtr<TextureClient> > mTextureClientsDeferred;
  nsRefPtr<nsITimer> mTimer;
  RefPtr<ISurfaceAllocator> mSurfaceAllocator;
};

}
}
#endif /* MOZILLA_GFX_TEXTURECLIENTPOOL_H */
