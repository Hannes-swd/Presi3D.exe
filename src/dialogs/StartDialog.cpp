#include "StartDialog.h"
#include "UpdateChecker.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QFrame>
#include <QIcon>
#include <QSize>
#include <QMessageBox>
#include <QApplication>
#include <QProcess>
#include <QTimer>
#include <QMenu>
#include <QInputDialog>
#include <QLineEdit>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>

static constexpr int kMaxRecent = 10;
static const char* kSettingsKey = "recentProjects";

// ── Static helpers ────────────────────────────────────────────────────────────

void StartDialog::addRecentProject(const QString& path) {
    if (path.isEmpty()) return;
    QSettings s;
    QStringList list = s.value(kSettingsKey).toStringList();
    list.removeAll(path);
    list.prepend(path);
    while (list.size() > kMaxRecent) list.removeLast();
    s.setValue(kSettingsKey, list);
}

void StartDialog::removeRecentProject(const QString& path) {
    if (path.isEmpty()) return;
    QSettings s;
    QStringList list = s.value(kSettingsKey).toStringList();
    list.removeAll(path);
    s.setValue(kSettingsKey, list);
}

QStringList StartDialog::recentProjects() {
    QSettings s;
    return s.value(kSettingsKey).toStringList();
}

// ── Constructor ───────────────────────────────────────────────────────────────

StartDialog::StartDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Presi 3D");
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setMinimumSize(400, 360);
    buildUi();
    loadRecentList();

    m_updateChecker = new UpdateChecker(this);
    connect(m_updateChecker, &UpdateChecker::updateAvailable, this, &StartDialog::onUpdateAvailable);
    connect(m_updateChecker, &UpdateChecker::upToDate,        this, &StartDialog::onUpToDate);
    connect(m_updateChecker, &UpdateChecker::checkFailed,     this, &StartDialog::onUpdateCheckFailed);
    connect(m_updateChecker, &UpdateChecker::installerReady,  this, &StartDialog::onInstallerReady);
    connect(m_updateChecker, &UpdateChecker::downloadFailed,  this, &StartDialog::onUpdateDownloadFailed);
    connect(m_updateChecker, &UpdateChecker::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0)
            m_updateBtn->setText(QString("Downloading update... %1%").arg(received * 100 / total));
    });

    QTimer::singleShot(1500, this, [this]() { m_updateChecker->checkForUpdates(); });
}

void StartDialog::buildUi() {
    auto* vbox = new QVBoxLayout(this);
    vbox->setSpacing(10);
    vbox->setContentsMargins(16, 16, 16, 16);

    auto* title = new QLabel("Presi 3D", this);
    title->setStyleSheet("font-size:22px; font-weight:bold; color:#111827; padding: 4px 0 8px 0;");
    vbox->addWidget(title);

    // Action buttons
    auto* newBtn  = new QPushButton("Create New Project", this);
    auto* openBtn = new QPushButton("Open Project...", this);
    newBtn->setFixedHeight(34);
    openBtn->setFixedHeight(34);
    newBtn->setStyleSheet(
        "QPushButton { background:#2563eb; color:white; border:none; border-radius:4px; padding:4px 16px; font-size:12px; }"
        "QPushButton:hover { background:#1d4ed8; }");
    openBtn->setStyleSheet(
        "QPushButton { background:#ffffff; color:#374151; border:1px solid #d1d5db; border-radius:4px; padding:4px 16px; font-size:12px; }"
        "QPushButton:hover { background:#f9fafb; border-color:#9ca3af; }");

    vbox->addWidget(newBtn);
    vbox->addWidget(openBtn);

    // Divider
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    vbox->addWidget(line);

    auto* recentLabel = new QLabel("Recently Edited:", this);
    recentLabel->setStyleSheet("color:#374151; font-size:11px; font-weight:bold;");
    vbox->addWidget(recentLabel);

    m_recentList = new QListWidget(this);
    m_recentList->setContextMenuPolicy(Qt::CustomContextMenu);
    vbox->addWidget(m_recentList, 1);

    // Bottom row
    auto* btnRow = new QHBoxLayout;
    m_updateBtn = new QPushButton(QIcon(":/icons/download.svg"), "Check for Updates", this);
    m_updateBtn->setIconSize(QSize(14, 14));
    m_updateBtn->setFlat(true);
    m_updateBtn->setCursor(Qt::PointingHandCursor);
    connect(m_updateBtn, &QPushButton::clicked, this, &StartDialog::onCheckUpdateClicked);

    m_openRecentBtn = new QPushButton("Open", this);
    m_openRecentBtn->setEnabled(false);
    auto* cancelBtn = new QPushButton("Cancel", this);
    btnRow->addWidget(m_updateBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_openRecentBtn);
    btnRow->addWidget(cancelBtn);
    vbox->addLayout(btnRow);

    refreshUpdateButton();

    connect(newBtn,          &QPushButton::clicked,         this, &StartDialog::onNewProject);
    connect(openBtn,         &QPushButton::clicked,         this, &StartDialog::onOpenProject);
    connect(m_openRecentBtn, &QPushButton::clicked,         this, &StartDialog::onRecentDoubleClicked);
    connect(m_recentList,    &QListWidget::itemDoubleClicked, this, &StartDialog::onRecentDoubleClicked);
    connect(m_recentList,    &QListWidget::itemClicked,       this, &StartDialog::onRecentClicked);
    connect(m_recentList,    &QListWidget::customContextMenuRequested, this, &StartDialog::onRecentContextMenu);
    connect(cancelBtn,       &QPushButton::clicked,         this, &QDialog::reject);
}

