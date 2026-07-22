#include "ShapeBoolean.h"
#include "models/DataModel.h"
#include "ShapeUtils.h"
#include <QFont>
#include <QTextLayout>
#include <QTextOption>
#include <QTransform>
#include <algorithm>

namespace {

// QTextLayout treats only QChar::LineSeparator (not '\n') as a forced line
// break — mirrors SlideEditor2D.cpp's layoutText(), which every text layout
// in this codebase relies on for consistent wrapping.
QString layoutText(const QString& s) {
    QString r = s;
    r.replace(QLatin1Char('\n'), QChar::LineSeparator);
    return r;
}

// Builds a Text element's glyph outline in the element's own local
// coordinates (top-left of the element == origin), mirroring the word-wrap/
// alignment layout SlideEditor2D.cpp uses to paint it, so the cut result
// matches what's on screen.
QPainterPath textElementToLocalPath(const SlideElement& e) {
    QFont font(e.fontFamily, qMax(6, int(e.fontSize)));
    font.setBold(e.bold);
    font.setItalic(e.italic);

    const QString text = layoutText(e.content);
    QTextLayout layout(text, font);
    QTextOption opt;
    if      (e.textAlignment == "center") opt.setAlignment(Qt::AlignHCenter);
    else if (e.textAlignment == "right")  opt.setAlignment(Qt::AlignRight);
    else                                  opt.setAlignment(Qt::AlignLeft);
    opt.setWrapMode(QTextOption::WordWrap);
    layout.setTextOption(opt);

    QVector<QTextLine> lines;
    layout.beginLayout();
    float y = 0;
    for (;;) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(e.width);
        line.setPosition(QPointF(0, y));
        y += line.height();
        lines << line;
    }
    layout.endLayout();

    float vOff = 0.f;
    if (!lines.isEmpty() && !e.verticalAlignment.isEmpty() && e.verticalAlignment != "top") {
        const QTextLine& last = lines.last();
        float totalH = float(last.y() + last.height());
        if (e.verticalAlignment == "middle") vOff = std::max(0.f, (e.height - totalH) * 0.5f);
        else if (e.verticalAlignment == "bottom") vOff = std::max(0.f, e.height - totalH);
    }

    QPainterPath path;
    path.setFillRule(Qt::WindingFill); // overlapping glyphs (e.g. script fonts) must stay filled
    for (const QTextLine& line : lines) {
        const QString lineText = text.mid(line.textStart(), line.textLength());
        if (lineText.trimmed().isEmpty()) continue;
        path.addText(QPointF(line.x(), vOff + line.y() + line.ascent()), font, lineText);
    }
    return path;
}

} // namespace

QPainterPath ShapeBoolean::elementToPath(const SlideElement& e) {
    QPainterPath path;
    if (e.type == SlideElement::Shape) {
        if (e.content == "line") return QPainterPath(); // no fillable area to combine
        path = ShapeUtils::shapeToPath(e.content, QRectF(e.x, e.y, e.width, e.height), e.customPathData);
    } else if (e.type == SlideElement::Text) {
        path = textElementToLocalPath(e);
        path.translate(e.x, e.y);
    } else {
        return QPainterPath();
    }

    if (e.rotation != 0.f) {
        const QPointF center(e.x + e.width * 0.5, e.y + e.height * 0.5);
        QTransform t;
        t.translate(center.x(), center.y());
        t.rotate(double(e.rotation));
        t.translate(-center.x(), -center.y());
        path = t.map(path);
    }
    return path;
}

QPainterPath ShapeBoolean::combine(const QVector<const SlideElement*>& backToFront, Op op) {
    QVector<QPainterPath> paths;
    paths.reserve(backToFront.size());
    for (const SlideElement* e : backToFront) {
        QPainterPath p = elementToPath(*e);
        if (!p.isEmpty()) paths << p;
    }
    if (paths.size() < 2) return QPainterPath();

    QPainterPath acc = paths[0];
    for (int i = 1; i < paths.size(); ++i) {
        const QPainterPath& next = paths[i];
        switch (op) {
        case Op::Union:     acc = acc.united(next); break;
        case Op::Subtract:  acc = acc.subtracted(next); break;
        case Op::Intersect: acc = acc.intersected(next); break;
        case Op::Exclude:   acc = acc.united(next).subtracted(acc.intersected(next)); break;
        }
    }
    return acc;
}
