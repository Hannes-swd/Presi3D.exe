#include "IconUtils.h"
#include "IconPackManager.h"

const QVector<IconUtils::IconDef>& IconUtils::allIcons()
{
    return IconPackManager::instance().icons();
}

QPainterPath IconUtils::iconToPath(const QString& id, const QRectF& rect)
{
    QPainterPath p = IconPackManager::instance().pathFor(id, rect);
    if (!p.isEmpty()) return p;

    // No pack installed (or unknown id): draw a faint placeholder frame so an
    // already-placed icon element stays visible and selectable rather than
    // vanishing outright.
    double inset = qMin(rect.width(), rect.height()) * 0.12;
    QRectF outerR = rect.adjusted(inset, inset, -inset, -inset);
    QRectF innerR = outerR.adjusted(2, 2, -2, -2);
    QPainterPath ph;
    ph.addRoundedRect(outerR, 2, 2);
    if (innerR.width() > 0 && innerR.height() > 0) {
        QPainterPath hole;
        hole.addRoundedRect(innerR, 1, 1);
        ph.addPath(hole.toReversed());
    }
    ph.setFillRule(Qt::WindingFill);
    return ph;
}
