/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "gfxUtils.h"
#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxDrawable.h"
#include "nsRegion.h"
#include "yuv_convert.h"
#include "ycbcr_to_rgb565.h"

#ifdef XP_WIN
#include "gfxWindowsPlatform.h"
#endif

using namespace mozilla;
using namespace mozilla::layers;
using namespace mozilla::gfx;

static PRUint8 sUnpremultiplyTable[256*256];
static PRUint8 sPremultiplyTable[256*256];
static bool sTablesInitialized = false;

static const PRUint8 PremultiplyValue(PRUint8 a, PRUint8 v) {
    return sPremultiplyTable[a*256+v];
}

static const PRUint8 UnpremultiplyValue(PRUint8 a, PRUint8 v) {
    return sUnpremultiplyTable[a*256+v];
}

static void
CalculateTables()
{
    // It's important that the array be indexed first by alpha and then by rgb
    // value.  When we unpremultiply a pixel, we're guaranteed to do three
    // lookups with the same alpha; indexing by alpha first makes it likely that
    // those three lookups will be close to one another in memory, thus
    // increasing the chance of a cache hit.

    // Unpremultiply table

    // a == 0 case
    for (PRUint32 c = 0; c <= 255; c++) {
        sUnpremultiplyTable[c] = c;
    }

    for (int a = 1; a <= 255; a++) {
        for (int c = 0; c <= 255; c++) {
            sUnpremultiplyTable[a*256+c] = (PRUint8)((c * 255) / a);
        }
    }

    // Premultiply table

    for (int a = 0; a <= 255; a++) {
        for (int c = 0; c <= 255; c++) {
            sPremultiplyTable[a*256+c] = (a * c + 254) / 255;
        }
    }

    sTablesInitialized = true;
}

void
gfxUtils::PremultiplyImageSurface(gfxImageSurface *aSourceSurface,
                                  gfxImageSurface *aDestSurface)
{
    if (!aDestSurface)
        aDestSurface = aSourceSurface;

    NS_ASSERTION(aSourceSurface->Format() == aDestSurface->Format() &&
                 aSourceSurface->Width() == aDestSurface->Width() &&
                 aSourceSurface->Height() == aDestSurface->Height() &&
                 aSourceSurface->Stride() == aDestSurface->Stride(),
                 "Source and destination surfaces don't have identical characteristics");

    NS_ASSERTION(aSourceSurface->Stride() == aSourceSurface->Width() * 4,
                 "Source surface stride isn't tightly packed");

    // Only premultiply ARGB32
    if (aSourceSurface->Format() != gfxASurface::ImageFormatARGB32) {
        if (aDestSurface != aSourceSurface) {
            memcpy(aDestSurface->Data(), aSourceSurface->Data(),
                   aSourceSurface->Stride() * aSourceSurface->Height());
        }
        return;
    }

    if (!sTablesInitialized)
        CalculateTables();

    PRUint8 *src = aSourceSurface->Data();
    PRUint8 *dst = aDestSurface->Data();

    PRUint32 dim = aSourceSurface->Width() * aSourceSurface->Height();
    for (PRUint32 i = 0; i < dim; ++i) {
#ifdef IS_LITTLE_ENDIAN
        PRUint8 b = *src++;
        PRUint8 g = *src++;
        PRUint8 r = *src++;
        PRUint8 a = *src++;

        *dst++ = PremultiplyValue(a, b);
        *dst++ = PremultiplyValue(a, g);
        *dst++ = PremultiplyValue(a, r);
        *dst++ = a;
#else
        PRUint8 a = *src++;
        PRUint8 r = *src++;
        PRUint8 g = *src++;
        PRUint8 b = *src++;

        *dst++ = a;
        *dst++ = PremultiplyValue(a, r);
        *dst++ = PremultiplyValue(a, g);
        *dst++ = PremultiplyValue(a, b);
#endif
    }
}

