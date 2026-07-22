#pragma once
#include <QImage>
#include <QRectF>
#include <QSize>
#include <QVector>
#include <QPointF>
#include "models/MeshGradientData.h"

// Renders a free-form multi-point "mesh gradient" fill for Shape elements:
// the colored control points are Delaunay-triangulated and each triangle is
// filled with barycentric-interpolated RGBA, like per-vertex colors on an
// OpenGL triangle. Used by both SlideEditor2D and SlideEditor3D (the latter
// bakes shapes into a QImage texture via the same QPainter primitives as
// 2D), so calling this one function from both call sites guarantees
// pixel-identical output in both views. The exported HTML/JS render path
// ports this same algorithm to JavaScript (see HtmlExporter.cpp).
namespace MeshGradientRenderer {

struct Triangle { int a, b, c; };

// Bowyer-Watson Delaunay triangulation over points in general position.
// Defensive: merges points closer than ~1e-4 (normalized) and drops
// near-zero-area (collinear) triangles. Returns empty if fewer than 3
// usable points remain after filtering.
QVector<Triangle> delaunayTriangulate(const QVector<QPointF>& pts);

// Renders the mesh gradient into an ARGB32_Premultiplied image of size
// pixelSize, clipped (antialiased) to the shape's outline. For shapeType
// "rect" with a non-zero cornerRadiusPx, a rounded rect is used for the
// clip (matching the caller's drawRoundedRect stroke) instead of
// ShapeUtils::shapeToPath's sharp-cornered "rect" fallback. Returns a fully
// transparent image if mesh.isUsable() is false.
QImage renderMeshGradient(const QString& shapeType, const QSize& pixelSize,
                          const MeshGradientData& mesh,
                          const QSizeF& cornerRadiusPx = QSizeF(0, 0),
                          const QString& customPathData = QString());

} // namespace MeshGradientRenderer
