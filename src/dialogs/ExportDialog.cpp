#include "ExportDialog.h"
#include "export/HtmlExporter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QIcon>
#include <QPainter>

// Renders a Material Symbols SVG icon tinted with the given color.
static QPixmap tintedIcon(const QString& name, const QColor& color, int size) {
    QPixmap src = QIcon(":/icons/" + name + ".svg").pixmap(size, size);
    QPixmap tinted(src.size());
    tinted.fill(Qt::transparent);
    QPainter p(&tinted);
    p.drawPixmap(0, 0, src);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(tinted.rect(), color);
    p.end();
    return tinted;
}

ExportDialog::ExportDialog(Presentation* pres, QWidget* parent)
    : QDialog(parent), m_pres(pres)
{
    setWindowTitle("Export Presentation");
    setMinimumWidth(520);

    auto* vbox = new QVBoxLayout(this);
    vbox->setSpacing(10);

    auto* title = new QLabel("<b>Export Presentation</b>", this);
    title->setStyleSheet("font-size:14px;");
    vbox->addWidget(title);

    auto* info = new QLabel(
        "Creates a folder with <b>index.html</b>, CSS and assets.\n"
        "Impress.js is automatically included via CDN – just open index.html in a browser.", this);
    info->setWordWrap(true);
    info->setStyleSheet("color:#888; font-size:11px;");
    vbox->addWidget(info);

    // Form
    auto* form = new QFormLayout;
    form->setSpacing(8);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("my-presentation");

    // Pre-fill from last export path, or suggest a name from the first slide
    QString initParent = QDir::homePath();
    QString initName;
    if (pres && !pres->exportPath.isEmpty()) {
        QFileInfo fi(pres->exportPath);
        initParent = fi.absolutePath();
        initName   = fi.fileName();
    }
    if (initName.isEmpty() && pres && !pres->slides.isEmpty())
        initName = pres->slides.first().name.toLower().replace(' ', '-');
    if (initName.isEmpty()) initName = "presentation";
    m_nameEdit->setText(initName);
    form->addRow("Folder name:", m_nameEdit);

    auto* row = new QHBoxLayout;
    m_parentEdit = new QLineEdit(initParent, this);
    auto* browseBtn = new QPushButton("...", this);
    browseBtn->setFixedWidth(32);
    row->addWidget(m_parentEdit);
    row->addWidget(browseBtn);
    form->addRow("Location:", row);

    vbox->addLayout(form);

    // Preview
    m_previewLbl = new QLabel(this);
    m_previewLbl->setStyleSheet(
        "background:#222; border:1px solid #555; padding:6px; font-family:monospace; font-size:11px;");
    m_previewLbl->setWordWrap(true);
    vbox->addWidget(m_previewLbl);

    // Status
    auto* statusRow = new QHBoxLayout;
    statusRow->setSpacing(6);
    m_statusIcon = new QLabel(this);
    m_statusIcon->setFixedSize(16, 16);
    m_statusIcon->setAlignment(Qt::AlignTop);
    statusRow->addWidget(m_statusIcon, 0, Qt::AlignTop);
    m_status = new QLabel("", this);
    m_status->setWordWrap(true);
    m_status->setMinimumHeight(40);
    statusRow->addWidget(m_status, 1);
    vbox->addLayout(statusRow);

    vbox->addStretch();

    // Open button (hidden until export succeeds)
    m_openBtn = new QPushButton("Open Folder", this);
    m_openBtn->setVisible(false);
    vbox->addWidget(m_openBtn);

    // Bottom buttons
    auto* btnRow = new QHBoxLayout;
    m_expBtn = new QPushButton("Export", this);
    m_expBtn->setDefault(true);
    m_expBtn->setStyleSheet(
        "QPushButton { background:#0078d4; color:white; border:none; padding:6px 20px; }"
        "QPushButton:hover { background:#106ebe; }");
    auto* closeBtn = new QPushButton("Close", this);
    btnRow->addStretch();
    btnRow->addWidget(m_expBtn);
    btnRow->addWidget(closeBtn);
    vbox->addLayout(btnRow);

    connect(browseBtn,   &QPushButton::clicked,      this, &ExportDialog::browseFolder);
    connect(m_expBtn,    &QPushButton::clicked,      this, &ExportDialog::doExport);
    connect(closeBtn,    &QPushButton::clicked,      this, &QDialog::accept);
    connect(m_nameEdit,  &QLineEdit::textChanged,    this, &ExportDialog::updatePreview);
    connect(m_parentEdit,&QLineEdit::textChanged,    this, &ExportDialog::updatePreview);

    updatePreview();
}

void ExportDialog::browseFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "Choose Location",
        m_parentEdit->text().isEmpty() ? QDir::homePath() : m_parentEdit->text());
    if (!dir.isEmpty())
        m_parentEdit->setText(dir);
}

void ExportDialog::updatePreview() {
    QString parent = m_parentEdit->text().trimmed();
    QString name   = m_nameEdit->text().trimmed();
    if (parent.isEmpty()) parent = QDir::homePath();
    if (name.isEmpty()) name = "presentation";

    QString full = QDir(parent).filePath(name);
    m_previewLbl->setText(
        QString("<span style='color:#888;'>Creates:</span> <span style='color:#4af;'>%1</span><br>"
                "<span style='color:#888;'>    ├── index.html</span><br>"
                "<span style='color:#888;'>    ├── styles.css</span><br>"
                "<span style='color:#888;'>    └── assets/</span>").arg(full));
}

void ExportDialog::doExport() {
    QString parent = m_parentEdit->text().trimmed();
    QString name   = m_nameEdit->text().trimmed();

    if (parent.isEmpty() || name.isEmpty()) {
        m_statusIcon->clear();
        m_status->setStyleSheet("color: red;");
        m_status->setText("Please provide a folder name and location.");
        return;
    }
    if (!m_pres || m_pres->slides.isEmpty()) {
        m_statusIcon->clear();
        m_status->setStyleSheet("color: red;");
        m_status->setText("No slides available.");
        return;
    }

    QString outDir = QDir(parent).filePath(name);
    QDir().mkpath(outDir);

    m_expBtn->setEnabled(false);
    m_statusIcon->clear();
    m_status->setStyleSheet("color: orange;");
    m_status->setText("Exporting…");
    repaint();

    auto result = HtmlExporter::exportTo(*m_pres, outDir);

    m_expBtn->setEnabled(true);
    if (result.ok) {
        // Remember the export path on the presentation object
        if (m_pres) m_pres->exportPath = outDir;

        m_statusIcon->setPixmap(tintedIcon("check_circle", QColor("#44cc44"), 16));
        m_status->setStyleSheet("color: #4c4;");
        QString msg = QString("Successfully exported to:\n%1").arg(outDir);
        if (!result.errorMessage.isEmpty()) msg += result.errorMessage;
        m_status->setText(msg);

        m_openBtn->setVisible(true);
        disconnect(m_openBtn, nullptr, nullptr, nullptr);
        connect(m_openBtn, &QPushButton::clicked, this, [outDir]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(outDir));
        });
    } else {
        m_statusIcon->clear();
        m_status->setStyleSheet("color: red;");
        m_status->setText("Error: " + result.errorMessage);
    }
}
