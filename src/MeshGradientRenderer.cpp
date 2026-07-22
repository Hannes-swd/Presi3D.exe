#include "MeshGradientRenderer.h"
#include "ShapeUtils.h"
#include <QPainter>
#include <QCache>
#include <algorithm>
#include <cmath>

namespace MeshGradientRenderer {

namespace {

struct Edge {
    int a, b;
    bool operator==(const Edge& o) const { return (a == o.a && b == o.b) || (a == o.b && b == o.a); }
};

double triArea2(const QPointF& a, const QPointF& b, const QPointF& c) {
    return (b.x() - a.x()) * (c.y() - a.y()) - (c.x() - a.x()) * (b.y() - a.y());
}

// True if p lies inside the circumcircle of triangle (a,b,c), regardless of
// the triangle's winding order (checked via the signed area sign).
bool circumcircleContains(const QPointF& a, const QPointF& b, const QPointF& c, const QPointF& p) {
    double ax = a.x() - p.x(), ay = a.y() - p.y();
    double bx = b.x() - p.x(), by = b.y() - p.y();
    double cx = c.x() - p.x(), cy = c.y() - p.y();
    double det = (ax * ax + ay * ay) * (bx * cy - cx * by)
               - (bx * bx + by * by) * (ax * cy - cx * ay)
               + (cx * cx + cy * cy) * (ax * by - bx * ay);
    return triArea2(a, b, c) > 0 ? det > 0 : det < 0;
}

} // namespace

QVector<Triangle> delaunayTriangulate(const QVector<QPointF>& ptsIn) {
    if (ptsIn.size() < 3) return {};

    // Dedupe points closer than a bbox-relative epsilon so two clicks in the
    // same spot don't produce a zero-radius circumcircle / NaNs.
    QRectF bbox(ptsIn[0], QSizeF(0, 0));
    for (const auto& p : ptsIn) bbox |= QRectF(p, QSizeF(0, 0));
    const double eps = std::max(std::hypot(bbox.width(), bbox.height()) * 1e-4, 1e-6);

    QVector<QPointF> pts;
    for (const auto& p : ptsIn) {
        bool dup = false;
        for (const auto& q : pts)
            if (std::hypot(p.x() - q.x(), p.y() - q.y()) < eps) { dup = true; break; }
        if (!dup) pts << p;
    }
    if (pts.size() < 3) return {};

    // Super-triangle enclosing all points, appended after the real points.
    double minX = pts[0].x(), maxX = pts[0].x(), minY = pts[0].y(), maxY = pts[0].y();
    for (const auto& p : pts) {
        minX = std::min(minX, p.x()); maxX = std::max(maxX, p.x());
        minY = std::min(minY, p.y()); maxY = std::max(maxY, p.y());
    }
    const double deltaMax = std::max(maxX - minX, maxY - minY) * 10.0 + 10.0;
    const double midX = (minX + maxX) / 2.0, midY = (minY + maxY) / 2.0;

    const int stA = pts.size(), stB = stA + 1, stC = stA + 2;
    QVector<QPointF> work = pts;
    work << QPointF(midX - 2 * deltaMax, midY - deltaMax)
         << QPointF(midX, midY + 2 * deltaMax)
         << QPointF(midX + 2 * deltaMax, midY - deltaMax);

    QVector<Triangle> tris;
    tris << Triangle{stA, stB, stC};

    for (int pi = 0; pi < pts.size(); ++pi) {
        const QPointF& p = work[pi];

        QVector<Triangle> bad;
        for (const auto& t : tris)
            if (circumcircleContains(work[t.a], work[t.b], work[t.c], p))
                bad << t;

        // Boundary edges of the hole left by removing `bad`: edges that
        // appear in exactly one bad triangle.
        QVector<Edge> polygon;
        for (int i = 0; i < bad.size(); ++i) {
            const Edge edges[3] = {{bad[i].a, bad[i].b}, {bad[i].b, bad[i].c}, {bad[i].c, bad[i].a}};
            for (const auto& e : edges) {
                int shared = 0;
                for (int j = 0; j < bad.size(); ++j) {
                    if (j == i) continue;
                    const Edge e2s[3] = {{bad[j].a, bad[j].b}, {bad[j].b, bad[j].c}, {bad[j].c, bad[j].a}};
                    for (const auto& e2 : e2s) if (e == e2) { ++shared; break; }
                }
                if (shared == 0) polygon << e;
            }
        }

        QVector<Triangle> keep;
        for (const auto& t : tris) {
            bool isBad = false;
            for (const auto& b : bad)
                if (b.a == t.a && b.b == t.b && b.c == t.c) { isBad = true; break; }
            if (!isBad) keep << t;
        }
        tris = keep;

        for (const auto& e : polygon)
            tris << Triangle{e.a, e.b, pi};
    }

    // Drop triangles touching the super-triangle and near-zero-area
    // (collinear) triangles.
    QVector<Triangle> result;
    for (const auto& t : tris) {
        if (t.a >= stA || t.b >= stA || t.c >= stA) continue;
        if (std::fabs(triArea2(pts[t.a], pts[t.b], pts[t.c])) < 1e-9) continue;
        result << t;
    }
    return result;
}

QImage renderMeshGradient(const QString& shapeType, const QSize& pixelSize, const MeshGradientData& mesh,
                          const QSizeF& cornerRadiusPx, const QString& customPathData) {
    QImage blank(pixelSize.expandedTo(QSize(1, 1)), QImage::Format_ARGB32_Premultiplied);
    blank.fill(Qt::transparent);
    if (!mesh.isUsable() || pixelSize.width() < 1 || pixelSize.height() < 1) return blank;

    static QCache<QString, QImage> cache(32);
    QString key = QString("%1|%2x%3|%4,%5|%6").arg(shapeType).arg(pixelSize.width()).arg(pixelSize.height())
                      .arg(cornerRadiusPx.width()).arg(cornerRadiusPx.height()).arg(customPathData);
    for (const auto& p : mesh.points)
        key += QString("|%1,%2,%3,%4,%5,%6").arg(p.x).arg(p.y).arg(p.r).arg(p.g).arg(p.b).arg(p.a);
    if (QImage* cached = cache.object(key))
        return *cached;

    QImage img(pixelSize, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QVector<QPointF> pts;
    pts.reserve(mesh.points.size());
    for (const auto& p : mesh.points)
        pts << QPointF(p.x * pixelSize.width(), p.y * pixelSize.height());

    const QVector<Triangle> tris = delaunayTriangulate(pts);

    for (const auto& t : tris) {
        const QPointF& p0 = pts[t.a];
        const QPointF& p1 = pts[t.b];
        const QPointF& p2 = pts[t.c];
        const MeshGradientPoint& c0 = mesh.points[t.a];
        const MeshGradientPoint& c1 = mesh.points[t.b];
        const MeshGradientPoint& c2 = mesh.points[t.c];

        const int minX = std::max(0, int(std::floor(std::min({p0.x(), p1.x(), p2.x()}))));
        const int maxX = std::min(pixelSize.width() - 1, int(std::ceil(std::max({p0.x(), p1.x(), p2.x()}))));
        const int minY = std::max(0, int(std::floor(std::min({p0.y(), p1.y(), p2.y()}))));
        const int maxY = std::min(pixelSize.height() - 1, int(std::ceil(std::max({p0.y(), p1.y(), p2.y()}))));
        if (minX > maxX || minY > maxY) continue;

        const double denom = (p1.y() - p2.y()) * (p0.x() - p2.x()) + (p2.x() - p1.x()) * (p0.y() - p2.y());
        if (std::fabs(denom) < 1e-9) continue;

        for (int y = minY; y <= maxY; ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
            const double py = y + 0.5;
            for (int x = minX; x <= maxX; ++x) {
                const double px = x + 0.5;
                const double w0 = ((p1.y() - p2.y()) * (px - p2.x()) + (p2.x() - p1.x()) * (py - p2.y())) / denom;
                const double w1 = ((p2.y() - p0.y()) * (px - p2.x()) + (p0.x() - p2.x()) * (py - p2.y())) / denom;
                const double w2 = 1.0 - w0 - w1;
                if (w0 < -1e-6 || w1 < -1e-6 || w2 < -1e-6) continue;

                const double r = w0 * c0.r + w1 * c1.r + w2 * c2.r;
                const double g = w0 * c0.g + w1 * c1.g + w2 * c2.g;
                const double b = w0 * c0.b + w1 * c1.b + w2 * c2.b;
                const double a = std::clamp(w0 * c0.a + w1 * c1.a + w2 * c2.a, 0.0, 1.0);

                const int ai = int(a * 255.0 + 0.5);
                const int ri = int(std::clamp(r, 0.0, 255.0) * a + 0.5);
                const int gi = int(std::clamp(g, 0.0, 255.0) * a + 0.5);
                const int bi = int(std::clamp(b, 0.0, 255.0) * a + 0.5);
                line[x] = qRgba(ri, gi, bi, ai);
            }
        }
    }

    // Antialiased clip to the shape's actual outline (QPainter::setClipPath
    // isn't antialiased by default, so mask-and-composite instead).
    QImage mask(pixelSize, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    {
        QPainter mp(&mask);
        mp.setRenderHint(QPainter::Antialiasing);
        mp.setPen(Qt::NoPen);
        mp.setBrush(Qt::white);
        const QRectF full(QPointF(0, 0), QSizeF(pixelSize));
        if (shapeType == "rect" && (cornerRadiusPx.width() > 0 || cornerRadiusPx.height() > 0)) {
            QPainterPath rr;
            rr.addRoundedRect(full, cornerRadiusPx.width(), cornerRadiusPx.height());
            mp.drawPath(rr);
        } else {
            mp.drawPath(ShapeUtils::shapeToPath(shapeType, full, customPathData));
        }
    }
    {
        QPainter ip(&img);
        ip.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        ip.drawImage(0, 0, mask);
    }

    cache.insert(key, new QImage(img));
    return img;
}

} // namespace MeshGradientRenderer
