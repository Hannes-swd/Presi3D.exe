#pragma once
#include <QObject>
#include <QString>

class QNetworkAccessManager;

// Checks the GitHub "latest release" endpoint for a newer Presi3D version and,
// if the user opts in, downloads the installer asset to a temp file so the
// caller can launch it.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);

    void checkForUpdates();
    void downloadAndInstall(const QString& downloadUrl);

signals:
    void updateAvailable(const QString& version, const QString& downloadUrl);
    void upToDate();
    void checkFailed(const QString& error);

    void downloadProgress(qint64 received, qint64 total);
    void downloadFailed(const QString& error);
    void installerReady(const QString& installerPath);

private:
    QNetworkAccessManager* m_net;
};