void
gfxUtils::UnpremultiplyImageSurface(gfxImageSurface *aSourceSurface,
                                    gfxImageSurface *aDestSurface)
{
    if (!aDestSurface)
        aDestSurface = aSourceSurface;

    NS_ASSERTION(aSourceSurface->Format() == aDestSurface->Format() &&
                 aSourceSurface->Width() == aDestSurface->Width() &&
                 aSourceSurface->Height() == aDestSurface->Height() &&
                 aSourceSurface->Stride() == aDestSurface->Stride(),
                 "Source and destination surfaces don't have identical characteristics");

    NS_ASSERTION(aSourceSurface->Stride() == aSourceSurface->Width() * 4,
                 "Source surface stride isn't tightly packed");

    // Only premultiply ARGB32
    if (aSourceSurface->Format() != gfxASurface::ImageFormatARGB32) {
        if (aDestSurface != aSourceSurface) {
            memcpy(aDestSurface->Data(), aSourceSurface->Data(),
                   aSourceSurface->Stride() * aSourceSurface->Height());
        }
        return;
    }

    if (!sTablesInitialized)
        CalculateTables();

    PRUint8 *src = aSourceSurface->Data();
    PRUint8 *dst = aDestSurface->Data();

    PRUint32 dim = aSourceSurface->Width() * aSourceSurface->Height();
    for (PRUint32 i = 0; i < dim; ++i) {
#ifdef IS_LITTLE_ENDIAN
        PRUint8 b = *src++;
        PRUint8 g = *src++;
        PRUint8 r = *src++;
        PRUint8 a = *src++;

        *dst++ = UnpremultiplyValue(a, b);
        *dst++ = UnpremultiplyValue(a, g);
        *dst++ = UnpremultiplyValue(a, r);
        *dst++ = a;
#else
        PRUint8 a = *src++;
        PRUint8 r = *src++;
        PRUint8 g = *src++;
        PRUint8 b = *src++;

        *dst++ = a;
        *dst++ = UnpremultiplyValue(a, r);
        *dst++ = UnpremultiplyValue(a, g);
        *dst++ = UnpremultiplyValue(a, b);
#endif
    }
}

static bool
IsSafeImageTransformComponent(gfxFloat aValue)
{
  return aValue >= -32768 && aValue <= 32767;
}

/**
 * This returns the fastest operator to use for solid surfaces which have no
 * alpha channel or their alpha channel is uniformly opaque.
 * This differs per render mode.
 */
static gfxContext::GraphicsOperator
OptimalFillOperator()
{
#ifdef XP_WIN
    if (gfxWindowsPlatform::GetPlatform()->GetRenderMode() ==
        gfxWindowsPlatform::RENDER_DIRECT2D) {
        // D2D -really- hates operator source.
        return gfxContext::OPERATOR_OVER;
    } else {
#endif
        return gfxContext::OPERATOR_SOURCE;
#ifdef XP_WIN
    }
#endif
}

