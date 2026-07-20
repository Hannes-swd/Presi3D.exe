#pragma once
#include <QImage>
#include <QSize>
#include <QSizeF>
#include <QString>

// Renders a user-picked image as a "texture fill" for Shape elements: the
// image always covers the shape's bounds (like CSS background-size:cover,
// preserving aspect ratio, no gaps), with offsetX/offsetY panning within the
// resulting slack and scale zooming in further. Used by both SlideEditor2D
// and SlideEditor3D (the latter bakes shapes into a QImage texture via the
// same QPainter primitives as 2D), so calling this one function from both
// call sites guarantees pixel-identical output in both views. Mirrors
// MeshGradientRenderer::renderMeshGradient's shape-clipping approach.
namespace ImageFillRenderer {

// Renders the image fill into an ARGB32_Premultiplied image of size
// pixelSize, clipped (antialiased) to the shape's outline. For shapeType
// "rect" with a non-zero cornerRadiusPx, a rounded rect is used for the clip
// instead of ShapeUtils::shapeToPath's sharp-cornered "rect" fallback.
// Returns a fully transparent image if imagePath is empty or fails to load.
QImage renderImageFill(const QString& shapeType, const QSize& pixelSize,
                        const QString& imagePath, float offsetX, float offsetY, float scale,
                        const QSizeF& cornerRadiusPx = QSizeF(0, 0));

} // namespace ImageFillRenderer
