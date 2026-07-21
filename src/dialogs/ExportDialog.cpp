#include "ExportDialog.h"
#include "export/HtmlExporter.h"
#include "export/ImageExporter.h"
#include "export/PdfExporter.h"
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
#include <QListWidget>
#include <QStackedWidget>
#include <QFileInfo>

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

static const char* kPrimaryButtonStyle =
    "QPushButton { background:#0078d4; color:white; border:none; padding:6px 20px; }"
    "QPushButton:hover { background:#106ebe; }";

ExportDialog::ExportDialog(Presentation* pres, QWidget* parent)
    : QDialog(parent), m_pres(pres)
{
    setWindowTitle("Export Presentation");
    resize(760, 560);

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

    auto* outer = new QVBoxLayout(this);
    outer->setSpacing(0);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* body = new QHBoxLayout;
    body->setSpacing(0);
    body->setContentsMargins(0, 0, 0, 0);

    m_navList = new QListWidget(this);
    m_navList->setFixedWidth(190);
    m_navList->addItem("Save Presentation");
    m_navList->addItem("Export as Images");
    m_navList->addItem("Export as PDF");
    m_navList->setStyleSheet(
        "QListWidget { background:#f3f4f6; border:none; border-right:1px solid #d1d5db; outline:0; padding-top:8px; font-size:12px; }"
        "QListWidget::item { padding:10px 14px; color:#374151; }"
        "QListWidget::item:selected { background:#0078d4; color:white; }");
    body->addWidget(m_navList);

    m_stack = new QStackedWidget(this);
    m_stack->setContentsMargins(18, 16, 18, 8);
    m_stack->addWidget(buildSavePage(initParent, initName));
    m_stack->addWidget(buildImagesPage(initParent, initName));
    m_stack->addWidget(buildPdfPage(initParent, initName));
    body->addWidget(m_stack, 1);

    outer->addLayout(body, 1);

    // Bottom bar, shared across all pages
    auto* bottomRow = new QHBoxLayout;
    bottomRow->setContentsMargins(18, 8, 18, 16);
    auto* closeBtn = new QPushButton("Close", this);
    bottomRow->addStretch();
    bottomRow->addWidget(closeBtn);
    outer->addLayout(bottomRow);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_navList, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    m_navList->setCurrentRow(0);

    updatePreview();
}

// ── Page: Save Presentation (HTML export) ──────────────────────────────────

