#include "ChartRenderer.h"
#include <QPainterPath>
#include <QMap>
#include <QtMath>
#include <algorithm>

// ─── Helpers ──────────────────────────────────────────────────────────────────

QColor ChartRenderer::resolveColor(const QString& hex, int idx) {
    if (!hex.isEmpty()) return QColor(hex);
    return QColor(ChartData::defaultColor(idx));
}

QFont ChartRenderer::scaledFont(const QString& family, float baseSize, float sc, bool bold) {
    QFont f(family.isEmpty() ? "Arial" : family,
            qMax(5, int(baseSize * sc)));
    f.setBold(bold);
    return f;
}

void ChartRenderer::drawPlaceholder(QPainter& p, const QRectF& r, const QString& type) {
    p.save();
    p.fillRect(r, QColor(248, 249, 250));
    p.setPen(QPen(QColor(200, 200, 200), 1.5));
    p.drawRect(r.adjusted(0.5, 0.5, -0.5, -0.5));
    p.setPen(QColor(150, 150, 150));
    float sc = qMin(float(r.width()), float(r.height())) / 300.f;
    p.setFont(scaledFont("Arial", 13, qBound(0.4f, sc, 2.f)));
    p.drawText(r, Qt::AlignCenter, typeIcon(type) + "  " + typeName(type) + "\n(no data)");
    p.restore();
}

void ChartRenderer::drawArrow(QPainter& p, QPointF from, QPointF to, float headLen) {
    p.drawLine(from, to);
    QPointF dir = to - from;
    double len = qSqrt(dir.x()*dir.x() + dir.y()*dir.y());
    if (len < 1.0) return;
    dir /= len;
    QPointF perp(-dir.y(), dir.x());
    QPointF tip = to;
    QPointF b1 = tip - dir * headLen + perp * (headLen * 0.4);
    QPointF b2 = tip - dir * headLen - perp * (headLen * 0.4);
    QPainterPath path;
    path.moveTo(tip);
    path.lineTo(b1);
    path.lineTo(b2);
    path.closeSubpath();
    p.fillPath(path, p.pen().color());
}

void ChartRenderer::drawNodeShape(QPainter& p, const QRectF& r,
                                   const QString& shape, const QColor& fill,
                                   const QColor& border, float bw) {
    p.setPen(bw > 0 ? QPen(border, bw) : Qt::NoPen);
    p.setBrush(fill);
    if (shape == "oval" || shape == "circle") {
        p.drawEllipse(r);
    } else if (shape == "diamond") {
        QPointF pts[4] = {
            {r.center().x(), r.top()},
            {r.right(),      r.center().y()},
            {r.center().x(), r.bottom()},
            {r.left(),       r.center().y()}
        };
        p.drawPolygon(pts, 4);
    } else if (shape == "parallelogram") {
        float skew = r.height() * 0.3f;
        QPointF pts[4] = {
            {r.left() + skew, r.top()},
            {r.right(),       r.top()},
            {r.right() - skew,r.bottom()},
            {r.left(),        r.bottom()}
        };
        p.drawPolygon(pts, 4);
    } else if (shape == "rounded") {
        p.drawRoundedRect(r, 8, 8);
    } else {
        p.drawRect(r);
    }
}

QRectF ChartRenderer::nodeRect(const ChartNode& n, const QRectF& chartR) {
    double sx = chartR.width()  / 1000.0;
    double sy = chartR.height() / 1000.0;
    return QRectF(chartR.x() + n.x * sx - n.w * sx / 2.0,
                  chartR.y() + n.y * sy - n.h * sy / 2.0,
                  n.w * sx, n.h * sy);
}

void ChartRenderer::drawLegend(QPainter& p, const QRectF& lr,
                                const ChartData& d, float sc) {
    if (d.series.isEmpty()) return;
    p.save();
    float itemH = qMax(14.f, 16.f * sc);
    float boxSz = itemH * 0.7f;
    p.setFont(scaledFont("Arial", 9.5f, sc));
    float y = float(lr.top()) + 4;
    for (int i = 0; i < d.series.size() && y + itemH <= lr.bottom(); ++i) {
        QColor c = resolveColor(d.series[i].color, i);
        p.fillRect(QRectF(lr.x(), y + (itemH - boxSz) / 2, boxSz, boxSz), c);
        p.setPen(Qt::black);
        p.drawText(QRectF(lr.x() + boxSz + 4, y, lr.width() - boxSz - 8, itemH),
                   Qt::AlignVCenter | Qt::AlignLeft, d.series[i].name);
        y += itemH + 2;
    }
    p.restore();
}

// ─── Main dispatch ────────────────────────────────────────────────────────────

void ChartRenderer::paint(QPainter& p, const QRectF& rect, const ChartData& d) {
    if (rect.width() < 8 || rect.height() < 8) return;
    float sc = qBound(0.05f, float(qMin(rect.width(), rect.height())) / 350.f, 4.f);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setClipRect(rect);

    const QString& t = d.type;
    if      (t == "bar")       paintBar(p, rect, d, sc, false);
    else if (t == "bar_h")     paintBar(p, rect, d, sc, true);
    else if (t == "line")      paintLine(p, rect, d, sc, false);
    else if (t == "area")      paintLine(p, rect, d, sc, true);
    else if (t == "pie")       paintPie(p, rect, d, sc, false);
    else if (t == "donut")     paintPie(p, rect, d, sc, true);
    else if (t == "scatter")   paintScatter(p, rect, d, sc);
    else if (t == "flowchart") paintFlowchart(p, rect, d, sc);
    else if (t == "mindmap")   paintMindmap(p, rect, d, sc);
    else if (t == "orgchart")  paintOrgchart(p, rect, d, sc);
    else if (t == "timeline")  paintTimeline(p, rect, d, sc);
    else if (t == "gantt")     paintGantt(p, rect, d, sc);
    else if (t == "uml")       paintUml(p, rect, d, sc);
    else if (t == "venn")      paintVenn(p, rect, d, sc);
    else                       drawPlaceholder(p, rect, t);

    p.restore();
}

// ─── Bar Chart ────────────────────────────────────────────────────────────────

