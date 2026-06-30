#include "MainWindow.h"
#include "SlideListPanel.h"
#include "EditorArea.h"
#include "PropertiesPanel.h"
#include "FormatBar.h"
#include "dialogs/ExportDialog.h"
#include "dialogs/StartDialog.h"
#include "import/HtmlImporter.h"
#include "export/HtmlExporter.h"

#include <QMenuBar>
#include <QToolBar>
#include <QSplitter>
#include <QStatusBar>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QFile>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_presentation = new Presentation();

    setupUI();
    setupMenuBar();
    setupToolBar();
    connectSignals();

    m_presentation->addSlide("Slide 1");
    refreshAll();
    onSlideSelected(0);

    updateTitle();
    resize(1440, 860);
}

MainWindow::~MainWindow() {
    delete m_presentation;
}

void MainWindow::setupUI() {
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    m_slidePanel = new SlideListPanel(this);
    m_slidePanel->setMinimumWidth(180);
    m_slidePanel->setMaximumWidth(280);

    m_editorArea = new EditorArea(this);

    m_propPanel = new PropertiesPanel(this);
    m_propPanel->setMinimumWidth(230);
    m_propPanel->setMaximumWidth(320);

    splitter->addWidget(m_slidePanel);
    splitter->addWidget(m_editorArea);
    splitter->addWidget(m_propPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({220, 920, 260});

    setCentralWidget(splitter);
    statusBar()->showMessage("Bereit");
}

void MainWindow::setupMenuBar() {
    QMenu* fileMenu = menuBar()->addMenu("&Datei");
    fileMenu->addAction("&Neu",               this, &MainWindow::newPresentation,    QKeySequence::New);
    fileMenu->addAction("&Öffnen...",         this, &MainWindow::openPresentation,   QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction("&Speichern",         this, &MainWindow::savePresentation,   QKeySequence::Save);
    fileMenu->addAction("Speichern &unter...", this, &MainWindow::savePresentationAs, QKeySequence::SaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction("Im &Browser öffnen", this, &MainWindow::openInBrowser, QKeySequence("Ctrl+B"));
    fileMenu->addSeparator();
    fileMenu->addAction("&Beenden",           this, &QWidget::close,                 QKeySequence::Quit);

    QMenu* helpMenu = menuBar()->addMenu("&Hilfe");
    helpMenu->addAction("&Über", this, [this]() {
        QMessageBox::about(this, "Über Presi 3D",
            "<b>Presi 3D</b><br>"
            "Erstelle beeindruckende Präsentationen mit 3D-Effekten.<br><br>"
            "Qt 6 + OpenGL");
    });
}

void MainWindow::setupToolBar() {
    // ── Datei-Toolbar ──
    QToolBar* tb = addToolBar("Datei");
    tb->setMovable(false);
    tb->addAction("Neu",          this, &MainWindow::newPresentation);
    tb->addAction("Öffnen",       this, &MainWindow::openPresentation);
    tb->addAction("Speichern",    this, &MainWindow::savePresentation);
    tb->addSeparator();
    m_browserAction = tb->addAction("▷ Im Browser öffnen", this, &MainWindow::openInBrowser);
    m_browserAction->setToolTip("Exportierte Präsentation im Browser öffnen\n(F = Vollbild)");
    m_browserAction->setEnabled(false);

    // ── Format-Toolbar (second row) ──
    addToolBarBreak();
    QToolBar* ftb = addToolBar("Format");
    ftb->setMovable(false);
    m_formatBar = new FormatBar(ftb);
    ftb->addWidget(m_formatBar);
}

void MainWindow::connectSignals() {
    connect(m_slidePanel, &SlideListPanel::slideSelected,   this, &MainWindow::onSlideSelected);
    connect(m_slidePanel, &SlideListPanel::slideAdded,      this, &MainWindow::onSlideAdded);
    connect(m_slidePanel, &SlideListPanel::slideRemoved,    this, &MainWindow::onSlideRemoved);
    connect(m_slidePanel, &SlideListPanel::slideDuplicated, this, &MainWindow::onSlideDuplicated);
    connect(m_slidePanel, &SlideListPanel::slideRenamed,    this, &MainWindow::onSlideRenamed);
    connect(m_slidePanel, &SlideListPanel::slideMoved,      this, &MainWindow::onSlideMoved);

    connect(m_editorArea, &EditorArea::presentationModified, this, &MainWindow::onPresentationModified);
    connect(m_editorArea, &EditorArea::elementSelected,      this, &MainWindow::onElementSelected);

    connect(m_propPanel,  &PropertiesPanel::slideModified,               this, &MainWindow::onPresentationModified);
    connect(m_propPanel,  &PropertiesPanel::elementModified,             this, &MainWindow::onPresentationModified);
    connect(m_propPanel,  &PropertiesPanel::presentationSettingsModified, this, &MainWindow::onPresentationModified);

    connect(m_formatBar,  &FormatBar::modified,              this, &MainWindow::onPresentationModified);
    connect(m_formatBar,  &FormatBar::formatPainterRequested, this, &MainWindow::onFormatPainterRequested);

    connect(m_editorArea, &EditorArea::tableCellSelected,
            m_propPanel,  &PropertiesPanel::setSelectedTableCell);
    connect(m_editorArea, &EditorArea::tableCellSelected,
            m_formatBar,  &FormatBar::setTableCell);
}

void MainWindow::onSlideSelected(int index) {
    m_selectedSlide = index;
    m_selectedElem  = -1;
    m_editorArea->setPresentation(m_presentation, index);
    m_propPanel->setSlide(m_presentation, index);
    m_slidePanel->setSelectedSlide(index);
    m_formatBar->setContext(m_presentation, index, -1);
}

void MainWindow::onSlideAdded() {
    m_presentation->addSlide();
    int idx = m_presentation->slides.size() - 1;
    m_slidePanel->setPresentation(m_presentation);
    onSlideSelected(idx);
}

void MainWindow::onSlideRemoved(int index) {
    if (m_presentation->slides.size() <= 1) {
        QMessageBox::warning(this, "Löschen nicht möglich",
                             "Die letzte Slide kann nicht gelöscht werden.");
        return;
    }
    m_presentation->removeSlide(index);
    m_slidePanel->setPresentation(m_presentation);
    int newSel = qMin(index, m_presentation->slides.size() - 1);
    onSlideSelected(newSel);
}

void MainWindow::onSlideDuplicated(int index) {
    m_presentation->duplicateSlide(index);
    m_slidePanel->setPresentation(m_presentation);
    onSlideSelected(index + 1);
}

void MainWindow::onSlideRenamed(int index, const QString& name) {
    if (Slide* s = m_presentation->slideAt(index)) {
        s->name = name;
        m_presentation->modified = true;
        m_slidePanel->setPresentation(m_presentation);
        updateTitle();
    }
}

void MainWindow::onSlideMoved(int from, int to) {
    m_presentation->moveSlide(from, to);
    m_slidePanel->setPresentation(m_presentation);
    onSlideSelected(to);
}

void MainWindow::onPresentationModified() {
    m_presentation->modified = true;
    updateTitle();
    m_slidePanel->setPresentation(m_presentation);
    m_editorArea->refresh();
    // Refresh PropertiesPanel if an element is selected
    if (m_selectedElem >= 0)
        m_propPanel->setSelectedElement(m_selectedElem);
}

void MainWindow::onElementSelected(int elemIndex) {
    m_selectedElem = elemIndex;
    m_propPanel->setSelectedElement(elemIndex);
    m_formatBar->setContext(m_presentation, m_selectedSlide, elemIndex);
}

void MainWindow::refreshAll() {
    m_slidePanel->setPresentation(m_presentation);
    if (m_selectedSlide >= 0) {
        m_editorArea->setPresentation(m_presentation, m_selectedSlide);
        m_propPanel->setSlide(m_presentation, m_selectedSlide);
    }
    m_formatBar->setContext(m_presentation, m_selectedSlide, m_selectedElem);
}

void MainWindow::updateTitle() {
    QString title = "Presi 3D";
    if (!m_presentation->filePath.isEmpty())
        title = QFileInfo(m_presentation->filePath).baseName() + " — " + title;
    if (m_presentation->modified)
        title = "* " + title;
    setWindowTitle(title);

    if (m_browserAction) {
        bool hasExport = !m_presentation->exportPath.isEmpty()
                         && QFile::exists(m_presentation->exportPath + "/index.html");
        m_browserAction->setEnabled(hasExport);
    }
}

bool MainWindow::maybeSave() {
    if (!m_presentation->modified) return true;
    auto ret = QMessageBox::warning(this, "Ungespeicherte Änderungen",
        "Die Präsentation wurde geändert.\nMöchten Sie speichern?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ret == QMessageBox::Save)   { savePresentation(); return !m_presentation->modified; }
    if (ret == QMessageBox::Cancel) return false;
    return true;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    maybeSave() ? event->accept() : event->ignore();
}

void MainWindow::newPresentation() {
    if (!maybeSave()) return;
    delete m_presentation;
    m_presentation = new Presentation();
    m_presentation->addSlide("Slide 1");
    m_selectedSlide = 0;
    m_selectedElem  = -1;
    refreshAll();
    onSlideSelected(0);
    updateTitle();
}

void MainWindow::openPresentationFromFolder(const QString& folder) {
    if (folder.isEmpty()) return;

    QString errorMsg;
    Presentation* loaded = HtmlImporter::importFrom(folder, errorMsg);
    if (!loaded) {
        QMessageBox::warning(this, "Laden fehlgeschlagen", errorMsg);
        return;
    }

    delete m_presentation;
    m_presentation  = loaded;
    m_selectedSlide = 0;
    m_selectedElem  = -1;
    refreshAll();
    onSlideSelected(0);
    updateTitle();
    StartDialog::addRecentProject(folder);
    statusBar()->showMessage(
        QString("Geladen: %1  (%2 Folien)").arg(folder).arg(m_presentation->slides.size()));
}

void MainWindow::openPresentation() {
    if (!maybeSave()) return;

    QString folder = QFileDialog::getExistingDirectory(
        this, "Präsentationsordner öffnen",
        m_presentation->exportPath.isEmpty()
            ? QDir::homePath() : QFileInfo(m_presentation->exportPath).absolutePath(),
        QFileDialog::ShowDirsOnly);

    if (folder.isEmpty()) return;
    openPresentationFromFolder(folder);
}

void MainWindow::savePresentation() {
    if (m_presentation->exportPath.isEmpty()) {
        savePresentationAs();
        return;
    }
    auto result = HtmlExporter::exportTo(*m_presentation, m_presentation->exportPath);
    if (result.ok) {
        m_presentation->modified = false;
        updateTitle();
        statusBar()->showMessage("Gespeichert: " + m_presentation->exportPath, 4000);
    } else {
        QMessageBox::warning(this, "Fehler beim Speichern", result.errorMessage);
    }
}

void MainWindow::savePresentationAs() {
    ExportDialog dlg(m_presentation, this);
    dlg.exec();
    if (!m_presentation->exportPath.isEmpty()) {
        StartDialog::addRecentProject(m_presentation->exportPath);
        m_presentation->modified = false;
        updateTitle();
    }
}

void MainWindow::exportPresentation() {
    ExportDialog dlg(m_presentation, this);
    dlg.exec();
    if (!m_presentation->exportPath.isEmpty())
        StartDialog::addRecentProject(m_presentation->exportPath);
}

void MainWindow::openInBrowser() {
    QString indexPath = m_presentation->exportPath + "/index.html";
    if (!QFile::exists(indexPath)) {
        QMessageBox::information(this, "Kein Export",
            "Bitte zuerst exportieren (Datei → Exportieren),\n"
            "dann kann die Präsentation im Browser geöffnet werden.");
        return;
    }

    QString url = QUrl::fromLocalFile(indexPath).toString();

    static const QStringList chromePaths = {
        "C:/Program Files/Google/Chrome/Application/chrome.exe",
        "C:/Program Files (x86)/Google/Chrome/Application/chrome.exe",
    };
    for (const QString& exe : chromePaths) {
        if (QFile::exists(exe)) {
            QProcess::startDetached(exe, {url});
            return;
        }
    }

    // Chrome nicht gefunden — Standard-Browser als Fallback
    QDesktopServices::openUrl(QUrl::fromLocalFile(indexPath));
}

void MainWindow::onFormatPainterRequested() {
    if (m_selectedSlide < 0 || m_selectedElem < 0) return;
    const Slide* s = m_presentation->slideAt(m_selectedSlide);
    if (!s || m_selectedElem >= s->elements.size()) return;
    m_editorArea->activateFormatPainter(s->elements[m_selectedElem]);
}