QWidget* ExportDialog::buildSavePage(const QString& initParent, const QString& initName) {
    auto* page = new QWidget(this);
    auto* vbox = new QVBoxLayout(page);
    vbox->setSpacing(10);

    auto* title = new QLabel("<b>Save Presentation As</b>", page);
    title->setStyleSheet("font-size:14px;");
    vbox->addWidget(title);

    auto* info = new QLabel(
        "Creates a folder with <b>index.html</b>, CSS and assets.\n"
        "Impress.js is automatically included via CDN – just open index.html in a browser.", page);
    info->setWordWrap(true);
    info->setStyleSheet("color:#888; font-size:11px;");
    vbox->addWidget(info);

    auto* form = new QFormLayout;
    form->setSpacing(8);

    m_nameEdit = new QLineEdit(page);
    m_nameEdit->setPlaceholderText("my-presentation");
    m_nameEdit->setText(initName);
    form->addRow("Folder name:", m_nameEdit);

    auto* row = new QHBoxLayout;
    m_parentEdit = new QLineEdit(initParent, page);
    auto* browseBtn = new QPushButton("...", page);
    browseBtn->setFixedWidth(32);
    row->addWidget(m_parentEdit);
    row->addWidget(browseBtn);
    form->addRow("Location:", row);

    vbox->addLayout(form);

    m_previewLbl = new QLabel(page);
    m_previewLbl->setStyleSheet(
        "background:#222; border:1px solid #555; padding:6px; font-family:monospace; font-size:11px;");
    m_previewLbl->setWordWrap(true);
    vbox->addWidget(m_previewLbl);

    auto* statusRow = new QHBoxLayout;
    statusRow->setSpacing(6);
    m_statusIcon = new QLabel(page);
    m_statusIcon->setFixedSize(16, 16);
    m_statusIcon->setAlignment(Qt::AlignTop);
    statusRow->addWidget(m_statusIcon, 0, Qt::AlignTop);
    m_status = new QLabel("", page);
    m_status->setWordWrap(true);
    m_status->setMinimumHeight(40);
    statusRow->addWidget(m_status, 1);
    vbox->addLayout(statusRow);

    vbox->addStretch();

    m_openBtn = new QPushButton("Open Folder", page);
    m_openBtn->setVisible(false);
    vbox->addWidget(m_openBtn);

    auto* btnRow = new QHBoxLayout;
    m_expBtn = new QPushButton("Export", page);
    m_expBtn->setDefault(true);
    m_expBtn->setStyleSheet(kPrimaryButtonStyle);
    btnRow->addStretch();
    btnRow->addWidget(m_expBtn);
    vbox->addLayout(btnRow);

    connect(browseBtn,    &QPushButton::clicked,   this, &ExportDialog::browseFolder);
    connect(m_expBtn,     &QPushButton::clicked,   this, &ExportDialog::doExport);
    connect(m_nameEdit,   &QLineEdit::textChanged, this, &ExportDialog::updatePreview);
    connect(m_parentEdit, &QLineEdit::textChanged, this, &ExportDialog::updatePreview);

    return page;
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

// ── Shared slide-selection list, used by the Images and PDF pages ─────────

QListWidget* ExportDialog::buildSlideCheckList() {
    auto* list = new QListWidget();
    list->setMinimumHeight(160);
    if (m_pres) {
        for (int i = 0; i < m_pres->slides.size(); ++i) {
            QString label = QString("%1. %2").arg(i + 1)
                .arg(m_pres->slides[i].name.isEmpty() ? QString("Slide %1").arg(i + 1)
                                                       : m_pres->slides[i].name);
            auto* item = new QListWidgetItem(label, list);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Checked);
            item->setData(Qt::UserRole, i);
        }
    }
    return list;
}

void ExportDialog::checkAllSlides(QListWidget* list, bool checked) {
    for (int i = 0; i < list->count(); ++i)
        list->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

QVector<int> ExportDialog::checkedSlideIndices(QListWidget* list) {
    QVector<int> indices;
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->checkState() == Qt::Checked)
            indices.append(list->item(i)->data(Qt::UserRole).toInt());
    return indices;
}

// ── Page: Export as Images ──────────────────────────────────────────────────

