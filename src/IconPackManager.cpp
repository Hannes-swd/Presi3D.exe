#include "IconPackManager.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFontDatabase>
#include <QFont>
#include <QFontMetricsF>
#include <QTransform>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QUrl>
#include <QWidget>
#include <QProgressDialog>
#include <QMessageBox>

namespace {
// The official Google "Material Icons" font (Apache-2.0) and its name ->
// codepoint mapping, hosted at a long-stable path in the upstream repo.
const char* kFontUrl =
    "https://raw.githubusercontent.com/google/material-design-icons/master/font/MaterialIcons-Regular.ttf";
const char* kCodepointsUrl =
    "https://raw.githubusercontent.com/google/material-design-icons/master/font/MaterialIcons-Regular.codepoints";

QVector<IconUtils::IconDef> parseCodepoints(const QByteArray& data) {
    QVector<IconUtils::IconDef> out;
    const QList<QByteArray> lines = data.split('\n');
    out.reserve(lines.size());
    for (const QByteArray& raw : lines) {
        QByteArray line = raw.trimmed();
        if (line.isEmpty()) continue;
        int sp = line.indexOf(' ');
        if (sp < 0) continue;
        QString name = QString::fromUtf8(line.left(sp));
        bool ok = false;
        quint32 cp = line.mid(sp + 1).trimmed().toUInt(&ok, 16);
        if (!ok || cp == 0) continue;

        QStringList words = QString(name).replace('_', ' ').split(' ', Qt::SkipEmptyParts);
        for (QString& w : words)
            if (!w.isEmpty()) w[0] = w[0].toUpper();

        IconUtils::IconDef def;
        def.id         = name;
        def.label      = words.join(' ');
        def.category   = "Material Icons";
        def.codepoint  = cp;
        out << def;
    }
    return out;
}
} // namespace

IconPackManager& IconPackManager::instance() {
    static IconPackManager inst;
    return inst;
}

IconPackManager::IconPackManager(QObject* parent) : QObject(parent) {
    m_net = new QNetworkAccessManager(this);
    loadFromDisk();
}

QString IconPackManager::cacheDir() const {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath("iconpack");
}
QString IconPackManager::fontPath() const {
    return QDir(cacheDir()).filePath("MaterialIcons-Regular.ttf");
}
QString IconPackManager::codepointsPath() const {
    return QDir(cacheDir()).filePath("MaterialIcons-Regular.codepoints");
}

void IconPackManager::loadFromDisk() {
    unloadFont();
    m_defs.clear();
    m_byId.clear();
    m_installed = false;

    QFile fCp(codepointsPath());
    if (!QFile::exists(fontPath()) || !fCp.open(QIODevice::ReadOnly)) return;
    const QByteArray cpData = fCp.readAll();
    fCp.close();

    m_fontId = QFontDatabase::addApplicationFont(fontPath());
    if (m_fontId < 0) return;
    const QStringList families = QFontDatabase::applicationFontFamilies(m_fontId);
    if (families.isEmpty()) return;
    m_fontFamily = families.first();

    m_defs = parseCodepoints(cpData);
    for (const auto& d : m_defs) m_byId.insert(d.id, d);
    m_installed = !m_defs.isEmpty();
}

void IconPackManager::unloadFont() {
    if (m_fontId >= 0) {
        QFontDatabase::removeApplicationFont(m_fontId);
        m_fontId = -1;
    }
    m_fontFamily.clear();
}

QPainterPath IconPackManager::pathFor(const QString& id, const QRectF& rect) const {
    if (!m_installed) return QPainterPath();
    auto it = m_byId.constFind(id);
    if (it == m_byId.constEnd()) return QPainterPath();

    QString ch = QString(QChar(static_cast<char16_t>(it->codepoint)));
    QFont f(m_fontFamily);
    f.setPixelSize(200);
    QFontMetricsF fm(f);
    QRectF br = fm.tightBoundingRect(ch);
    if (br.width() < 1.0 || br.height() < 1.0) return QPainterPath();

    double scale = qMin(rect.width() / br.width(), rect.height() / br.height());
    QPainterPath glyph;
    glyph.addText(0, 0, f, ch);
    QTransform t;
    t.translate(rect.center().x(), rect.center().y());
    t.scale(scale, scale);
    t.translate(-br.center().x(), -br.center().y());
    QPainterPath mapped = t.map(glyph);
    mapped.setFillRule(Qt::WindingFill);
    return mapped;
}

