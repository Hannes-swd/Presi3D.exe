#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QJsonObject>

// ─── Data series for bar/line/area/scatter/pie/donut ─────────────────────────

struct ChartSeries {
    QString         name;
    QVector<double> values;
    QString         color;        // hex "#rrggbb", empty → auto from palette
    QStringList     valueColors;  // per-element colors (pie/donut/bar-mixed)

    // Parallel to `values` (same indices, same length once normalized).
    // Empty entry = use the fixed number in `values[i]`. Non-empty = a
    // {variable} expression that overrides values[i] when rendered with a
    // VariableSet (see ChartRenderer::substituteVars, VARIABLEN_PLAN.md).
    QStringList     valueExprs;
};

// ─── Node for structural diagrams (flowchart/mindmap/orgchart/uml) ────────────

struct ChartNode {
    QString     id;
    QString     label;            // UML: "Name|attr: T\nattr2: T|method(): void"
    QString     shape = "rect";   // rect | diamond | oval | rounded | parallelogram
    QString     color;            // fill color hex, empty → auto
    QStringList edges;            // target IDs; UML: "id|type" (type=inheritance etc.)
    double      x = 0, y = 0;    // 0–1000 units within chart area (flowchart/uml)
    double      w = 150, h = 60;

    bool operator==(const ChartNode& o) const { return id == o.id; }
};

// ─── Timeline event ──────────────────────────────────────────────────────────

struct ChartTimelineEvent {
    QString label;
    QString desc;
    double  pos   = 50.0;   // 0–100 along the axis
    QString color;
};

// ─── Gantt task ──────────────────────────────────────────────────────────────

struct ChartGanttTask {
    QString name;
    double  start = 0.0;
    double  end   = 50.0;
    QString color;
};

// ─── Venn circle ─────────────────────────────────────────────────────────────

struct ChartVennCircle {
    QString label;
    double  cx = 50, cy = 50;
    double  radius  = 30;
    QString color;
    double  opacity = 0.45;
};

// ─── Main chart data container ───────────────────────────────────────────────

struct ChartData {
    // Chart type:
    //   data:       bar | bar_h | line | area | pie | donut | scatter
    //   structural: flowchart | mindmap | orgchart | uml
    //   special:    timeline | gantt | venn
    QString type = "bar";

    QString title;
    QString description;
    bool    showLegend = true;
    bool    showGrid   = true;

    // ── Data-driven (bar/line/area/scatter/pie/donut) ─────────────────────
    QStringList          labels;   // x-axis labels or pie slice labels
    QVector<ChartSeries> series;

    // ── Structural (flowchart/mindmap/orgchart/uml) ────────────────────────
    QVector<ChartNode> nodes;

    // ── Timeline ──────────────────────────────────────────────────────────
    QVector<ChartTimelineEvent> events;

    // ── Gantt ─────────────────────────────────────────────────────────────
    QVector<ChartGanttTask> tasks;
    QStringList             ganttAxisLabels;

    // ── Venn ──────────────────────────────────────────────────────────────
    QVector<ChartVennCircle> vennCircles;

    // Default 10-color Tableau-inspired palette
    static QStringList defaultPalette() {
        return {"#4e79a7","#f28e2b","#e15759","#76b7b2","#59a14f",
                "#edc948","#b07aa1","#ff9da7","#9c755f","#bab0ac"};
    }
    static QString defaultColor(int idx) {
        const QStringList p = defaultPalette();
        return p[((idx % p.size()) + p.size()) % p.size()];
    }

    // Creates a chart with sensible sample data for the given type
    static ChartData createDefault(const QString& type);

    // JSON round-trip (for project save/load)
    QJsonObject toJson()  const;
    static ChartData fromJson(const QJsonObject& obj);

    // Returns true for chart types that use the data table (labels + series)
    bool isDataChart() const {
        return type=="bar"||type=="bar_h"||type=="line"||type=="area"
            || type=="pie"||type=="donut"||type=="scatter";
    }
    bool isStructural() const {
        return type=="flowchart"||type=="mindmap"||type=="orgchart"||type=="uml";
    }
};
