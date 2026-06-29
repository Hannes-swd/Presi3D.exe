#include "StartDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QFrame>

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
}

void StartDialog::buildUi() {
    auto* vbox = new QVBoxLayout(this);
    vbox->setSpacing(10);
    vbox->setContentsMargins(16, 16, 16, 16);

    auto* title = new QLabel("<b>Presi 3D</b>", this);
    title->setStyleSheet("font-size:16px;");
    vbox->addWidget(title);

    // Action buttons
    auto* newBtn  = new QPushButton("Neues Projekt erstellen", this);
    auto* openBtn = new QPushButton("Projekt öffnen...", this);
    newBtn->setFixedHeight(32);
    openBtn->setFixedHeight(32);
    newBtn->setStyleSheet(
        "QPushButton { background:#0078d4; color:white; border:none; padding:4px 16px; }"
        "QPushButton:hover { background:#106ebe; }");

    vbox->addWidget(newBtn);
    vbox->addWidget(openBtn);

    // Divider
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    vbox->addWidget(line);

    auto* recentLabel = new QLabel("Zuletzt bearbeitet:", this);
    recentLabel->setStyleSheet("color:#888; font-size:11px;");
    vbox->addWidget(recentLabel);

    m_recentList = new QListWidget(this);
    vbox->addWidget(m_recentList, 1);

    // Bottom row
    auto* btnRow = new QHBoxLayout;
    m_openRecentBtn = new QPushButton("Öffnen", this);
    m_openRecentBtn->setEnabled(false);
    auto* cancelBtn = new QPushButton("Abbrechen", this);
    btnRow->addStretch();
    btnRow->addWidget(m_openRecentBtn);
    btnRow->addWidget(cancelBtn);
    vbox->addLayout(btnRow);

    connect(newBtn,          &QPushButton::clicked,         this, &StartDialog::onNewProject);
    connect(openBtn,         &QPushButton::clicked,         this, &StartDialog::onOpenProject);
    connect(m_openRecentBtn, &QPushButton::clicked,         this, &StartDialog::onRecentDoubleClicked);
    connect(m_recentList,    &QListWidget::itemDoubleClicked, this, &StartDialog::onRecentDoubleClicked);
    connect(m_recentList,    &QListWidget::itemClicked,       this, &StartDialog::onRecentClicked);
    connect(cancelBtn,       &QPushButton::clicked,         this, &QDialog::reject);
}

void StartDialog::loadRecentList() {
    m_recentList->clear();
    const QStringList paths = recentProjects();
    if (paths.isEmpty()) {
        auto* item = new QListWidgetItem("Keine kürzlichen Projekte");
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
        this, "Präsentationsordner öffnen", QDir::homePath(),
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