// EXTEND_PAD won't help us here; we have to create a temporary surface to hold
// the subimage of pixels we're allowed to sample.
static already_AddRefed<gfxDrawable>
CreateSamplingRestrictedDrawable(gfxDrawable* aDrawable,
                                 gfxContext* aContext,
                                 const gfxMatrix& aUserSpaceToImageSpace,
                                 const gfxRect& aSourceRect,
                                 const gfxRect& aSubimage,
                                 const gfxImageSurface::gfxImageFormat aFormat)
{
    gfxRect userSpaceClipExtents = aContext->GetClipExtents();
    // This isn't optimal --- if aContext has a rotation then GetClipExtents
    // will have to do a bounding-box computation, and TransformBounds might
    // too, so we could get a better result if we computed image space clip
    // extents in one go --- but it doesn't really matter and this is easier
    // to understand.
    gfxRect imageSpaceClipExtents =
        aUserSpaceToImageSpace.TransformBounds(userSpaceClipExtents);
    // Inflate by one pixel because bilinear filtering will sample at most
    // one pixel beyond the computed image pixel coordinate.
    imageSpaceClipExtents.Inflate(1.0);

    gfxRect needed = imageSpaceClipExtents.Intersect(aSourceRect);
    needed = needed.Intersect(aSubimage);
    needed.RoundOut();

    // if 'needed' is empty, nothing will be drawn since aFill
    // must be entirely outside the clip region, so it doesn't
    // matter what we do here, but we should avoid trying to
    // create a zero-size surface.
    if (needed.IsEmpty())
        return nsnull;

    gfxIntSize size(PRInt32(needed.Width()), PRInt32(needed.Height()));
    nsRefPtr<gfxASurface> temp =
        gfxPlatform::GetPlatform()->CreateOffscreenSurface(size, gfxASurface::ContentFromFormat(aFormat));
    if (!temp || temp->CairoStatus())
        return nsnull;

    nsRefPtr<gfxContext> tmpCtx = new gfxContext(temp);
    tmpCtx->SetOperator(OptimalFillOperator());
    aDrawable->Draw(tmpCtx, needed - needed.TopLeft(), true,
                    gfxPattern::FILTER_FAST, gfxMatrix().Translate(needed.TopLeft()));

    nsRefPtr<gfxPattern> resultPattern = new gfxPattern(temp);
    if (!resultPattern)
        return nsnull;

    nsRefPtr<gfxDrawable> drawable = 
        new gfxSurfaceDrawable(temp, size, gfxMatrix().Translate(-needed.TopLeft()));
    return drawable.forget();
}

// working around cairo/pixman bug (bug 364968)
// Our device-space-to-image-space transform may not be acceptable to pixman.
struct NS_STACK_CLASS AutoCairoPixmanBugWorkaround
{
    AutoCairoPixmanBugWorkaround(gfxContext*      aContext,
                                 const gfxMatrix& aDeviceSpaceToImageSpace,
                                 const gfxRect&   aFill,
                                 const gfxASurface::gfxSurfaceType& aSurfaceType)
     : mContext(aContext), mSucceeded(true), mPushedGroup(false)
    {
        // Quartz's limits for matrix are much larger than pixman
        if (aSurfaceType == gfxASurface::SurfaceTypeQuartz)
            return;

        if (!IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.xx) ||
            !IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.xy) ||
            !IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.yx) ||
            !IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.yy)) {
            NS_WARNING("Scaling up too much, bailing out");
            mSucceeded = false;
            return;
        }

        if (IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.x0) &&
            IsSafeImageTransformComponent(aDeviceSpaceToImageSpace.y0))
            return;

        // We'll push a group, which will hopefully reduce our transform's
        // translation so it's in bounds.
        gfxMatrix currentMatrix = mContext->CurrentMatrix();
        mContext->Save();

        // Clip the rounded-out-to-device-pixels bounds of the
        // transformed fill area. This is the area for the group we
        // want to push.
        mContext->IdentityMatrix();
        gfxRect bounds = currentMatrix.TransformBounds(aFill);
        bounds.RoundOut();
        mContext->Clip(bounds);
        mContext->SetMatrix(currentMatrix);
        mContext->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
        mContext->SetOperator(gfxContext::OPERATOR_OVER);

        mPushedGroup = true;
    }

    ~AutoCairoPixmanBugWorkaround()
    {
        if (mPushedGroup) {
            mContext->PopGroupToSource();
            mContext->Paint();
            mContext->Restore();
        }
    }

    bool PushedGroup() { return mPushedGroup; }
    bool Succeeded() { return mSucceeded; }

private:
    gfxContext* mContext;
    bool mSucceeded;
    bool mPushedGroup;
};

static gfxMatrix
DeviceToImageTransform(gfxContext* aContext,
                       const gfxMatrix& aUserSpaceToImageSpace)
{
    gfxFloat deviceX, deviceY;
    nsRefPtr<gfxASurface> currentTarget =
        aContext->CurrentSurface(&deviceX, &deviceY);
    gfxMatrix currentMatrix = aContext->CurrentMatrix();
    gfxMatrix deviceToUser = gfxMatrix(currentMatrix).Invert();
    deviceToUser.Translate(-gfxPoint(-deviceX, -deviceY));
    return gfxMatrix(deviceToUser).Multiply(aUserSpaceToImageSpace);
}