QWidget* ExportDialog::buildImagesPage(const QString& initParent, const QString& initName) {
    auto* page = new QWidget(this);
    auto* vbox = new QVBoxLayout(page);
    vbox->setSpacing(10);

    auto* title = new QLabel("<b>Export Slides as Images</b>", page);
    title->setStyleSheet("font-size:14px;");
    vbox->addWidget(title);

    auto* info = new QLabel(
        "Tick the slides you want as PNG images (or use Select All). "
        "They are all saved together in the folder below.", page);
    info->setWordWrap(true);
    info->setStyleSheet("color:#888; font-size:11px;");
    vbox->addWidget(info);

    m_slideList = buildSlideCheckList();
    vbox->addWidget(m_slideList, 1);

    auto* selRow = new QHBoxLayout;
    auto* selAllBtn  = new QPushButton("Select All", page);
    auto* selNoneBtn = new QPushButton("Select None", page);
    selRow->addWidget(selAllBtn);
    selRow->addWidget(selNoneBtn);
    selRow->addStretch();
    vbox->addLayout(selRow);

    auto* imgForm = new QFormLayout;
    imgForm->setSpacing(8);
    auto* imgLocRow = new QHBoxLayout;
    m_imgFolderEdit = new QLineEdit(page);
    m_imgFolderEdit->setText(QDir(initParent).filePath(initName + "-images"));
    auto* imgBrowseBtn = new QPushButton("...", page);
    imgBrowseBtn->setFixedWidth(32);
    imgLocRow->addWidget(m_imgFolderEdit);
    imgLocRow->addWidget(imgBrowseBtn);
    imgForm->addRow("Save to:", imgLocRow);
    vbox->addLayout(imgForm);

    m_imgStatus = new QLabel("", page);
    m_imgStatus->setWordWrap(true);
    vbox->addWidget(m_imgStatus);

    m_imgExportBtn = new QPushButton("Export Images", page);
    m_imgExportBtn->setStyleSheet(kPrimaryButtonStyle);
    vbox->addWidget(m_imgExportBtn);

    connect(selAllBtn,      &QPushButton::clicked, this, [this]() { checkAllSlides(m_slideList, true); });
    connect(selNoneBtn,     &QPushButton::clicked, this, [this]() { checkAllSlides(m_slideList, false); });
    connect(imgBrowseBtn,   &QPushButton::clicked, this, &ExportDialog::browseImageFolder);
    connect(m_imgExportBtn, &QPushButton::clicked, this, &ExportDialog::doExportImages);

    return page;
}

void ExportDialog::browseImageFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, "Choose Location",
        m_imgFolderEdit->text().isEmpty() ? QDir::homePath() : m_imgFolderEdit->text());
    if (!dir.isEmpty())
        m_imgFolderEdit->setText(dir);
}

void ExportDialog::doExportImages() {
    if (!m_pres || m_pres->slides.isEmpty()) {
        m_imgStatus->setStyleSheet("color: red;");
        m_imgStatus->setText("No slides available.");
        return;
    }

    QVector<int> indices = checkedSlideIndices(m_slideList);
    if (indices.isEmpty()) {
        m_imgStatus->setStyleSheet("color: red;");
        m_imgStatus->setText("Please select at least one slide.");
        return;
    }

    QString outDir = m_imgFolderEdit->text().trimmed();
    if (outDir.isEmpty()) {
        m_imgStatus->setStyleSheet("color: red;");
        m_imgStatus->setText("Please choose a folder.");
        return;
    }

    m_imgExportBtn->setEnabled(false);
    m_imgStatus->setStyleSheet("color: orange;");
    m_imgStatus->setText("Exporting images…");
    repaint();

    auto result = ImageExporter::exportSlides(*m_pres, indices, outDir);

    m_imgExportBtn->setEnabled(true);
    if (result.ok) {
        m_imgStatus->setStyleSheet("color: #4c4;");
        m_imgStatus->setText(QString("Exported %1 slide(s) to:\n%2").arg(indices.size()).arg(outDir));
    } else {
        m_imgStatus->setStyleSheet("color: red;");
        m_imgStatus->setText("Error: " + result.errorMessage);
    }
}

// ── Page: Export as PDF ──────────────────────────────────────────────────────