void ChartRenderer::paintBar(QPainter& p, const QRectF& rect,
                              const ChartData& d, float sc, bool horiz) {
    if (d.series.isEmpty() || d.labels.isEmpty()) {
        drawPlaceholder(p, rect, horiz ? "bar_h" : "bar"); return;
    }
    int nCats = d.labels.size();
    int nSer  = d.series.size();

    double maxVal = 1;
    for (const auto& s : d.series)
        for (double v : s.values)
            maxVal = qMax(maxVal, v);

    // Layout margins
    float titleH  = d.title.isEmpty() ? 0 : 22.f * sc;
    float legendW = (d.showLegend && nSer > 1) ? 90.f * sc : 0;
    float axisL   = horiz ? 80.f * sc : 40.f * sc;
    float axisB   = horiz ? 30.f * sc : 28.f * sc;
    float pad     = 8.f * sc;

    // Title
    if (!d.title.isEmpty()) {
        p.save();
        p.setPen(QColor(40,40,40));
        p.setFont(scaledFont("Arial", 12, sc, true));
        p.drawText(QRectF(rect.x(), rect.y(), rect.width(), titleH + pad),
                   Qt::AlignHCenter | Qt::AlignTop, d.title);
        p.restore();
    }

    QRectF plot(rect.x() + axisL, rect.y() + titleH + pad,
                rect.width() - axisL - legendW - pad,
                rect.height() - titleH - axisB - pad * 2);

    if (plot.width() < 10 || plot.height() < 10) return;

    // Grid
    if (d.showGrid) {
        p.save();
        p.setPen(QPen(QColor(210, 210, 210), 0.8 * sc));
        int ng = 5;
        for (int g = 0; g <= ng; ++g) {
            if (horiz) {
                float x = float(plot.left() + g * plot.width() / ng);
                p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            } else {
                float y = float(plot.top() + (ng-g) * plot.height() / ng);
                p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            }
        }
        p.restore();
    }

    // Bars
    float catSize = float(horiz ? plot.height() : plot.width()) / nCats;
    float barSize = catSize * 0.75f / nSer;
    float barGap  = catSize * 0.25f / 2.f;

    for (int cat = 0; cat < nCats; ++cat) {
        for (int ser = 0; ser < nSer; ++ser) {
            const ChartSeries& s = d.series[ser];
            double val = (cat < s.values.size()) ? s.values[cat] : 0.0;
            float barLen = float(val / maxVal * (horiz ? plot.width() : plot.height()));

            QColor c;
            if (cat < s.valueColors.size() && !s.valueColors[cat].isEmpty())
                c = QColor(s.valueColors[cat]);
            else
                c = resolveColor(s.color, ser);

            QRectF bar;
            if (horiz) {
                float y = float(plot.top()) + cat * catSize + barGap + ser * barSize;
                bar = QRectF(plot.left(), y, barLen, barSize - 1.5f * sc);
            } else {
                float x = float(plot.left()) + cat * catSize + barGap + ser * barSize;
                bar = QRectF(x, float(plot.bottom()) - barLen, barSize - 1.5f * sc, barLen);
            }
            p.fillRect(bar, c);
        }

        // Category label
        p.save();
        p.setPen(QColor(80, 80, 80));
        p.setFont(scaledFont("Arial", 8.5f, sc));
        QString lbl = (cat < d.labels.size()) ? d.labels[cat] : QString::number(cat + 1);
        if (horiz) {
            QRectF lr(rect.x(), float(plot.top()) + cat * catSize, axisL - 4, catSize);
            p.drawText(lr, Qt::AlignRight | Qt::AlignVCenter, lbl);
        } else {
            QRectF lr(float(plot.left()) + cat * catSize, float(plot.bottom()) + 2,
                      catSize, axisB);
            p.drawText(lr, Qt::AlignHCenter | Qt::AlignTop, lbl);
        }
        p.restore();
    }

    // Value axis labels
    p.save();
    p.setPen(QColor(100, 100, 100));
    p.setFont(scaledFont("Arial", 8, sc));
    int ng = 5;
    for (int g = 0; g <= ng; ++g) {
        double val = maxVal * g / ng;
        QString lbl = (val >= 10000) ? QString("%1k").arg(int(val/1000))
                    : (val >= 1000)  ? QString::number(int(val/100)*100)
                    :                   QString::number(int(val));
        if (horiz) {
            float x = float(plot.left() + g * plot.width() / ng);
            p.drawText(QRectF(x - 20, float(plot.bottom()) + 2, 40, axisB),
                       Qt::AlignHCenter | Qt::AlignTop, lbl);
        } else {
            float y = float(plot.top() + (ng-g) * plot.height() / ng);
            p.drawText(QRectF(rect.x(), y - 10, axisL - 4, 20),
                       Qt::AlignRight | Qt::AlignVCenter, lbl);
        }
    }
    p.restore();

    // Axes
    p.save();
    p.setPen(QPen(QColor(100, 100, 100), 1.2 * sc));
    p.drawLine(plot.bottomLeft(), horiz ? plot.bottomRight() : plot.topLeft());
    p.drawLine(plot.bottomLeft(), horiz ? plot.topLeft() : plot.bottomRight());
    p.restore();

    // Legend
    if (d.showLegend && nSer > 1) {
        drawLegend(p, QRectF(rect.right() - legendW, rect.y() + titleH + pad,
                             legendW, rect.height() - titleH - pad), d, sc);
    }
}

// ─── Line / Area Chart ────────────────────────────────────────────────────────