/* static */ void
gfxUtils::DrawPixelSnapped(gfxContext*      aContext,
                           gfxDrawable*     aDrawable,
                           const gfxMatrix& aUserSpaceToImageSpace,
                           const gfxRect&   aSubimage,
                           const gfxRect&   aSourceRect,
                           const gfxRect&   aImageRect,
                           const gfxRect&   aFill,
                           const gfxImageSurface::gfxImageFormat aFormat,
                           const gfxPattern::GraphicsFilter& aFilter)
{
    bool doTile = !aImageRect.Contains(aSourceRect);

    nsRefPtr<gfxASurface> currentTarget = aContext->CurrentSurface();
    gfxASurface::gfxSurfaceType surfaceType = currentTarget->GetType();
    gfxMatrix deviceSpaceToImageSpace =
        DeviceToImageTransform(aContext, aUserSpaceToImageSpace);

    AutoCairoPixmanBugWorkaround workaround(aContext, deviceSpaceToImageSpace,
                                            aFill, surfaceType);
    if (!workaround.Succeeded())
        return;

    nsRefPtr<gfxDrawable> drawable = aDrawable;

    // OK now, the hard part left is to account for the subimage sampling
    // restriction. If all the transforms involved are just integer
    // translations, then we assume no resampling will occur so there's
    // nothing to do.
    // XXX if only we had source-clipping in cairo!
    if (aContext->CurrentMatrix().HasNonIntegerTranslation() ||
        aUserSpaceToImageSpace.HasNonIntegerTranslation()) {
        if (doTile || !aSubimage.Contains(aImageRect)) {
            nsRefPtr<gfxDrawable> restrictedDrawable =
              CreateSamplingRestrictedDrawable(aDrawable, aContext,
                                               aUserSpaceToImageSpace, aSourceRect,
                                               aSubimage, aFormat);
            if (restrictedDrawable) {
                drawable.swap(restrictedDrawable);
            }
        }
        // We no longer need to tile: Either we never needed to, or we already
        // filled a surface with the tiled pattern; this surface can now be
        // drawn without tiling.
        doTile = false;
    }

    gfxContext::GraphicsOperator op = aContext->CurrentOperator();
    if ((op == gfxContext::OPERATOR_OVER || workaround.PushedGroup()) &&
        aFormat == gfxASurface::ImageFormatRGB24) {
        aContext->SetOperator(OptimalFillOperator());
    }

    drawable->Draw(aContext, aFill, doTile, aFilter, aUserSpaceToImageSpace);

    aContext->SetOperator(op);
}

/* static */ int
gfxUtils::ImageFormatToDepth(gfxASurface::gfxImageFormat aFormat)
{
    switch (aFormat) {
        case gfxASurface::ImageFormatARGB32:
            return 32;
        case gfxASurface::ImageFormatRGB24:
            return 24;
        case gfxASurface::ImageFormatRGB16_565:
            return 16;
        default:
            break;
    }
    return 0;
}

static void
PathFromRegionInternal(gfxContext* aContext, const nsIntRegion& aRegion,
                       bool aSnap)
{
  aContext->NewPath();
  nsIntRegionRectIterator iter(aRegion);
  const nsIntRect* r;
  while ((r = iter.Next()) != nsnull) {
    aContext->Rectangle(gfxRect(r->x, r->y, r->width, r->height), aSnap);
  }
}

static void
ClipToRegionInternal(gfxContext* aContext, const nsIntRegion& aRegion,
                     bool aSnap)
{
  PathFromRegionInternal(aContext, aRegion, aSnap);
  aContext->Clip();
}

/*static*/ void
gfxUtils::ClipToRegion(gfxContext* aContext, const nsIntRegion& aRegion)
{
  ClipToRegionInternal(aContext, aRegion, false);
}

