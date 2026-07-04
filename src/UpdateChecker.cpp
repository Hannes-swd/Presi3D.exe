#include "UpdateChecker.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QUrl>

namespace {
const char* kReleasesApiUrl = "https://api.github.com/repos/Hannes-swd/Presi3D.exe/releases/latest";
const char* kAssetName      = "Presi3DSetup.exe";

// Returns true if `latest` (e.g. "v1.2") denotes a newer version than `current` (e.g. "1.1").
bool isNewerVersion(QString latest, QString current) {
    if (latest.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))  latest.remove(0, 1);
    if (current.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) current.remove(0, 1);

    const QStringList a = latest.split(QLatin1Char('.'));
    const QStringList b = current.split(QLatin1Char('.'));
    for (int i = 0; i < qMax(a.size(), b.size()); ++i) {
        const int av = i < a.size() ? a[i].toInt() : 0;
        const int bv = i < b.size() ? b[i].toInt() : 0;
        if (av != bv) return av > bv;
    }
    return false;
}
} // namespace

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent) {
    m_net = new QNetworkAccessManager(this);
}

void UpdateChecker::checkForUpdates() {
    QNetworkRequest req{QUrl(QString::fromLatin1(kReleasesApiUrl))};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Presi3D-UpdateChecker"));
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply* reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(reply->errorString());
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = obj.value(QStringLiteral("tag_name")).toString();
        if (tag.isEmpty()) {
            emit checkFailed(tr("Unexpected response from GitHub."));
            return;
        }

        if (!isNewerVersion(tag, QStringLiteral(APP_VERSION))) {
            emit upToDate();
            return;
        }

        QString downloadUrl;
        for (const QJsonValue& v : obj.value(QStringLiteral("assets")).toArray()) {
            const QJsonObject asset = v.toObject();
            if (asset.value(QStringLiteral("name")).toString() == QLatin1String(kAssetName)) {
                downloadUrl = asset.value(QStringLiteral("browser_download_url")).toString();
                break;
            }
        }
        if (downloadUrl.isEmpty()) {
            emit checkFailed(tr("Release %1 has no installer asset.").arg(tag));
            return;
        }
        emit updateAvailable(tag, downloadUrl);
    });
}

void UpdateChecker::downloadAndInstall(const QString& downloadUrl) {
    QNetworkRequest req{QUrl(downloadUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Presi3D-UpdateChecker"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_net->get(req);
    connect(reply, &QNetworkReply::downloadProgress, this, &UpdateChecker::downloadProgress);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit downloadFailed(reply->errorString());
            return;
        }

        const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                  .filePath(QString::fromLatin1(kAssetName));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            emit downloadFailed(tr("Could not write to %1").arg(path));
            return;
        }
        file.write(reply->readAll());
        file.close();

        emit installerReady(path);
    });
}