void ChartRenderer::paintLine(QPainter& p, const QRectF& rect,
                               const ChartData& d, float sc, bool filled) {
    if (d.series.isEmpty() || d.labels.isEmpty()) {
        drawPlaceholder(p, rect, filled ? "area" : "line"); return;
    }
    int nCats = d.labels.size();
    int nSer  = d.series.size();

    double maxVal = 1;
    for (const auto& s : d.series)
        for (double v : s.values)
            maxVal = qMax(maxVal, v);

    float titleH  = d.title.isEmpty() ? 0 : 22.f * sc;
    float legendW = (d.showLegend && nSer > 1) ? 90.f * sc : 0;
    float axisL   = 40.f * sc;
    float axisB   = 28.f * sc;
    float pad     = 8.f * sc;

    if (!d.title.isEmpty()) {
        p.save();
        p.setPen(QColor(40,40,40));
        p.setFont(scaledFont("Arial", 12, sc, true));
        p.drawText(QRectF(rect.x(), rect.y(), rect.width(), titleH + pad),
                   Qt::AlignHCenter | Qt::AlignTop, d.title);
        p.restore();
    }

    QRectF plot(rect.x() + axisL, rect.y() + titleH + pad,
                rect.width() - axisL - legendW - pad,
                rect.height() - titleH - axisB - pad * 2);
    if (plot.width() < 10 || plot.height() < 10) return;

    // Grid
    if (d.showGrid) {
        p.save();
        p.setPen(QPen(QColor(210, 210, 210), 0.8 * sc));
        for (int g = 0; g <= 5; ++g) {
            float y = float(plot.top() + (5-g) * plot.height() / 5);
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
        p.restore();
    }

    // Series
    float stepX = nCats > 1 ? float(plot.width()) / (nCats - 1) : 0;

    for (int ser = 0; ser < nSer; ++ser) {
        const ChartSeries& s = d.series[ser];
        QColor c = resolveColor(s.color, ser);

        QVector<QPointF> pts;
        for (int cat = 0; cat < nCats; ++cat) {
            double v = (cat < s.values.size()) ? s.values[cat] : 0.0;
            float x = float(plot.left() + cat * stepX);
            float y = float(plot.bottom() - v / maxVal * plot.height());
            pts.append({x, y});
        }

        if (filled && pts.size() >= 2) {
            QPolygonF poly;
            poly << QPointF(pts.first().x(), float(plot.bottom()));
            for (const QPointF& pt : pts) poly << pt;
            poly << QPointF(pts.last().x(), float(plot.bottom()));
            p.save();
            QColor fc = c; fc.setAlphaF(0.3);
            p.setPen(Qt::NoPen);
            p.setBrush(fc);
            p.drawPolygon(poly);
            p.restore();
        }

        p.save();
        p.setPen(QPen(c, 2 * sc, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        for (int i = 0; i + 1 < pts.size(); ++i)
            p.drawLine(pts[i], pts[i+1]);
        // Points
        p.setBrush(c);
        p.setPen(QPen(Qt::white, 1 * sc));
        float r = 3.5f * sc;
        for (const QPointF& pt : pts)
            p.drawEllipse(pt, r, r);
        p.restore();
    }

    // X labels
    p.save();
    p.setPen(QColor(80, 80, 80));
    p.setFont(scaledFont("Arial", 8.5f, sc));
    for (int cat = 0; cat < nCats; ++cat) {
        float x = float(plot.left() + cat * stepX);
        QString lbl = (cat < d.labels.size()) ? d.labels[cat] : QString::number(cat + 1);
        p.drawText(QRectF(x - 20, float(plot.bottom()) + 2, 40, axisB),
                   Qt::AlignHCenter | Qt::AlignTop, lbl);
    }
    p.restore();

    // Y labels
    p.save();
    p.setPen(QColor(100, 100, 100));
    p.setFont(scaledFont("Arial", 8, sc));
    for (int g = 0; g <= 5; ++g) {
        double val = maxVal * g / 5;
        float y = float(plot.bottom() - g * plot.height() / 5);
        QString lbl = (val >= 1000) ? QString("%1k").arg(int(val/1000))
                    :                  QString::number(int(val));
        p.drawText(QRectF(rect.x(), y - 10, axisL - 4, 20),
                   Qt::AlignRight | Qt::AlignVCenter, lbl);
    }
    p.restore();

    // Axes
    p.save();
    p.setPen(QPen(QColor(100, 100, 100), 1.2 * sc));
    p.drawLine(plot.bottomLeft(), plot.bottomRight());
    p.drawLine(plot.bottomLeft(), plot.topLeft());
    p.restore();

    if (d.showLegend && nSer > 1)
        drawLegend(p, QRectF(rect.right()-legendW, rect.y()+titleH+pad,
                             legendW, rect.height()-titleH-pad), d, sc);
}

// ─── Pie / Donut ─────────────────────────────────────────────────────────────

void ChartRenderer::paintPie(QPainter& p, const QRectF& rect,
                              const ChartData& d, float sc, bool donut) {
    if (d.series.isEmpty() || d.labels.isEmpty()) {
        drawPlaceholder(p, rect, donut ? "donut" : "pie"); return;
    }
    const ChartSeries& s0 = d.series[0];
    if (s0.values.isEmpty()) { drawPlaceholder(p, rect, d.type); return; }

    float titleH = d.title.isEmpty() ? 0 : 22.f * sc;
    float pad    = 8.f * sc;

    if (!d.title.isEmpty()) {
        p.save();
        p.setPen(QColor(40,40,40));
        p.setFont(scaledFont("Arial", 12, sc, true));
        p.drawText(QRectF(rect.x(), rect.y(), rect.width(), titleH + pad),
                   Qt::AlignHCenter | Qt::AlignTop, d.title);
        p.restore();
    }

    QRectF contentR(rect.x(), rect.y() + titleH + pad,
                    rect.width(), rect.height() - titleH - pad);

    // Deduce slice count and total
    int nSlices = qMin(s0.values.size(), d.labels.size());
    double total = 0;
    for (int i = 0; i < nSlices; ++i) total += s0.values[i];
    if (total <= 0) { drawPlaceholder(p, rect, d.type); return; }

    // Legend column width
    float legendW = d.showLegend ? qMin(120.f * sc, float(contentR.width() * 0.35)) : 0;
    QRectF pieArea(contentR.x(), contentR.y(),
                   contentR.width() - legendW, contentR.height());

    // Pie rect (square, centered)
    float side = qMin(float(pieArea.width()), float(pieArea.height())) - 20 * sc;
    QRectF pieR(pieArea.center().x() - side/2, pieArea.center().y() - side/2, side, side);

    int startAngle = 90 * 16; // 12 o'clock = Qt 90°
    for (int i = 0; i < nSlices; ++i) {
        int spanAngle = -int(s0.values[i] / total * 360.0 * 16);
        QColor c;
        if (i < s0.valueColors.size() && !s0.valueColors[i].isEmpty())
            c = QColor(s0.valueColors[i]);
        else
            c = resolveColor("", i);

        p.save();
        p.setPen(QPen(Qt::white, 1.5 * sc));
        p.setBrush(c);
        p.drawPie(pieR, startAngle, spanAngle);
        p.restore();

        startAngle += spanAngle;
    }

    // Donut hole
    if (donut) {
        float holeR = side * 0.38f;
        QRectF hole(pieR.center().x() - holeR, pieR.center().y() - holeR,
                    holeR * 2, holeR * 2);
        QColor bg = Qt::white;
        p.save();
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawEllipse(hole);
        // Percentage text
        p.setPen(QColor(60,60,60));
        p.setFont(scaledFont("Arial", 13, sc, true));
        p.drawText(hole, Qt::AlignCenter, "100%");
        p.restore();
    }

    // Legend
    if (d.showLegend) {
        p.save();
        QRectF lr(contentR.right() - legendW + 4, contentR.top() + 4,
                  legendW - 8, contentR.height());
        float itemH = qMax(14.f, 16.f * sc);
        float boxSz = itemH * 0.7f;
        p.setFont(scaledFont("Arial", 9, sc));
        float y = float(lr.top());
        for (int i = 0; i < nSlices && y + itemH <= lr.bottom(); ++i) {
            QColor c;
            if (i < s0.valueColors.size() && !s0.valueColors[i].isEmpty())
                c = QColor(s0.valueColors[i]);
            else
                c = resolveColor("", i);
            p.fillRect(QRectF(lr.x(), y + (itemH - boxSz)/2, boxSz, boxSz), c);
            p.setPen(QColor(60,60,60));
            QString lbl = (i < d.labels.size()) ? d.labels[i] : "";
            double pct = s0.values[i] / total * 100.0;
            p.drawText(QRectF(lr.x() + boxSz + 3, y, lr.width() - boxSz - 3, itemH),
                       Qt::AlignVCenter | Qt::AlignLeft,
                       lbl + " " + QString::number(pct, 'f', 1) + "%");
            y += itemH + 2;
        }
        p.restore();
    }
}

// ─── Scatter ─────────────────────────────────────────────────────────────────

void ChartRenderer::paintScatter(QPainter& p, const QRectF& rect,
                                  const ChartData& d, float sc) {
    if (d.series.isEmpty()) { drawPlaceholder(p, rect, "scatter"); return; }

    double minX=0, maxX=1, minY=0, maxY=1;
    bool first = true;
    for (const auto& s : d.series) {
        for (int i = 0; i + 1 < s.values.size(); i += 2) {
            double x = s.values[i], y = s.values[i+1];
            if (first) { minX=maxX=x; minY=maxY=y; first=false; }
            minX=qMin(minX,x); maxX=qMax(maxX,x);
            minY=qMin(minY,y); maxY=qMax(maxY,y);
        }
    }
    if (maxX <= minX) maxX = minX + 1;
    if (maxY <= minY) maxY = minY + 1;

    float titleH  = d.title.isEmpty() ? 0 : 22.f * sc;
    float legendW = (d.showLegend && d.series.size() > 1) ? 90.f * sc : 0;
    float axisL   = 40.f * sc, axisB = 28.f * sc, pad = 8.f * sc;

    if (!d.title.isEmpty()) {
        p.save();
        p.setPen(QColor(40,40,40));
        p.setFont(scaledFont("Arial", 12, sc, true));
        p.drawText(QRectF(rect.x(), rect.y(), rect.width(), titleH + pad),
                   Qt::AlignHCenter | Qt::AlignTop, d.title);
        p.restore();
    }

    QRectF plot(rect.x() + axisL, rect.y() + titleH + pad,
                rect.width() - axisL - legendW - pad,
                rect.height() - titleH - axisB - pad * 2);
    if (plot.width() < 10 || plot.height() < 10) return;

    if (d.showGrid) {
        p.save();
        p.setPen(QPen(QColor(210,210,210), 0.8 * sc));
        for (int g = 0; g <= 5; ++g) {
            float y = float(plot.top() + (5-g)*plot.height()/5);
            p.drawLine(QPointF(plot.left(),y), QPointF(plot.right(),y));
            float x = float(plot.left() + g*plot.width()/5);
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        p.restore();
    }

    auto toScreen = [&](double x, double y) -> QPointF {
        return {float(plot.left() + (x - minX)/(maxX - minX) * plot.width()),
                float(plot.bottom() - (y - minY)/(maxY - minY) * plot.height())};
    };

    for (int ser = 0; ser < d.series.size(); ++ser) {
        const ChartSeries& s = d.series[ser];
        QColor c = resolveColor(s.color, ser);
        p.save();
        p.setPen(QPen(Qt::white, 0.8 * sc));
        p.setBrush(c);
        float r = 4.f * sc;
        for (int i = 0; i + 1 < s.values.size(); i += 2)
            p.drawEllipse(toScreen(s.values[i], s.values[i+1]), r, r);
        p.restore();
    }

    p.save();
    p.setPen(QPen(QColor(100,100,100), 1.2 * sc));
    p.drawLine(plot.bottomLeft(), plot.bottomRight());
    p.drawLine(plot.bottomLeft(), plot.topLeft());
    p.restore();

    if (d.showLegend && d.series.size() > 1)
        drawLegend(p, QRectF(rect.right()-legendW, rect.y()+titleH+pad,
                             legendW, rect.height()-titleH-pad), d, sc);
}

// ─── Flowchart ────────────────────────────────────────────────────────────────

void ChartRenderer::paintFlowchart(QPainter& p, const QRectF& rect,
                                    const ChartData& d, float sc) {
    if (d.nodes.isEmpty()) { drawPlaceholder(p, rect, "flowchart"); return; }

    float titleH = d.title.isEmpty() ? 0 : 22.f * sc;
    float pad = 10.f * sc;

    if (!d.title.isEmpty()) {
        p.save();
        p.setPen(QColor(40,40,40));
        p.setFont(scaledFont("Arial", 12, sc, true));
        p.drawText(QRectF(rect.x(), rect.y(), rect.width(), titleH + pad),
                   Qt::AlignHCenter | Qt::AlignTop, d.title);
        p.restore();
    }

    QRectF chartR(rect.x() + pad, rect.y() + titleH + pad,
                  rect.width() - 2*pad, rect.height() - titleH - 2*pad);

    QMap<QString, QRectF> nodeRects;
    for (const auto& n : d.nodes)
        nodeRects[n.id] = nodeRect(n, chartR);

    // Draw edges first
    p.save();
    p.setPen(QPen(QColor(100,100,100), 1.5 * sc, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(QColor(100,100,100));
    for (const auto& n : d.nodes) {
        if (!nodeRects.contains(n.id)) continue;
        QRectF fr = nodeRects[n.id];
        for (const QString& tgt : n.edges) {
            if (!nodeRects.contains(tgt)) continue;
            QRectF tr = nodeRects[tgt];
            QPointF from(fr.center().x(), fr.bottom());
            QPointF to(tr.center().x(), tr.top());
            drawArrow(p, from, to, 7 * sc);
        }
    }
    p.restore();

    // Draw nodes
    for (const auto& n : d.nodes) {
        if (!nodeRects.contains(n.id)) continue;
        QRectF nr = nodeRects[n.id];
        QColor fill = resolveColor(n.color.isEmpty() ? "" : n.color,
                                   d.nodes.indexOf(n) % 10);
        if (n.color.isEmpty()) fill = QColor(100,149,237);
        else                   fill = QColor(n.color);
        QColor border = fill.darker(130);
        drawNodeShape(p, nr, n.shape, fill, border, 1.5 * sc);

        // Label
        p.save();
        p.setPen(Qt::white);
        p.setFont(scaledFont("Arial", 9, sc, true));
        p.drawText(nr.adjusted(4,4,-4,-4), Qt::AlignCenter | Qt::TextWordWrap, n.label);
        p.restore();
    }
}

// ─── Mindmap ─────────────────────────────────────────────────────────────────

int ChartRenderer::layoutTree(QVector<ChartNode>& nodes, const QString& rootId,
                               double cx, double cy,
                               double angleFrom, double angleTo,
                               double radius, int depth) {
    int rootIdx = -1;
    for (int i = 0; i < nodes.size(); ++i)
        if (nodes[i].id == rootId) { rootIdx = i; break; }
    if (rootIdx < 0) return 0;

    nodes[rootIdx].x = cx;
    nodes[rootIdx].y = cy;

    const QStringList& children = nodes[rootIdx].edges;
    if (children.isEmpty()) return 1;

    double totalAngle = angleTo - angleFrom;
    double step = totalAngle / children.size();
    int total = 1;
    for (int i = 0; i < children.size(); ++i) {
        double a = angleFrom + step * i + step / 2.0;
        double childCx = cx + radius * qCos(qDegreesToRadians(a));
        double childCy = cy + radius * qSin(qDegreesToRadians(a));
        double nextR = radius * 0.65;
        double spread = step * 0.85;
        total += layoutTree(nodes, children[i], childCx, childCy,
                            a - spread/2, a + spread/2, nextR, depth + 1);
    }
    return total;
}

void ChartRenderer::paintMindmap(QPainter& p, const QRectF& rect,
                                  const ChartData& d, float sc) {
    if (d.nodes.isEmpty()) { drawPlaceholder(p, rect, "mindmap"); return; }

    float titleH = d.title.isEmpty() ? 0 : 22.f * sc;
    float pad = 8.f * sc;

    if (!d.title.isEmpty()) {
        p.save();
        p.setPen(QColor(40,40,40));
        p.setFont(scaledFont("Arial", 12, sc, true));
        p.drawText(QRectF(rect.x(), rect.y(), rect.width(), titleH + pad),
                   Qt::AlignHCenter | Qt::AlignTop, d.title);
        p.restore();
    }

    // Layout mindmap nodes radially (positions in 0-1000 space)
    QVector<ChartNode> nodes = d.nodes;
    if (!nodes.isEmpty()) {
        // Root at center (500, 500 in 0-1000 space)
        layoutTree(nodes, nodes[0].id, 500, 500, -180, 180, 280, 0);
    }

    QRectF chartR(rect.x() + pad, rect.y() + titleH + pad,
                  rect.width() - 2*pad, rect.height() - titleH - 2*pad);

    QMap<QString, QRectF> nodeRects;
    for (const auto& n : nodes) {
        ChartNode tmp = n; tmp.w = 110; tmp.h = 40;
        nodeRects[n.id] = nodeRect(tmp, chartR);
    }

    // Edges (lines from parent center to child center)
    p.save();
    for (const auto& n : nodes) {
        if (!nodeRects.contains(n.id)) continue;
        QPointF from = nodeRects[n.id].center();
        for (const QString& tgt : n.edges) {
            if (!nodeRects.contains(tgt)) continue;
            QColor c = n.color.isEmpty() ? QColor(150,150,200) : QColor(n.color);
            c.setAlphaF(0.6);
            p.setPen(QPen(c, 2 * sc, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(from, nodeRects[tgt].center());
        }
    }
    p.restore();

    // Nodes
    int nodeIdx = 0;
    for (const auto& n : nodes) {
        if (!nodeRects.contains(n.id)) continue;
        QRectF nr = nodeRects[n.id];
        bool isRoot = (n.id == nodes[0].id);
        QColor fill = n.color.isEmpty() ? resolveColor("", nodeIdx) : QColor(n.color);
        if (isRoot) fill = fill.darker(120);

        p.save();
        p.setPen(QPen(fill.darker(130), 1.5 * sc));
        p.setBrush(fill);
        p.drawRoundedRect(nr, 5 * sc, 5 * sc);
        p.setPen(Qt::white);
        p.setFont(scaledFont("Arial", isRoot ? 9.5f : 8.5f, sc, isRoot));
        p.drawText(nr.adjusted(3,2,-3,-2), Qt::AlignCenter | Qt::TextWordWrap, n.label);
        p.restore();
        ++nodeIdx;
    }
}

// ─── Orgchart ────────────────────────────────────────────────────────────────

int ChartRenderer::layoutOrgTree(QVector<ChartNode>& nodes, const QString& rootId,
                                  double x, double y, double levelH, double colW) {
    int rootIdx = -1;
    for (int i = 0; i < nodes.size(); ++i)
        if (nodes[i].id == rootId) { rootIdx = i; break; }
    if (rootIdx < 0) return 0;

    const QStringList& children = nodes[rootIdx].edges;
    int nChildren = children.size();

    int totalLeaves = nChildren == 0 ? 1 : 0;
    QVector<int> childLeaves(nChildren);
    for (int i = 0; i < nChildren; ++i) {
        childLeaves[i] = layoutOrgTree(nodes, children[i], 0, 0, levelH, colW);
        totalLeaves += childLeaves[i];
    }

    nodes[rootIdx].x = x + totalLeaves * colW / 2.0;
    nodes[rootIdx].y = y;

    double childX = x;
    for (int i = 0; i < nChildren; ++i) {
        int idx = -1;
        for (int j = 0; j < nodes.size(); ++j)
            if (nodes[j].id == children[i]) { idx = j; break; }
        if (idx >= 0) {
            nodes[idx].x = childX + childLeaves[i] * colW / 2.0;
            nodes[idx].y = y + levelH;
        }
        childX += childLeaves[i] * colW;
        layoutOrgTree(nodes, children[i], childX - childLeaves[i]*colW,
                      y + levelH, levelH, colW);
    }
    return totalLeaves;
}

void ChartRenderer::paintOrgchart(QPainter& p, const QRectF& rect,
                                   const ChartData& d, float sc) {
    if (d.nodes.isEmpty()) { drawPlaceholder(p, rect, "orgchart"); return; }

    float titleH = d.title.isEmpty() ? 0 : 22.f * sc;
    float pad = 8.f * sc;

    if (!d.title.isEmpty()) {
        p.save();
        p.setPen(QColor(40,40,40));
        p.setFont(scaledFont("Arial", 12, sc, true));
        p.drawText(QRectF(rect.x(), rect.y(), rect.width(), titleH + pad),
                   Qt::AlignHCenter | Qt::AlignTop, d.title);
        p.restore();
    }

    QVector<ChartNode> nodes = d.nodes;
    if (!nodes.isEmpty()) {
        // Count leaves to determine layout width
        int nLeaves = 0;
        std::function<int(const QString&)> countLeaves = [&](const QString& id) -> int {
            for (auto& n : nodes) {
                if (n.id == id) {
                    if (n.edges.isEmpty()) return 1;
                    int s = 0;
                    for (const QString& e : n.edges) s += countLeaves(e);
                    return s;
                }
            }
            return 1;
        };
        nLeaves = qMax(1, countLeaves(nodes[0].id));
        double colW  = 1000.0 / nLeaves;
        double levelH = qMin(350.0, 1000.0 / (nodes.size() > 3 ? 3.0 : 2.0));
        layoutOrgTree(nodes, nodes[0].id, 0, 80, levelH, colW);
    }

    QRectF chartR(rect.x() + pad, rect.y() + titleH + pad,
                  rect.width() - 2*pad, rect.height() - titleH - 2*pad);
    QMap<QString, QRectF> nodeRects;
    for (const auto& n : nodes) {
        ChartNode tmp = n; tmp.w = 140; tmp.h = 52;
        nodeRects[n.id] = nodeRect(tmp, chartR);
    }

    // Lines from parent to children
    p.save();
    p.setPen(QPen(QColor(150,150,150), 1.5 * sc));
    for (const auto& n : nodes) {
        if (!nodeRects.contains(n.id)) continue;
        QPointF from(nodeRects[n.id].center().x(), nodeRects[n.id].bottom());
        for (const QString& tgt : n.edges) {
            if (!nodeRects.contains(tgt)) continue;
            QPointF to(nodeRects[tgt].center().x(), nodeRects[tgt].top());
            float midY = (float(from.y()) + float(to.y())) / 2.f;
            p.drawLine(from, QPointF(from.x(), midY));
            p.drawLine(QPointF(from.x(), midY), QPointF(to.x(), midY));
            p.drawLine(QPointF(to.x(), midY), to);
        }
    }
    p.restore();

    // Node boxes
    int ni = 0;
    for (const auto& n : nodes) {
        if (!nodeRects.contains(n.id)) continue;
        QRectF nr = nodeRects[n.id];
        QColor fill = n.color.isEmpty() ? resolveColor("", ni) : QColor(n.color);
        p.save();
        p.setPen(QPen(fill.darker(130), 1.5 * sc));
        p.setBrush(fill);
        p.drawRoundedRect(nr, 5 * sc, 5 * sc);
        p.setPen(Qt::white);
        p.setFont(scaledFont("Arial", 9, sc, true));
        p.drawText(nr, Qt::AlignCenter | Qt::TextWordWrap, n.label);
        p.restore();
        ++ni;
    }
}

// ─── Timeline ────────────────────────────────────────────────────────────────

void ChartRenderer::paintTimeline(QPainter& p, const QRectF& rect,
                                   const ChartData& d, float sc) {
    if (d.events.isEmpty()) { drawPlaceholder(p, rect, "timeline"); return; }

    float titleH = d.title.isEmpty() ? 0 : 22.f * sc;
    float pad = 12.f * sc;

    if (!d.title.isEmpty()) {
        p.save();
        p.setPen(QColor(40,40,40));
        p.setFont(scaledFont("Arial", 12, sc, true));
        p.drawText(QRectF(rect.x(), rect.y(), rect.width(), titleH + pad),
                   Qt::AlignHCenter | Qt::AlignTop, d.title);
        p.restore();
    }

    QRectF areaR(rect.x() + pad, rect.y() + titleH + pad,
                 rect.width() - 2*pad, rect.height() - titleH - 2*pad);

    float lineY  = float(areaR.center().y());
    float lineX1 = float(areaR.left());
    float lineX2 = float(areaR.right());
    float arrowH = 6.f * sc;

    // Main axis line
    p.save();
    p.setPen(QPen(QColor(100,100,100), 2 * sc, Qt::SolidLine, Qt::RoundCap));
    drawArrow(p, {lineX1, lineY}, {lineX2, lineY}, arrowH);
    p.restore();

    float labelZoneH = (areaR.height() / 2.f - 12.f * sc);
    float dotR = 5.f * sc;

    for (int i = 0; i < d.events.size(); ++i) {
        const ChartTimelineEvent& ev = d.events[i];
        float x = lineX1 + float(ev.pos / 100.0) * (lineX2 - lineX1 - arrowH);
        bool above = (i % 2 == 0);

        QColor c = ev.color.isEmpty() ? resolveColor("", i) : QColor(ev.color);

        // Vertical tick
        p.save();
        p.setPen(QPen(c, 1.5 * sc));
        p.setBrush(c);
        float tickLen = 20.f * sc;
        if (above)
            p.drawLine(QPointF(x, lineY - dotR), QPointF(x, lineY - tickLen));
        else
            p.drawLine(QPointF(x, lineY + dotR), QPointF(x, lineY + tickLen));
        p.drawEllipse(QPointF(x, lineY), dotR, dotR);
        p.restore();

        // Label
        p.save();
        p.setPen(QColor(40,40,40));
        p.setFont(scaledFont("Arial", 8.5f, sc, true));
        float lblW = 80.f * sc;
        float lblH = labelZoneH;
        float lblX = x - lblW / 2.f;
        float lblY = above ? (lineY - tickLen - lblH) : (lineY + tickLen);
        p.drawText(QRectF(lblX, lblY, lblW, lblH * 0.45f),
                   Qt::AlignHCenter | Qt::AlignBottom, ev.label);
        if (!ev.desc.isEmpty()) {
            p.setPen(QColor(120,120,120));
            p.setFont(scaledFont("Arial", 7.5f, sc));
            p.drawText(QRectF(lblX, lblY + lblH * 0.45f, lblW, lblH * 0.55f),
                       Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, ev.desc);
        }
        p.restore();
    }
}

// ─── Gantt ────────────────────────────────────────────────────────────────────

void ChartRenderer::paintGantt(QPainter& p, const QRectF& rect,
                                const ChartData& d, float sc) {
    if (d.tasks.isEmpty()) { drawPlaceholder(p, rect, "gantt"); return; }

    float titleH = d.title.isEmpty() ? 0 : 22.f * sc;
    float pad = 8.f * sc;
    float nameW = 90.f * sc;
    float headerH = 24.f * sc;

    if (!d.title.isEmpty()) {
        p.save();
        p.setPen(QColor(40,40,40));
        p.setFont(scaledFont("Arial", 12, sc, true));
        p.drawText(QRectF(rect.x(), rect.y(), rect.width(), titleH + pad),
                   Qt::AlignHCenter | Qt::AlignTop, d.title);
        p.restore();
    }

    QRectF areaR(rect.x() + pad, rect.y() + titleH + pad,
                 rect.width() - 2*pad, rect.height() - titleH - 2*pad);

    QRectF barArea(areaR.x() + nameW, areaR.y() + headerH,
                   areaR.width() - nameW, areaR.height() - headerH);

    // Background and grid
    p.fillRect(barArea, QColor(248, 249, 250));
    if (!d.ganttAxisLabels.isEmpty()) {
        int nCols = d.ganttAxisLabels.size();
        p.save();
        p.setFont(scaledFont("Arial", 8, sc));
        p.setPen(QColor(100,100,100));
        for (int i = 0; i < nCols; ++i) {
            float x = float(barArea.x() + i * barArea.width() / nCols);
            float w = float(barArea.width() / nCols);
            p.drawText(QRectF(x, areaR.y(), w, headerH),
                       Qt::AlignHCenter | Qt::AlignVCenter, d.ganttAxisLabels[i]);
            p.setPen(QPen(QColor(220,220,220), 0.8 * sc));
            p.drawLine(QPointF(x, areaR.y()), QPointF(x, areaR.bottom()));
            p.setPen(QColor(100,100,100));
        }
        p.restore();
    }

    float rowH = float(barArea.height()) / d.tasks.size();

    for (int i = 0; i < d.tasks.size(); ++i) {
        const ChartGanttTask& t = d.tasks[i];
        float y = float(barArea.top()) + i * rowH;
        QColor c = t.color.isEmpty() ? resolveColor("", i) : QColor(t.color);

        // Alternating row bg
        if (i % 2 == 0)
            p.fillRect(QRectF(barArea.x(), y, barArea.width(), rowH), QColor(240,244,255,120));

        // Task name
        p.save();
        p.setPen(QColor(60,60,60));
        p.setFont(scaledFont("Arial", 8.5f, sc));
        p.drawText(QRectF(areaR.x(), y, nameW - 4, rowH),
                   Qt::AlignRight | Qt::AlignVCenter, t.name);
        p.restore();

        // Bar
        float bx = float(barArea.x() + t.start/100.0 * barArea.width());
        float bw = float((t.end - t.start)/100.0 * barArea.width());
        float bh = rowH * 0.6f;
        float by = y + (rowH - bh) / 2.f;
        QRectF bar(bx, by, bw, bh);
        p.save();
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(bar, 3 * sc, 3 * sc);
        p.restore();
    }

    // Border
    p.save();
    p.setPen(QPen(QColor(180,180,180), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(barArea);
    p.restore();
}

// ─── UML ─────────────────────────────────────────────────────────────────────

void ChartRenderer::paintUml(QPainter& p, const QRectF& rect,
                              const ChartData& d, float sc) {
    if (d.nodes.isEmpty()) { drawPlaceholder(p, rect, "uml"); return; }

    float titleH = d.title.isEmpty() ? 0 : 22.f * sc;
    float pad = 10.f * sc;

    if (!d.title.isEmpty()) {
        p.save();
        p.setPen(QColor(40,40,40));
        p.setFont(scaledFont("Arial", 12, sc, true));
        p.drawText(QRectF(rect.x(), rect.y(), rect.width(), titleH + pad),
                   Qt::AlignHCenter | Qt::AlignTop, d.title);
        p.restore();
    }

    QRectF chartR(rect.x() + pad, rect.y() + titleH + pad,
                  rect.width() - 2*pad, rect.height() - titleH - 2*pad);

    QMap<QString, QRectF> nodeRects;
    for (const auto& n : d.nodes)
        nodeRects[n.id] = nodeRect(n, chartR);

    // Edges (arrows)
    p.save();
    for (const auto& n : d.nodes) {
        for (const QString& eStr : n.edges) {
            QString tgtId = eStr.contains('|') ? eStr.section('|', 0, 0) : eStr;
            QString eType = eStr.contains('|') ? eStr.section('|', 1, 1) : "association";
            if (!nodeRects.contains(tgtId) || !nodeRects.contains(n.id)) continue;

            QRectF fr = nodeRects[n.id], tr = nodeRects[tgtId];
            QPointF from(fr.center().x(), fr.top());
            QPointF to(tr.center().x(), tr.bottom());

            bool dashed = (eType == "dependency");
            p.setPen(QPen(QColor(80,80,80), 1.5 * sc,
                          dashed ? Qt::DashLine : Qt::SolidLine));
            p.drawLine(from, to);

            // Arrow head
            float ah = 8.f * sc;
            QPointF dir = to - from;
            double len = qSqrt(dir.x()*dir.x() + dir.y()*dir.y());
            if (len < 1) continue;
            dir /= len;
            QPointF perp(-dir.y(), dir.x());

            if (eType == "inheritance") {
                // Open triangle
                QPointF tip = to;
                QPointF b1 = tip - dir * ah + perp * (ah * 0.5);
                QPointF b2 = tip - dir * ah - perp * (ah * 0.5);
                p.setPen(QPen(QColor(80,80,80), 1.5 * sc, Qt::SolidLine));
                p.setBrush(Qt::white);
                QPolygonF tri; tri << tip << b1 << b2;
                p.drawPolygon(tri);
            } else {
                // Simple arrow
                p.setPen(QPen(QColor(80,80,80), 1.5 * sc));
                p.setBrush(QColor(80,80,80));
                drawArrow(p, from, to, ah);
            }
        }
    }
    p.restore();

    // Class boxes
    for (const auto& n : d.nodes) {
        if (!nodeRects.contains(n.id)) continue;
        QRectF nr = nodeRects[n.id];

        // Parse: "Name|attributes|methods"
        QStringList parts = n.label.split('|');
        QString className = parts.value(0);
        QStringList attrs   = parts.size() > 1 ? parts[1].split('\n') : QStringList();
        QStringList methods = parts.size() > 2 ? parts[2].split('\n') : QStringList();

        // Compute section heights
        float lineH  = qMax(12.f, 15.f * sc);
        float headerH = lineH * 1.5f;
        float attrH   = attrs.isEmpty() ? lineH : attrs.size() * lineH + 6 * sc;
        float methH   = methods.isEmpty() ? lineH : methods.size() * lineH + 6 * sc;

        // If nr is too small, clamp content
        float totalH = headerH + attrH + methH;
        float scaleFac = totalH > nr.height() ? float(nr.height()) / totalH : 1.f;
        headerH *= scaleFac;
        attrH   *= scaleFac;
        lineH   *= scaleFac;

        QColor headerFill = n.color.isEmpty() ? QColor(78,121,167) : QColor(n.color);
        QColor bodyFill   = Qt::white;

        // Header (class name)
        QRectF hdr(nr.x(), nr.y(), nr.width(), headerH);
        p.fillRect(hdr, headerFill);
        p.save();
        p.setPen(Qt::white);
        p.setFont(scaledFont("Arial", 9, sc, true));
        p.drawText(hdr, Qt::AlignCenter, className);
        p.restore();

        // Attributes section
        QRectF atr(nr.x(), nr.y() + headerH, nr.width(), attrH);
        p.fillRect(atr, bodyFill);
        p.save();
        p.setPen(QColor(60,60,60));
        p.setFont(scaledFont("Arial", 7.5f, sc));
        for (int i = 0; i < attrs.size(); ++i) {
            p.drawText(QRectF(atr.x() + 4, atr.y() + i * lineH + 3,
                              atr.width() - 8, lineH),
                       Qt::AlignVCenter | Qt::AlignLeft, attrs[i]);
        }
        p.restore();

        // Methods section
        QRectF mth(nr.x(), nr.y() + headerH + attrH, nr.width(), methH * scaleFac);
        p.fillRect(mth, bodyFill.darker(103));
        p.save();
        p.setPen(QColor(60,60,60));
        p.setFont(scaledFont("Arial", 7.5f, sc));
        for (int i = 0; i < methods.size(); ++i) {
            p.drawText(QRectF(mth.x() + 4, mth.y() + i * lineH + 3,
                              mth.width() - 8, lineH),
                       Qt::AlignVCenter | Qt::AlignLeft, methods[i]);
        }
        p.restore();

        // Border around whole class
        p.save();
        p.setPen(QPen(headerFill.darker(140), 1.5 * sc));
        p.setBrush(Qt::NoBrush);
        p.drawRect(nr);
        p.drawLine(nr.x(), nr.y() + headerH,  nr.right(), nr.y() + headerH);
        p.drawLine(nr.x(), nr.y() + headerH + attrH, nr.right(), nr.y() + headerH + attrH);
        p.restore();
    }
}

// ─── Venn ─────────────────────────────────────────────────────────────────────

void ChartRenderer::paintVenn(QPainter& p, const QRectF& rect,
                               const ChartData& d, float sc) {
    if (d.vennCircles.isEmpty()) { drawPlaceholder(p, rect, "venn"); return; }

    float titleH = d.title.isEmpty() ? 0 : 22.f * sc;
    float pad = 12.f * sc;

    if (!d.title.isEmpty()) {
        p.save();
        p.setPen(QColor(40,40,40));
        p.setFont(scaledFont("Arial", 12, sc, true));
        p.drawText(QRectF(rect.x(), rect.y(), rect.width(), titleH + pad),
                   Qt::AlignHCenter | Qt::AlignTop, d.title);
        p.restore();
    }

    QRectF areaR(rect.x() + pad, rect.y() + titleH + pad,
                 rect.width() - 2*pad, rect.height() - titleH - 2*pad);

    // Draw circles (back-to-front)
    for (int i = 0; i < d.vennCircles.size(); ++i) {
        const ChartVennCircle& vc = d.vennCircles[i];
        double px = areaR.x() + vc.cx / 100.0 * areaR.width();
        double py = areaR.y() + vc.cy / 100.0 * areaR.height();
        double rx = vc.radius / 100.0 * qMin(areaR.width(), areaR.height());
        QColor c = vc.color.isEmpty() ? resolveColor("", i) : QColor(vc.color);
        QColor fill = c; fill.setAlphaF(vc.opacity);
        p.save();
        p.setPen(QPen(c.darker(130), 1.5 * sc));
        p.setBrush(fill);
        p.drawEllipse(QPointF(px, py), rx, rx);
        p.restore();
    }

    // Labels (on top)
    for (int i = 0; i < d.vennCircles.size(); ++i) {
        const ChartVennCircle& vc = d.vennCircles[i];
        double px = areaR.x() + vc.cx / 100.0 * areaR.width();
        double py = areaR.y() + vc.cy / 100.0 * areaR.height();
        double rx = vc.radius / 100.0 * qMin(areaR.width(), areaR.height());
        // Offset label towards the outer edge
        double offX = (vc.cx < 50) ? -rx * 0.45 : rx * 0.45;
        p.save();
        p.setPen(QColor(40,40,40));
        p.setFont(scaledFont("Arial", 10, sc, true));
        p.drawText(QRectF(px + offX - 50, py - 12, 100, 24),
                   Qt::AlignCenter, vc.label);
        p.restore();
    }
}

// ─── Type info ────────────────────────────────────────────────────────────────

QString ChartRenderer::typeName(const QString& type) {
    static const QMap<QString, QString> names = {
        {"bar",       "Bar Chart"},
        {"bar_h",     "Horizontal Bar Chart"},
        {"line",      "Line Chart"},
        {"area",      "Area Chart"},
        {"pie",       "Pie Chart"},
        {"donut",     "Donut Chart"},
        {"scatter",   "Scatter Chart"},
        {"flowchart", "Flowchart"},
        {"mindmap",   "Mind Map"},
        {"orgchart",  "Org Chart"},
        {"timeline",  "Timeline"},
        {"gantt",     "Gantt Chart"},
        {"uml",       "UML Class Diagram"},
        {"venn",      "Venn Diagram"},
    };
    return names.value(type, type);
}

QString ChartRenderer::typeIcon(const QString& type) {
    static const QMap<QString, QString> icons = {
        {"bar",       "▮"},
        {"bar_h",     "▬"},
        {"line",      "╱"},
        {"area",      "△"},
        {"pie",       "◑"},
        {"donut",     "◎"},
        {"scatter",   "⁚"},
        {"flowchart", "⬡"},
        {"mindmap",   "✦"},
        {"orgchart",  "⊤"},
        {"timeline",  "──"},
        {"gantt",     "▤"},
        {"uml",       "⊞"},
        {"venn",      "⊕"},
    };
    return icons.value(type, "□");
}