/*static*/ void
gfxUtils::ClipToRegionSnapped(gfxContext* aContext, const nsIntRegion& aRegion)
{
  ClipToRegionInternal(aContext, aRegion, true);
}

/*static*/ gfxFloat
gfxUtils::ClampToScaleFactor(gfxFloat aVal)
{
  // Arbitary scale factor limitation. We can increase this
  // for better scaling performance at the cost of worse
  // quality.
  static const gfxFloat kScaleResolution = 2;

  // Negative scaling is just a flip and irrelevant to
  // our resolution calculation.
  if (aVal < 0.0) {
    aVal = -aVal;
  }

  gfxFloat power = log(aVal)/log(kScaleResolution);

  // If power is within 1e-6 of an integer, round to nearest to
  // prevent floating point errors, otherwise round up to the
  // next integer value.
  if (fabs(power - NS_round(power)) < 1e-6) {
    power = NS_round(power);
  } else {
    power = ceil(power);
  }

  return pow(kScaleResolution, power);
}


/*static*/ void
gfxUtils::PathFromRegion(gfxContext* aContext, const nsIntRegion& aRegion)
{
  PathFromRegionInternal(aContext, aRegion, false);
}

/*static*/ void
gfxUtils::PathFromRegionSnapped(gfxContext* aContext, const nsIntRegion& aRegion)
{
  PathFromRegionInternal(aContext, aRegion, true);
}


bool
gfxUtils::GfxRectToIntRect(const gfxRect& aIn, nsIntRect* aOut)
{
  *aOut = nsIntRect(PRInt32(aIn.X()), PRInt32(aIn.Y()),
  PRInt32(aIn.Width()), PRInt32(aIn.Height()));
  return gfxRect(aOut->x, aOut->y, aOut->width, aOut->height).IsEqualEdges(aIn);
}

void
gfxUtils::GetYCbCrToRGBDestFormatAndSize(const PlanarYCbCrImage::Data& aData,
                                         gfxASurface::gfxImageFormat& aSuggestedFormat,
                                         gfxIntSize& aSuggestedSize)
{
  gfx::YUVType yuvtype =
    gfx::TypeFromSize(aData.mYSize.width,
                      aData.mYSize.height,
                      aData.mCbCrSize.width,
                      aData.mCbCrSize.height);

  // 'prescale' is true if the scaling is to be done as part of the
  // YCbCr to RGB conversion rather than on the RGB data when rendered.
  bool prescale = aSuggestedSize.width > 0 && aSuggestedSize.height > 0 &&
                    aSuggestedSize != aData.mPicSize;

  if (aSuggestedFormat == gfxASurface::ImageFormatRGB16_565) {
#if defined(HAVE_YCBCR_TO_RGB565)
    if (prescale &&
        !gfx::IsScaleYCbCrToRGB565Fast(aData.mPicX,
                                       aData.mPicY,
                                       aData.mPicSize.width,
                                       aData.mPicSize.height,
                                       aSuggestedSize.width,
                                       aSuggestedSize.height,
                                       yuvtype,
                                       gfx::FILTER_BILINEAR) &&
        gfx::IsConvertYCbCrToRGB565Fast(aData.mPicX,
                                        aData.mPicY,
                                        aData.mPicSize.width,
                                        aData.mPicSize.height,
                                        yuvtype)) {
      prescale = false;
    }
#else
    // yuv2rgb16 function not available
    aSuggestedFormat = gfxASurface::ImageFormatRGB24;
#endif
  }
  else if (aSuggestedFormat != gfxASurface::ImageFormatRGB24) {
    // No other formats are currently supported.
    aSuggestedFormat = gfxASurface::ImageFormatRGB24;
  }
  if (aSuggestedFormat == gfxASurface::ImageFormatRGB24) {
    /* ScaleYCbCrToRGB32 does not support a picture offset, nor 4:4:4 data.
       See bugs 639415 and 640073. */
    if (aData.mPicX != 0 || aData.mPicY != 0 || yuvtype == gfx::YV24)
      prescale = false;
  }
  if (!prescale) {
    aSuggestedSize = aData.mPicSize;
  }
}

