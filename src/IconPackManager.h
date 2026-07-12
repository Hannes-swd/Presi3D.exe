#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QPainterPath>
#include <QRectF>
#include "IconUtils.h"

class QNetworkAccessManager;
class QWidget;

// Downloads and manages the official Google "Material Icons" font
// (Apache-2.0) on demand, caching it locally so the full ~2100-icon set is
// available offline after a one-time install. See IconUtils for the
// rendering API consumed by the rest of the app; this class is the backing
// store IconUtils delegates to.
class IconPackManager : public QObject {
    Q_OBJECT
public:
    static IconPackManager& instance();

    bool isInstalled() const { return m_installed; }
    int  count() const { return m_defs.size(); }

    const QVector<IconUtils::IconDef>& icons() const { return m_defs; }
    QPainterPath pathFor(const QString& id, const QRectF& rect) const;

    // Fire-and-forget entry points with built-in progress/result dialogs.
    void installInteractive(QWidget* parent);
    void uninstallInteractive(QWidget* parent);

signals:
    void installProgress(qint64 received, qint64 total);
    void installFinished(bool success, const QString& error);
    void uninstalled();

private:
    explicit IconPackManager(QObject* parent = nullptr);

    void install();
    void uninstall();
    void loadFromDisk();
    void unloadFont();
    void fetchFont();

    QString cacheDir() const;
    QString fontPath() const;
    QString codepointsPath() const;

    QNetworkAccessManager* m_net;
    bool    m_installed = false;
    bool    m_busy      = false;
    int     m_fontId    = -1;
    QString m_fontFamily;
    QByteArray m_pendingCodepointsData;

    QVector<IconUtils::IconDef>   m_defs;
    QHash<QString, IconUtils::IconDef> m_byId;
};
