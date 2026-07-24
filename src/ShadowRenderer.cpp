#include "ShadowRenderer.h"
#include "ShapeUtils.h"
#include "IconUtils.h"
#include "TextLayoutUtils.h"
#include <QPainter>
#include <QPainterPath>
#include <QTextLayout>
#include <QVector>
#include <QtMath>
#include <algorithm>

namespace ShadowRenderer {

int padding(float blurPx, float spreadPx) {
    return int(qCeil(qMax(0.f, spreadPx) + qMax(0.f, blurPx) * 1.5f)) + 2;
}

// ── Alpha-channel raster helpers ────────────────────────────────────────────

static QVector<quint8> extractAlpha(const QImage& img) {
    const int w = img.width(), h = img.height();
    QVector<quint8> out(w * h);
    for (int y = 0; y < h; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        quint8* outLine = out.data() + y * w;
        for (int x = 0; x < w; ++x)
            outLine[x] = quint8(qAlpha(line[x]));
    }
    return out;
}

// Separable box blur (horizontal pass then vertical pass) via sliding window
// sum — O(w*h) regardless of radius. Repeated 3x by blurAlpha() below
// approximates a Gaussian blur closely enough for a visual shadow.
static void boxBlurPass(QVector<quint8>& data, int w, int h, int radius) {
    if (radius < 1 || w < 1 || h < 1) return;
    QVector<quint8> tmp(data.size());
    for (int y = 0; y < h; ++y) {
        int row = y * w;
        int sum = 0;
        for (int x = -radius; x <= radius; ++x)
            sum += data[row + qBound(0, x, w - 1)];
        for (int x = 0; x < w; ++x) {
            tmp[row + x] = quint8(sum / (2 * radius + 1));
            int addX = qBound(0, x + radius + 1, w - 1);
            int subX = qBound(0, x - radius, w - 1);
            sum += data[row + addX] - data[row + subX];
        }
    }
    QVector<quint8> out(data.size());
    for (int x = 0; x < w; ++x) {
        int sum = 0;
        for (int y = -radius; y <= radius; ++y)
            sum += tmp[qBound(0, y, h - 1) * w + x];
        for (int y = 0; y < h; ++y) {
            out[y * w + x] = quint8(sum / (2 * radius + 1));
            int addY = qBound(0, y + radius + 1, h - 1);
            int subY = qBound(0, y - radius, h - 1);
            sum += tmp[addY * w + x] - tmp[subY * w + x];
        }
    }
    data = out;
}

static void blurAlpha(QVector<quint8>& alpha, int w, int h, float blurPx) {
    if (blurPx <= 0.01f) return;
    int radius = qMax(1, int(qRound(blurPx / 2.f)));
    for (int i = 0; i < 3; ++i)
        boxBlurPass(alpha, w, h, radius);
}

// Grows the silhouette outward by ~spreadPx: blur the mask by that radius,
// then hard-threshold back to a crisp (but now larger) edge. Works uniformly
// for any content type since it operates purely on the rasterized alpha.
static void spreadAlpha(QVector<quint8>& alpha, int w, int h, float spreadPx) {
    if (spreadPx <= 0.01f) return;
    int radius = qMax(1, int(qRound(spreadPx)));
    boxBlurPass(alpha, w, h, radius);
    for (quint8& a : alpha) a = (a >= 96) ? 255 : 0;
}

static QImage tintFromAlpha(const QVector<quint8>& alpha, int w, int h, const QColor& color) {
    QImage result(w, h, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    const int r = color.red(), g = color.green(), b = color.blue();
    const float colorAlphaF = float(color.alphaF());
    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        const quint8* aLine = alpha.constData() + y * w;
        for (int x = 0; x < w; ++x) {
            int a = int(aLine[x] * colorAlphaF);
            if (a <= 0) continue;
            line[x] = qRgba(r * a / 255, g * a / 255, b * a / 255, a);
        }
    }
    return result;
}

QImage render(const QSize& localSize, const SilhouettePainter& paintSilhouette,
              float blurPx, float spreadPx, const QColor& shadowColor) {
    int pad = padding(blurPx, spreadPx);
    QSize fullSize = (localSize + QSize(pad * 2, pad * 2)).expandedTo(QSize(1, 1));
    if (!paintSilhouette || !shadowColor.isValid())
        return QImage();

    QImage silhouette(fullSize, QImage::Format_ARGB32_Premultiplied);
    silhouette.fill(Qt::transparent);
    {
        QPainter p(&silhouette);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(pad, pad);
        paintSilhouette(p);
    }

    QVector<quint8> alpha = extractAlpha(silhouette);
    spreadAlpha(alpha, fullSize.width(), fullSize.height(), spreadPx);
    blurAlpha(alpha, fullSize.width(), fullSize.height(), blurPx);
    return tintFromAlpha(alpha, fullSize.width(), fullSize.height(), shadowColor);
}

// ── Silhouette construction per element type ────────────────────────────────

static void paintTextSilhouette(QPainter& p, const SlideElement& e, const QRectF& localRect) {
    if (localRect.width() < 1.0 || localRect.height() < 1.0) return;
    QString displayText = e.content;
    if (e.listStyle != SlideElement::NoList) {
        QStringList lines = displayText.split('\n');
        QStringList fmt;
        for (int ln = 0; ln < lines.size(); ++ln)
            fmt << (e.listStyle == SlideElement::Bullets
                    ? QString("\xE2\x80\xA2 ") + lines[ln]
                    : QString::number(ln + 1) + ". " + lines[ln]);
        displayText = fmt.join('\n');
    }
    float scaleY = float(localRect.height()) / qMax(1.f, e.height);
    QFont font = elemFont(e, scaleY);
    font.setUnderline(e.underline || !e.hyperlink.trimmed().isEmpty());
    font.setStrikeOut(e.strikethrough);

    QTextLayout layout(layoutText(displayText), font);
    buildLayout(layout, e, font, float(localRect.width()));
    float vOff = textVOff(layout, float(localRect.height()), e.verticalAlignment);
    p.setPen(Qt::black);
    p.setClipRect(localRect, Qt::IntersectClip);
    layout.draw(&p, localRect.topLeft() + QPointF(0, vOff));
}

static void paintShapeSilhouette(QPainter& p, const SlideElement& e, const QRectF& localRect) {
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    if (e.content == "line") {
        QPen pen(Qt::black, qMax(1.f, e.borderWidth));
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.drawLine(localRect.topLeft(), localRect.bottomRight());
    } else if (e.content == "rect") {
        float rx = e.cornerRadius * float(localRect.width())  / qMax(1.f, e.width);
        float ry = e.cornerRadius * float(localRect.height()) / qMax(1.f, e.height);
        if (rx > 0 || ry > 0) p.drawRoundedRect(localRect, rx, ry);
        else                   p.drawRect(localRect);
    } else {
        p.drawPath(ShapeUtils::shapeToPath(e.content, localRect, e.customPathData));
    }
}

static void paintIconSilhouette(QPainter& p, const SlideElement& e, const QRectF& localRect) {
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    p.drawPath(IconUtils::iconToPath(e.content, localRect));
}

static void paintImageSilhouette(QPainter& p, const SlideElement& e, const QRectF& localRect) {
    if (!e.content.isEmpty()) {
        QImage img(e.content);
        if (!img.isNull()) {
            p.drawImage(localRect.toRect(), img);
            return;
        }
    }
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    p.drawRect(localRect);
}

static void paintFallbackSilhouette(QPainter& p, const SlideElement& e, const QRectF& localRect) {
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    if (e.cornerRadius > 0)
        p.drawRoundedRect(localRect, e.cornerRadius, e.cornerRadius);
    else
        p.drawRect(localRect);
}

SilhouettePainter buildSilhouettePainter(const SlideElement& e, const QRectF& localRect) {
    switch (e.type) {
    case SlideElement::Text:
        return [&e, localRect](QPainter& p) { paintTextSilhouette(p, e, localRect); };
    case SlideElement::Shape:
        return [&e, localRect](QPainter& p) { paintShapeSilhouette(p, e, localRect); };
    case SlideElement::Icon:
        return [&e, localRect](QPainter& p) { paintIconSilhouette(p, e, localRect); };
    case SlideElement::Image:
        return [&e, localRect](QPainter& p) { paintImageSilhouette(p, e, localRect); };
    default:
        return [&e, localRect](QPainter& p) { paintFallbackSilhouette(p, e, localRect); };
    }
}

} // namespace ShadowRenderer