void
gfxUtils::ConvertYCbCrToRGB(const PlanarYCbCrImage::Data& aData,
                            const gfxASurface::gfxImageFormat& aDestFormat,
                            const gfxIntSize& aDestSize,
                            unsigned char* aDestBuffer,
                            PRInt32 aStride)
{
  gfx::YUVType yuvtype =
    gfx::TypeFromSize(aData.mYSize.width,
                      aData.mYSize.height,
                      aData.mCbCrSize.width,
                      aData.mCbCrSize.height);

  // Convert from YCbCr to RGB now, scaling the image if needed.
  if (aDestSize != aData.mPicSize) {
#if defined(HAVE_YCBCR_TO_RGB565)
    if (aDestFormat == gfxASurface::ImageFormatRGB16_565) {
      gfx::ScaleYCbCrToRGB565(aData.mYChannel,
                              aData.mCbChannel,
                              aData.mCrChannel,
                              aDestBuffer,
                              aData.mPicX,
                              aData.mPicY,
                              aData.mPicSize.width,
                              aData.mPicSize.height,
                              aDestSize.width,
                              aDestSize.height,
                              aData.mYStride,
                              aData.mCbCrStride,
                              aStride,
                              yuvtype,
                              gfx::FILTER_BILINEAR);
    } else
#endif
      gfx::ScaleYCbCrToRGB32(aData.mYChannel,
                             aData.mCbChannel,
                             aData.mCrChannel,
                             aDestBuffer,
                             aData.mPicSize.width,
                             aData.mPicSize.height,
                             aDestSize.width,
                             aDestSize.height,
                             aData.mYStride,
                             aData.mCbCrStride,
                             aStride,
                             yuvtype,
                             gfx::ROTATE_0,
                             gfx::FILTER_BILINEAR);
  } else { // no prescale
#if defined(HAVE_YCBCR_TO_RGB565)
    if (aDestFormat == gfxASurface::ImageFormatRGB16_565) {
      gfx::ConvertYCbCrToRGB565(aData.mYChannel,
                                aData.mCbChannel,
                                aData.mCrChannel,
                                aDestBuffer,
                                aData.mPicX,
                                aData.mPicY,
                                aData.mPicSize.width,
                                aData.mPicSize.height,
                                aData.mYStride,
                                aData.mCbCrStride,
                                aStride,
                                yuvtype);
    } else // aDestFormat != gfxASurface::ImageFormatRGB16_565
#endif
      gfx::ConvertYCbCrToRGB32(aData.mYChannel,
                               aData.mCbChannel,
                               aData.mCrChannel,
                               aDestBuffer,
                               aData.mPicX,
                               aData.mPicY,
                               aData.mPicSize.width,
                               aData.mPicSize.height,
                               aData.mYStride,
                               aData.mCbCrStride,
                               aStride,
                               yuvtype);
  }
}

#ifdef MOZ_DUMP_PAINTING
/* static */ void
gfxUtils::WriteAsPNG(DrawTarget* aDT, const char* aFile)
{
  aDT->Flush();
  nsRefPtr<gfxASurface> surf = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(aDT);
  if (surf) {
    surf->WriteAsPNG(aFile);
  } else {
    NS_WARNING("Failed to get Thebes surface!");
  }
}

/* static */ void
gfxUtils::DumpAsDataURL(DrawTarget* aDT)
{
  aDT->Flush();
  nsRefPtr<gfxASurface> surf = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(aDT);
  if (surf) {
    surf->DumpAsDataURL();
  } else {
    NS_WARNING("Failed to get Thebes surface!");
  }
}

/* static */ void
gfxUtils::CopyAsDataURL(DrawTarget* aDT)
{
  aDT->Flush();
  nsRefPtr<gfxASurface> surf = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(aDT);
  if (surf) {
    surf->CopyAsDataURL();
  } else {
    NS_WARNING("Failed to get Thebes surface!");
  }
}
#endif
