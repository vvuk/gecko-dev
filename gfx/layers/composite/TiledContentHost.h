/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_TILEDCONTENTHOST_H
#define GFX_TILEDCONTENTHOST_H

#include <stdint.h>                     // for uint16_t
#include <stdio.h>                      // for FILE
#include <algorithm>                    // for swap
#include "ContentHost.h"                // for ContentHost
#include "TiledLayerBuffer.h"           // for TiledLayerBuffer, etc
#include "CompositableHost.h"
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for Filter
#include "mozilla/layers/CompositorTypes.h"  // for TextureInfo, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/LayersTypes.h"  // for LayerRenderState, etc
#include "mozilla/layers/TextureHost.h"  // for DeprecatedTextureHost
#include "mozilla/layers/TiledContentClient.h"
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsRegion.h"                   // for nsIntRegion
#include "nscore.h"                     // for nsACString

class gfxReusableSurfaceWrapper;
struct nsIntPoint;
struct nsIntRect;
struct nsIntSize;

namespace mozilla {
namespace gfx {
class Matrix4x4;
}

namespace layers {

class Compositor;
class ISurfaceAllocator;
class Layer;
class ThebesBufferData;
class TiledThebesLayerComposite;
struct EffectChain;


class TileHost {
public:
  // Constructs a placeholder TileHost. See the comments above
  // TiledLayerBuffer for more information on what this is used for;
  // essentially, this is a sentinel used to represent an invalid or blank
  // tile.
  TileHost()
    : mSharedLock(nullptr)
    , mTextureHost(nullptr)
  {}

  // Constructs a TileHost from a gfxSharedReadLock and TextureHost.
  TileHost(gfxSharedReadLock* aSharedLock,
               TextureHost* aTextureHost)
    : mSharedLock(aSharedLock)
    , mTextureHost(aTextureHost)
  {}

  TileHost(const TileHost& o) {
    mTextureHost = o.mTextureHost;
    mSharedLock = o.mSharedLock;
  }
  TileHost& operator=(const TileHost& o) {
    if (this == &o) {
      return *this;
    }
    mTextureHost = o.mTextureHost;
    mSharedLock = o.mSharedLock;
    return *this;
  }

  bool operator== (const TileHost& o) const {
    return mTextureHost == o.mTextureHost;
  }
  bool operator!= (const TileHost& o) const {
    return mTextureHost != o.mTextureHost;
  }

  bool IsPlaceholderTile() const { return mTextureHost == nullptr; }

  void ReadUnlock() {
    if (IsPlaceholderTile()) {
      return;
    }
    NS_ASSERTION(mSharedLock != nullptr, "ReadUnlock with no gfxSharedReadLock");
    if (mSharedLock) {
      mSharedLock->ReadUnlock();
    }
  }

  RefPtr<gfxSharedReadLock> mSharedLock;
  RefPtr<TextureHost> mTextureHost;
};

class TiledLayerBufferComposite
  : public TiledLayerBuffer<TiledLayerBufferComposite, TileHost>
{
  friend class TiledLayerBuffer<TiledLayerBufferComposite, TileHost>;

public:
  typedef TiledLayerBuffer<TiledLayerBufferComposite, TileHost>::Iterator Iterator;

  TiledLayerBufferComposite();
  TiledLayerBufferComposite(ISurfaceAllocator* aAllocator,
                            const SurfaceDescriptorTiles& aDescriptor,
                            const nsIntRegion& aOldPaintedRegion);

  TileHost GetPlaceholderTile() const { return TileHost(); }

  // Stores the absolute resolution of the containing frame, calculated
  // by the sum of the resolutions of all parent layers' FrameMetrics.
  const CSSToScreenScale& GetFrameResolution() { return mFrameResolution; }

  void ReadUnlock();

  void Upload();

  void SetCompositor(Compositor* aCompositor);

  bool HasDoubleBufferedTiles() { return mHasDoubleBufferedTiles; }

  bool IsValid() const { return !mUninitialized; }

protected:
  TileHost ValidateTile(TileHost aTile,
                        const nsIntPoint& aTileRect,
                        const nsIntRegion& dirtyRect);

  // do nothing, the desctructor in the texture host takes care of releasing resources
  void ReleaseTile(TileHost aTile) {}

  void SwapTiles(TileHost& aTileA, TileHost& aTileB) { std::swap(aTileA, aTileB); }

private:
  CSSToScreenScale mFrameResolution;
  bool mHasDoubleBufferedTiles;
  bool mUninitialized;
};

/**
 * XXX Sort out this comment.
 * ContentHost for tiled Thebes layers. Since tiled layers are special snow
 * flakes, we don't call UpdateThebes or AddTextureHost, etc. We do call Composite
 * in the usual way though.
 *
 * There is no corresponding content client - on the client side we use a
 * ClientTiledLayerBuffer owned by a BasicTiledThebesLayer. On the host side, we
 * just use a regular ThebesLayerComposite, but with a tiled content host.
 *
 * TiledContentHost has a TiledLayerBufferComposite which keeps hold of the tiles.
 * Each tile has a reference to a texture host. During the layers transaction, we
 * receive a copy of the client-side tile buffer (UseTiledLayerBuffer). This is
 * copied into the main memory tile buffer and then deleted. Copying copies tiles,
 * but we only copy references to the underlying texture clients.
 *
 * When the content host is composited, we first upload any pending tiles
 * (Process*UploadQueue), then render (RenderLayerBuffer). The former calls Validate
 * on the tile (via ValidateTile and Update), that calls Update on the texture host,
 * which works as for regular texture hosts. Rendering takes us to RenderTile which
 * is similar to Composite for non-tiled ContentHosts.
 */
class TiledContentHost : public ContentHost,
                         public TiledLayerComposer
{
public:
  TiledContentHost(const TextureInfo& aTextureInfo);

  ~TiledContentHost();

  virtual LayerRenderState GetRenderState() MOZ_OVERRIDE
  {
    return LayerRenderState();
  }


  virtual bool UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack,
                            nsIntRegion* aUpdatedRegionBack)
  {
    NS_ERROR("N/A for tiled layers");
    return false;
  }

