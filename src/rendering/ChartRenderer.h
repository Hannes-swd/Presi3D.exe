#pragma once
#include <QPainter>
#include <QPixmap>
#include <QRectF>
#include "models/ChartData.h"
#include "models/VariableModel.h"

// Renders ChartData to a QPainter or to an inline SVG string.
// All geometry is computed from the given bounding rect.
class ChartRenderer {
public:
    // Render to QPainter (2D editor, 3D texture, thumbnails).
    // vars/currentSlideId are optional: when vars is non-null, {name} placeholders
    // in the title/labels/series names/node labels/etc. are resolved before drawing
    // (see VARIABLEN_PLAN.md). Pass nullptr for contexts with no presentation/slide
    // context (e.g. the chart-type picker's sample preview).
    static void paint(QPainter& p, const QRectF& rect, const ChartData& data,
                      const VariableSet* vars = nullptr, const QString& currentSlideId = {});

    // Human-readable chart type name (German)
    static QString typeName(const QString& type);
    // Material Symbols icon resource name for chart type (see resources/icons)
    static QString typeIcon(const QString& type);
    // Rendered icon pixmap for chart type, tinted with `color`
    static QPixmap typeIconPixmap(const QString& type, int size, const QColor& color = QColor(80, 80, 80));

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
    // Returns a copy of d with {name} placeholders resolved in every text field
    // that gets drawn (title, labels, series/node/event/task/venn names).
    static ChartData substituteVars(const ChartData& d, const VariableSet& vars,
                                    const QString& currentSlideId);
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
