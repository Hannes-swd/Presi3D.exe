#pragma once
#include <QString>
#include <QVector>
#include <QPainterPath>
#include <QRectF>

namespace ShapeUtils {

struct ShapeDef {
    QString id;
    QString label;
    QString category;
};

const QVector<ShapeDef>& allShapes();

// Returns a QPainterPath for the given shape type fitted inside rect.
QPainterPath shapeToPath(const QString& type, const QRectF& rect);

// Returns a CSS style property (no trailing semicolon) for the shape,
// e.g. "border-radius:50%" for circle, "clip-path:polygon(...)" for polygon.
// Returns empty string for rect or unsupported types.
QString shapeToCssStyle(const QString& type);

} // namespace ShapeUtils
