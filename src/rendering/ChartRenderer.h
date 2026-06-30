#pragma once
#include <QPainter>
#include <QRectF>
#include "models/ChartData.h"

// Renders ChartData to a QPainter or to an inline SVG string.
// All geometry is computed from the given bounding rect.
class ChartRenderer {
public:
    // Render to QPainter (2D editor, 3D texture, thumbnails)
    static void paint(QPainter& p, const QRectF& rect, const ChartData& data);

    // Human-readable chart type name (German)
    static QString typeName(const QString& type);
    // Short icon/emoji for chart type
    static QString typeIcon(const QString& type);

private:
    // ── Per-type renderers ────────────────────────────────────────────────
    static void paintBar(QPainter&, const QRectF&, const ChartData&, float sc, bool horiz);
    static void paintLine(QPainter&, const QRectF&, const ChartData&, float sc, bool filled);
    static void paintPie(QPainter&, const QRectF&, const ChartData&, float sc, bool donut);
    static void paintScatter(QPainter&, const QRectF&, const ChartData&, float sc);
    static void paintFlowchart(QPainter&, const QRectF&, const ChartData&, float sc);
    static void paintMindmap(QPainter&, const QRectF&, const ChartData&, float sc);
    static void paintOrgchart(QPainter&, const QRectF&, const ChartData&, float sc);
    static void paintTimeline(QPainter&, const QRectF&, const ChartData&, float sc);
    static void paintGantt(QPainter&, const QRectF&, const ChartData&, float sc);
    static void paintUml(QPainter&, const QRectF&, const ChartData&, float sc);
    static void paintVenn(QPainter&, const QRectF&, const ChartData&, float sc);

    // ── Shared helpers ────────────────────────────────────────────────────
    static QColor resolveColor(const QString& hex, int idx);
    static void   drawLegend(QPainter&, const QRectF&, const ChartData&, float sc);
    static void   drawArrow(QPainter&, QPointF from, QPointF to, float headLen);
    static void   drawNodeShape(QPainter&, const QRectF& r, const QString& shape,
                                const QColor& fill, const QColor& border, float bw);
    static void   drawPlaceholder(QPainter&, const QRectF& r, const QString& type);
    static QFont  scaledFont(const QString& family, float baseSize, float sc,
                             bool bold = false);
    // Map from node positions (0-1000) to chart rect pixels
    static QRectF nodeRect(const ChartNode& n, const QRectF& chartR);
    // Layout tree recursively and return child count
    static int    layoutTree(QVector<ChartNode>& nodes, const QString& rootId,
                             double cx, double cy, double angleFrom, double angleTo,
                             double radius, int depth);
    static int    layoutOrgTree(QVector<ChartNode>& nodes, const QString& rootId,
                                double x, double y, double levelH, double colW);
};