void IconPackManager::install() {
    if (m_busy) return;
    m_busy = true;
    fetchFont();
}

void IconPackManager::fetchFont() {
    QNetworkRequest cpReq{QUrl(QString::fromLatin1(kCodepointsUrl))};
    cpReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* cpReply = m_net->get(cpReq);
    connect(cpReply, &QNetworkReply::finished, this, [this, cpReply]() {
        cpReply->deleteLater();
        if (cpReply->error() != QNetworkReply::NoError) {
            m_busy = false;
            emit installFinished(false, cpReply->errorString());
            return;
        }
        m_pendingCodepointsData = cpReply->readAll();

        QNetworkRequest fontReq{QUrl(QString::fromLatin1(kFontUrl))};
        fontReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* fontReply = m_net->get(fontReq);
        connect(fontReply, &QNetworkReply::downloadProgress, this, &IconPackManager::installProgress);
        connect(fontReply, &QNetworkReply::finished, this, [this, fontReply]() {
            fontReply->deleteLater();
            m_busy = false;
            if (fontReply->error() != QNetworkReply::NoError) {
                emit installFinished(false, fontReply->errorString());
                return;
            }
            const QByteArray fontData = fontReply->readAll();

            QDir().mkpath(cacheDir());
            QFile fFont(fontPath());
            QFile fCp(codepointsPath());
            if (!fFont.open(QIODevice::WriteOnly) || !fCp.open(QIODevice::WriteOnly)) {
                emit installFinished(false, tr("Could not write to %1").arg(cacheDir()));
                return;
            }
            fFont.write(fontData);
            fCp.write(m_pendingCodepointsData);
            fFont.close();
            fCp.close();
            m_pendingCodepointsData.clear();

            loadFromDisk();
            emit installFinished(m_installed, m_installed ? QString() : tr("Downloaded files could not be parsed."));
        });
    });
}

void IconPackManager::uninstall() {
    unloadFont();
    QFile::remove(fontPath());
    QFile::remove(codepointsPath());
    m_defs.clear();
    m_byId.clear();
    m_installed = false;
    emit uninstalled();
}

void IconPackManager::installInteractive(QWidget* parent) {
    if (m_busy) return;
    if (m_installed) {
        QMessageBox::information(parent, tr("Icon Package"),
            tr("The Material Icons package is already installed (%1 icons).").arg(count()));
        return;
    }
    auto res = QMessageBox::question(parent, tr("Download Icon Package"),
        tr("Download the official Google Material Icons set (~2100 icons, "
           "about 350 KB) so the full library is available in the icon picker?\n\n"
           "This downloads once from raw.githubusercontent.com and is cached locally."));
    if (res != QMessageBox::Yes) return;

    auto* progress = new QProgressDialog(tr("Downloading Material Icons\xE2\x80\xA6"), QString(), 0, 100, parent);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setCancelButton(nullptr);
    progress->setValue(0);
    progress->show();

    connect(this, &IconPackManager::installProgress, progress,
        [progress](qint64 received, qint64 total) {
            if (total > 0) progress->setValue(int(received * 100 / total));
        });
    connect(this, &IconPackManager::installFinished, progress,
        [this, progress](bool ok, const QString& error) {
            QWidget* parentW = progress->parentWidget();
            progress->close();
            progress->deleteLater();
            if (!ok)
                QMessageBox::warning(parentW, tr("Download Failed"),
                    tr("Could not download the Material Icons package:\n%1").arg(error));
            else
                QMessageBox::information(parentW, tr("Icons Installed"),
                    tr("%1 icons are now available in the icon picker.").arg(count()));
        });

    install();
}

void IconPackManager::uninstallInteractive(QWidget* parent) {
    if (!m_installed) {
        QMessageBox::information(parent, tr("Icon Package"),
            tr("No icon package is currently installed."));
        return;
    }
    auto res = QMessageBox::question(parent, tr("Remove Icon Package"),
        tr("Remove the downloaded Material Icons package (%1 icons)?\n"
           "Icons already placed in a presentation will show a placeholder "
           "frame until you reinstall the package.").arg(count()));
    if (res != QMessageBox::Yes) return;
    uninstall();
    QMessageBox::information(parent, tr("Removed"), tr("The icon package has been removed."));
}
