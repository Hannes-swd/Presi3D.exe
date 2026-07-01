#pragma once
#include <QString>
#include <QSizeF>
#include <QRectF>
#include <QColor>

class QPainter;

// Lightweight LaTeX-subset math typesetter (QPainter-based, no external deps).
// Supports: super/subscripts (^, _), \frac{}{}, \sqrt{}, \text{}/\mathrm{},
// \left \right delimiters, common symbols (\pm, \times, \leq, ...) and Greek letters.
// Not a full LaTeX engine — unsupported commands fall back to showing their name.
class LatexRenderer {
public:
    // Paints latex centered within rect, shrinking the font to fit if needed.
    static void paint(QPainter& painter, const QRectF& rect, const QString& latex,
                       const QString& fontFamily, qreal fontPx, const QColor& color);

    // Natural (unbounded) size of the rendered formula at the given font size.
    static QSizeF measure(const QString& latex, const QString& fontFamily, qreal fontPx);
};
