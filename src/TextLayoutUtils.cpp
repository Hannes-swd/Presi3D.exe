#include "TextLayoutUtils.h"

QString layoutText(const QString& s) {
    QString r = s;
    r.replace(QLatin1Char('\n'), QChar::LineSeparator);
    return r;
}

QFont elemFont(const SlideElement& e, float scaleY) {
    QFont f(e.fontFamily, qMax(6, int(e.fontSize * scaleY)));
    f.setBold(e.bold); f.setItalic(e.italic);
    return f;
}

void buildLayout(QTextLayout& layout, const SlideElement& e,
                  const QFont& font, float width,
                  const QString& alignOverride) {
    layout.setFont(font);
    QTextOption opt;
    const QString& align = alignOverride.isEmpty() ? e.textAlignment : alignOverride;
    if      (align == "center") opt.setAlignment(Qt::AlignHCenter);
    else if (align == "right")  opt.setAlignment(Qt::AlignRight);
    else                        opt.setAlignment(Qt::AlignLeft);
    opt.setWrapMode(QTextOption::WordWrap);
    layout.setTextOption(opt);
    layout.beginLayout();
    float y = 0;
    for (;;) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(width);
        line.setPosition(QPointF(0, y));
        y += line.height();
    }
    layout.endLayout();
}

float textVOff(const QTextLayout& layout, float elemH, const QString& vAlign) {
    if (layout.lineCount() == 0 || vAlign.isEmpty() || vAlign == "top") return 0;
    QTextLine last = layout.lineAt(layout.lineCount() - 1);
    float totalH = float(last.y() + last.height());
    if (vAlign == "middle") return qMax(0.f, (elemH - totalH) * 0.5f);
    if (vAlign == "bottom") return qMax(0.f, elemH - totalH);
    return 0;
}