  const nsIntRegion& GetValidLowPrecisionRegion() const
  {
    return mLowPrecisionTiledBuffer.GetValidRegion();
  }

  void UseTiledLayerBuffer(ISurfaceAllocator* aAllocator,
                           const SurfaceDescriptorTiles& aTiledDescriptor);

  // Renders a single given tile.
  void RenderTile(const TileHost& aTile,
                  EffectChain& aEffectChain,
                  float aOpacity,
                  const gfx::Matrix4x4& aTransform,
                  const gfx::Filter& aFilter,
                  const gfx::Rect& aClipRect,
                  const nsIntRegion& aScreenRegion,
                  const nsIntPoint& aTextureOffset,
                  const nsIntSize& aTextureBounds);

  void Composite(EffectChain& aEffectChain,
                 float aOpacity,
                 const gfx::Matrix4x4& aTransform,
                 const gfx::Filter& aFilter,
                 const gfx::Rect& aClipRect,
                 const nsIntRegion* aVisibleRegion = nullptr,
                 TiledLayerProperties* aLayerProperties = nullptr);

  virtual CompositableType GetType() { return BUFFER_TILED; }

  virtual TiledLayerComposer* AsTiledLayerComposer() MOZ_OVERRIDE { return this; }

  virtual void EnsureDeprecatedTextureHost(TextureIdentifier aTextureId,
                                 const SurfaceDescriptor& aSurface,
                                 ISurfaceAllocator* aAllocator,
                                 const TextureInfo& aTextureInfo) MOZ_OVERRIDE
  {
    MOZ_CRASH("Does nothing");
  }

  virtual void Attach(Layer* aLayer,
                      Compositor* aCompositor,
                      AttachFlags aFlags = NO_FLAGS) MOZ_OVERRIDE;

#ifdef MOZ_DUMP_PAINTING
  virtual void Dump(FILE* aFile=nullptr,
                    const char* aPrefix="",
                    bool aDumpHtml=false) MOZ_OVERRIDE;
#endif

  virtual void PrintInfo(nsACString& aTo, const char* aPrefix);

private:
  void RenderLayerBuffer(TiledLayerBufferComposite& aLayerBuffer,
                         const nsIntRegion& aValidRegion,
                         EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion& aMaskRegion,
                         nsIntRect aVisibleRect,
                         gfx::Matrix4x4 aTransform);

  void EnsureTileStore() {}

  TiledLayerBufferComposite    mTiledBuffer;
  TiledLayerBufferComposite    mLowPrecisionTiledBuffer;
  TiledLayerBufferComposite    mOldTiledBuffer;
  TiledLayerBufferComposite    mOldLowPrecisionTiledBuffer;
  bool                         mPendingUpload : 1;
  bool                         mPendingLowPrecisionUpload : 1;
};

}
}

#endif
