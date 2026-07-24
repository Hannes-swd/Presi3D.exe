#pragma once
#include <QImage>
#include <QSize>
#include <QRectF>
#include <QColor>
#include <functional>
#include "models/DataModel.h"

// Renders a configurable drop shadow (offset/blur/spread/color) for any
// SlideElement type, by painting the element's own silhouette (any opaque
// color, only alpha matters) into an offscreen buffer, then blurring/
// growing/tinting just the alpha channel — the same principle as CSS
// filter:drop-shadow(). Shared by SlideEditor2D's live 2D canvas and
// ShadowDialog's preview canvas so both stay pixel-consistent.
namespace ShadowRenderer {

// Paints the element's silhouette into the painter's current coordinate
// system, filling exactly the rect (0,0,w,h) it was built for (see
// buildSilhouettePainter). Only alpha coverage matters — color is ignored
// by render(), which re-tints the result uniformly.
using SilhouettePainter = std::function<void(QPainter&)>;

// Extra margin (px) render() adds around localSize so a blurred/spread
// silhouette isn't clipped. Callers need this to correctly position the
// returned image (it must be drawn `padding` px up/left of the element).
int padding(float blurPx, float spreadPx);

// Renders a blurred, spread, uniformly-tinted silhouette. The returned image
// is sized localSize + 2*padding(blurPx,spreadPx) in each dimension, with the
// unpadded silhouette content centered at offset (padding, padding).
QImage render(const QSize& localSize, const SilhouettePainter& paintSilhouette,
              float blurPx, float spreadPx, const QColor& shadowColor);

// Builds a type-correct SilhouettePainter for `e`, sized to fill localRect
// (normally QRectF(0,0,w,h), matching the element's on-screen size in
// whatever pixel space the caller is drawing in — text font size is scaled
// by localRect.height()/e.height so it matches that same pixel space).
// The returned function captures `e` BY REFERENCE (localRect by value) — only
// valid for a synchronous call (e.g. immediately passed to render()), never
// stored past the caller's current scope.
SilhouettePainter buildSilhouettePainter(const SlideElement& e, const QRectF& localRect);

} // namespace ShadowRenderer
