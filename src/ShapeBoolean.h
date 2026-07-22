#pragma once
#include <QPainterPath>
#include <QVector>

struct SlideElement;

// Boolean ("Pathfinder"-style) combination of Shape/Text elements into a
// single result path, used by SlideEditor2D::booleanCutSelection() to turn
// 2+ selected elements into one new "custom" Shape element. Built entirely
// on QPainterPath's native united/subtracted/intersected — no external
// geometry library needed.
namespace ShapeBoolean {

enum class Op { Union, Subtract, Intersect, Exclude };

// Returns e's outline in absolute slide coordinates (position, size and
// rotation baked in). Shape elements go through ShapeUtils::shapeToPath;
// Text elements are converted via glyph outlines, mirroring SlideEditor2D's
// text layout (word-wrap, alignment, vertical alignment) so the cut result
// matches what the text looks like on screen. Any other element type
// returns an empty path.
QPainterPath elementToPath(const SlideElement& e);

// Combines elems (front-to-back irrelevant to the caller — pass them in
// z-order, back/lowest-index first) with op. Subtract accumulates as
// "backmost minus everything after it" in the given order. Returns an
// empty path if elems has fewer than 2 usable (Shape/Text) entries or the
// operation yields nothing (e.g. non-overlapping Intersect).
QPainterPath combine(const QVector<const SlideElement*>& backToFront, Op op);

} // namespace ShapeBoolean
