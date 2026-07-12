#pragma once
#include <QString>
#include <QVector>
#include <QPainterPath>
#include <QRectF>

namespace IconUtils {

struct IconDef {
    QString id;
    QString label;
    QString category;
    quint32 codepoint = 0; // Private-Use-Area codepoint in the Material Icons font
};

const QVector<IconDef>& allIcons();

// Returns a filled QPainterPath for the given icon id, fitted inside rect
// (icons are authored on a 24x24 grid and scaled/positioned to fit).
QPainterPath iconToPath(const QString& id, const QRectF& rect);

} // namespace IconUtils