void StartDialog::loadRecentList() {
    m_recentList->clear();
    const QStringList paths = recentProjects();
    if (paths.isEmpty()) {
        auto* item = new QListWidgetItem("No recent projects");
        item->setFlags(Qt::NoItemFlags);
        item->setForeground(QColor("#aaa"));
        m_recentList->addItem(item);
        return;
    }
    for (const QString& p : paths) {
        auto* item = new QListWidgetItem(QFileInfo(p).fileName(), m_recentList);
        item->setData(Qt::UserRole, p);
        item->setToolTip(p);
    }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void StartDialog::onNewProject() {
    m_choice = NewProject;
    accept();
}

void StartDialog::onOpenProject() {
    QString folder = QFileDialog::getExistingDirectory(
        this, "Open Presentation Folder", QDir::homePath(),
        QFileDialog::ShowDirsOnly);
    if (folder.isEmpty()) return;
    m_choice       = OpenProject;
    m_selectedPath = folder;
    accept();
}

void StartDialog::onRecentDoubleClicked() {
    auto* item = m_recentList->currentItem();
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;
    m_choice       = OpenProject;
    m_selectedPath = path;
    accept();
}

void StartDialog::onRecentClicked() {
    auto* item = m_recentList->currentItem();
    m_openRecentBtn->setEnabled(item && !item->data(Qt::UserRole).toString().isEmpty());
}

void StartDialog::onRecentContextMenu(const QPoint& pos) {
    auto* item = m_recentList->itemAt(pos);
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) return;

    QMenu menu(this);
    QAction* openAct   = menu.addAction("Im Explorer öffnen");
    QAction* renameAct = menu.addAction("Umbenennen...");
    menu.addSeparator();
    QAction* removeAct = menu.addAction("Aus Liste entfernen");
    QAction* deleteAct = menu.addAction("Projektordner löschen...");

    QAction* chosen = menu.exec(m_recentList->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == openAct) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    } else if (chosen == removeAct) {
        removeRecentProject(path);
        loadRecentList();
    } else if (chosen == deleteAct) {
        auto reply = QMessageBox::warning(this, "Projektordner löschen",
            QString("Soll der Ordner\n\n%1\n\nunwiderruflich gelöscht werden?").arg(path),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        QDir dir(path);
        if (!dir.removeRecursively()) {
            QMessageBox::warning(this, "Löschen fehlgeschlagen",
                "Der Ordner konnte nicht vollständig gelöscht werden.");
        }
        removeRecentProject(path);
        loadRecentList();
    } else if (chosen == renameAct) {
        QFileInfo fi(path);
        QDir parentDir = fi.dir();
        bool ok = false;
        QString newName = QInputDialog::getText(this, "Projekt umbenennen",
            "Neuer Ordnername:", QLineEdit::Normal, fi.fileName(), &ok);
        if (!ok) return;
        newName = newName.trimmed();
        if (newName.isEmpty() || newName == fi.fileName()) return;
        if (parentDir.exists(newName)) {
            QMessageBox::warning(this, "Umbenennen fehlgeschlagen",
                "Es existiert bereits ein Ordner mit diesem Namen.");
            return;
        }
        if (!parentDir.rename(fi.fileName(), newName)) {
            QMessageBox::warning(this, "Umbenennen fehlgeschlagen",
                "Der Ordner konnte nicht umbenannt werden.");
            return;
        }
        const QString newPath = parentDir.filePath(newName);
        QSettings s;
        QStringList list = s.value(kSettingsKey).toStringList();
        int idx = list.indexOf(path);
        if (idx >= 0) list[idx] = newPath;
        s.setValue(kSettingsKey, list);
        loadRecentList();
    }
}

// ── Update check ──────────────────────────────────────────────────────────────

void StartDialog::refreshUpdateButton() {
    if (m_updateBusy) return;

    if (m_updateAvailable) {
        m_updateBtn->setText(QString("Update to %1 available - click to install").arg(m_updateVersion));
        m_updateBtn->setStyleSheet(
            "QPushButton { color:#ffffff; background:#dc2626; border:none; border-radius:4px; padding:4px 12px; font-weight:bold; }"
            "QPushButton:hover { background:#b91c1c; }");
    } else {
        m_updateBtn->setText("Check for Updates");
        m_updateBtn->setStyleSheet(
            "QPushButton { color:#6b7280; background:transparent; border:none; padding:4px 8px; }"
            "QPushButton:hover { color:#374151; text-decoration:underline; }");
    }
}

void StartDialog::onCheckUpdateClicked() {
    if (m_updateBusy) return;

    if (m_updateAvailable) {
        m_updateBusy = true;
        m_updateBtn->setText("Downloading update...");
        m_updateChecker->downloadAndInstall(m_updateDownloadUrl);
        return;
    }

    m_updateBtn->setText("Checking for updates...");
    m_updateChecker->checkForUpdates();
}

void StartDialog::onUpdateAvailable(const QString& version, const QString& downloadUrl) {
    m_updateAvailable   = true;
    m_updateVersion     = version;
    m_updateDownloadUrl = downloadUrl;
    refreshUpdateButton();
}

void StartDialog::onUpToDate() {
    m_updateBtn->setText("You're up to date");
    QTimer::singleShot(3000, this, [this]() { refreshUpdateButton(); });
}

void StartDialog::onUpdateCheckFailed(const QString& error) {
    m_updateBtn->setText("Update check failed");
    m_updateBtn->setToolTip(error);
    QTimer::singleShot(3000, this, [this]() { refreshUpdateButton(); });
}

void StartDialog::onInstallerReady(const QString& installerPath) {
    QProcess::startDetached(installerPath, {});
    QApplication::quit();
}

void StartDialog::onUpdateDownloadFailed(const QString& error) {
    m_updateBusy = false;
    refreshUpdateButton();
    QMessageBox::warning(this, "Update Failed",
        QString("Could not download the update:\n%1").arg(error));
}