QWidget* ExportDialog::buildPdfPage(const QString& initParent, const QString& initName) {
    auto* page = new QWidget(this);
    auto* vbox = new QVBoxLayout(page);
    vbox->setSpacing(10);

    auto* title = new QLabel("<b>Export Slides as PDF</b>", page);
    title->setStyleSheet("font-size:14px;");
    vbox->addWidget(title);

    auto* info = new QLabel(
        "Tick the slides you want (or use Select All). They are combined into "
        "one PDF file, one slide per page.", page);
    info->setWordWrap(true);
    info->setStyleSheet("color:#888; font-size:11px;");
    vbox->addWidget(info);

    m_pdfSlideList = buildSlideCheckList();
    vbox->addWidget(m_pdfSlideList, 1);

    auto* selRow = new QHBoxLayout;
    auto* selAllBtn  = new QPushButton("Select All", page);
    auto* selNoneBtn = new QPushButton("Select None", page);
    selRow->addWidget(selAllBtn);
    selRow->addWidget(selNoneBtn);
    selRow->addStretch();
    vbox->addLayout(selRow);

    auto* pdfForm = new QFormLayout;
    pdfForm->setSpacing(8);
    auto* pdfLocRow = new QHBoxLayout;
    m_pdfFileEdit = new QLineEdit(page);
    m_pdfFileEdit->setText(QDir(initParent).filePath(initName + ".pdf"));
    auto* pdfBrowseBtn = new QPushButton("...", page);
    pdfBrowseBtn->setFixedWidth(32);
    pdfLocRow->addWidget(m_pdfFileEdit);
    pdfLocRow->addWidget(pdfBrowseBtn);
    pdfForm->addRow("Save to:", pdfLocRow);
    vbox->addLayout(pdfForm);

    m_pdfStatus = new QLabel("", page);
    m_pdfStatus->setWordWrap(true);
    vbox->addWidget(m_pdfStatus);

    m_pdfExportBtn = new QPushButton("Export PDF", page);
    m_pdfExportBtn->setStyleSheet(kPrimaryButtonStyle);
    vbox->addWidget(m_pdfExportBtn);

    connect(selAllBtn,      &QPushButton::clicked, this, [this]() { checkAllSlides(m_pdfSlideList, true); });
    connect(selNoneBtn,     &QPushButton::clicked, this, [this]() { checkAllSlides(m_pdfSlideList, false); });
    connect(pdfBrowseBtn,   &QPushButton::clicked, this, &ExportDialog::browsePdfFile);
    connect(m_pdfExportBtn, &QPushButton::clicked, this, &ExportDialog::doExportPdf);

    return page;
}

void ExportDialog::browsePdfFile() {
    QString file = QFileDialog::getSaveFileName(this, "Choose PDF File",
        m_pdfFileEdit->text().isEmpty() ? QDir::homePath() : m_pdfFileEdit->text(),
        "PDF Files (*.pdf)");
    if (!file.isEmpty()) {
        if (!file.endsWith(".pdf", Qt::CaseInsensitive)) file += ".pdf";
        m_pdfFileEdit->setText(file);
    }
}

void ExportDialog::doExportPdf() {
    if (!m_pres || m_pres->slides.isEmpty()) {
        m_pdfStatus->setStyleSheet("color: red;");
        m_pdfStatus->setText("No slides available.");
        return;
    }

    QVector<int> indices = checkedSlideIndices(m_pdfSlideList);
    if (indices.isEmpty()) {
        m_pdfStatus->setStyleSheet("color: red;");
        m_pdfStatus->setText("Please select at least one slide.");
        return;
    }

    QString filePath = m_pdfFileEdit->text().trimmed();
    if (filePath.isEmpty()) {
        m_pdfStatus->setStyleSheet("color: red;");
        m_pdfStatus->setText("Please choose a file.");
        return;
    }
    if (!filePath.endsWith(".pdf", Qt::CaseInsensitive)) filePath += ".pdf";

    m_pdfExportBtn->setEnabled(false);
    m_pdfStatus->setStyleSheet("color: orange;");
    m_pdfStatus->setText("Exporting PDF…");
    repaint();

    auto result = PdfExporter::exportSlides(*m_pres, indices, filePath);

    m_pdfExportBtn->setEnabled(true);
    if (result.ok) {
        m_pdfStatus->setStyleSheet("color: #4c4;");
        m_pdfStatus->setText(QString("Exported %1 slide(s) to:\n%2").arg(indices.size()).arg(filePath));
    } else {
        m_pdfStatus->setStyleSheet("color: red;");
        m_pdfStatus->setText("Error: " + result.errorMessage);
    }
}
