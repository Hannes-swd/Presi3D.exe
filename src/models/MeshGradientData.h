#pragma once
#include <QVector>
#include <QColor>
#include <QJsonObject>

// One colored control point of a shape's mesh-gradient fill. x/y are
// normalized 0..1 relative to the shape element's own bounding rect, so
// points stay put correctly when the shape is resized. r/g/b/a are stored
// as raw numeric components (not QColor) so the JS web-export renderer can
// read them straight out of the JSON without re-parsing a color string.
struct MeshGradientPoint {
    double x = 0.5, y = 0.5; // 0..1 within element bounds
    int    r = 255, g = 255, b = 255; // 0-255
    double a = 1.0;                   // 0..1

    QColor toQColor() const { return QColor(r, g, b, int(a * 255 + 0.5)); }
    static MeshGradientPoint fromQColor(double px, double py, const QColor& c) {
        return { px, py, c.red(), c.green(), c.blue(), c.alphaF() };
    }
};

struct MeshGradientData {
    QVector<MeshGradientPoint> points;

    bool isUsable() const { return points.size() >= 3; }

    QJsonObject toJson() const;
    static MeshGradientData fromJson(const QJsonObject& obj);
};
