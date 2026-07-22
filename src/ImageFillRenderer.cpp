#include "ImageFillRenderer.h"
#include "ShapeUtils.h"
#include <QPainter>
#include <QPainterPath>
#include <QCache>
#include <algorithm>

namespace ImageFillRenderer {

QImage renderImageFill(const QString& shapeType, const QSize& pixelSize,
                        const QString& imagePath, float offsetX, float offsetY, float scale,
                        const QSizeF& cornerRadiusPx, const QString& customPathData) {
    QImage blank(pixelSize.expandedTo(QSize(1, 1)), QImage::Format_ARGB32_Premultiplied);
    blank.fill(Qt::transparent);
    if (imagePath.isEmpty() || pixelSize.width() < 1 || pixelSize.height() < 1) return blank;

    static QCache<QString, QImage> resultCache(32);
    QString key = QString("%1|%2x%3|%4,%5|%6|%7,%8,%9|%10")
                      .arg(shapeType).arg(pixelSize.width()).arg(pixelSize.height())
                      .arg(cornerRadiusPx.width()).arg(cornerRadiusPx.height())
                      .arg(imagePath).arg(offsetX).arg(offsetY).arg(scale)
                      .arg(customPathData);
    if (QImage* cached = resultCache.object(key))
        return *cached;

    // Source images are cached separately (and un-keyed by the fill
    // parameters) so panning/zooming a texture doesn't re-read the file from
    // disk on every frame.
    static QCache<QString, QImage> sourceCache(16);
    QImage* src = sourceCache.object(imagePath);
    if (!src) {
        QImage loaded(imagePath);
        if (loaded.isNull()) return blank;
        src = new QImage(loaded.convertToFormat(QImage::Format_ARGB32_Premultiplied));
        sourceCache.insert(imagePath, src);
    }
    if (src->isNull() || src->width() < 1 || src->height() < 1) return blank;

    const double coverScale = std::max(double(pixelSize.width()) / src->width(),
                                        double(pixelSize.height()) / src->height());
    const double effScale = coverScale * std::max(0.01, double(scale));
    const double scaledW = src->width() * effScale;
    const double scaledH = src->height() * effScale;
    const double slackX = std::max(0.0, scaledW - pixelSize.width());
    const double slackY = std::max(0.0, scaledH - pixelSize.height());
    const double ox = std::clamp(double(offsetX), -1.0, 1.0);
    const double oy = std::clamp(double(offsetY), -1.0, 1.0);
    const double left = (pixelSize.width()  - scaledW) / 2.0 + ox * (slackX / 2.0);
    const double top  = (pixelSize.height() - scaledH) / 2.0 + oy * (slackY / 2.0);

    QImage img(pixelSize, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(QRectF(left, top, scaledW, scaledH), *src);
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

    resultCache.insert(key, new QImage(img));
    return img;
}

} // namespace ImageFillRenderer
