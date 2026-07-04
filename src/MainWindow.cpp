#include "MainWindow.h"
#include "SlideListPanel.h"
#include "EditorArea.h"
#include "PropertiesPanel.h"
#include "FormatBar.h"
#include "dialogs/ExportDialog.h"
#include "dialogs/StartDialog.h"
#include "import/HtmlImporter.h"
#include "export/HtmlExporter.h"
#include "LocalHttpServer.h"

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
#include <QTimer>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_presentation = new Presentation();

    setupUI();
    setupMenuBar();
    setupToolBar();
    connectSignals();

    m_presentation->addSlide("Slide 1");
    refreshAll();
    onSlideSelected(0);

    m_undoDebounce = new QTimer(this);
    m_undoDebounce->setSingleShot(true);
    connect(m_undoDebounce, &QTimer::timeout, this, &MainWindow::commitPendingUndo);
    resetUndoHistory();

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
    statusBar()->showMessage("Ready");
}

void MainWindow::setupMenuBar() {
    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&New",               this, &MainWindow::newPresentation,    QKeySequence::New);
    fileMenu->addAction("&Open...",           this, &MainWindow::openPresentation,   QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction("&Save",              this, &MainWindow::savePresentation,   QKeySequence::Save);
    fileMenu->addAction("Save &As...",        this, &MainWindow::savePresentationAs, QKeySequence::SaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction("Open in &Browser",   this, &MainWindow::openInBrowser, QKeySequence("Ctrl+B"));
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit",              this, &QWidget::close,                 QKeySequence::Quit);

    QMenu* editMenu = menuBar()->addMenu("&Edit");
    m_undoAction = editMenu->addAction("&Undo", this, &MainWindow::undo, QKeySequence::Undo);
    m_redoAction = editMenu->addAction("&Redo", this, &MainWindow::redo, QKeySequence::Redo);

    QMenu* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", this, [this]() {
        QMessageBox::about(this, "About Presi 3D",
            "<b>Presi 3D</b><br>"
            "Create stunning presentations with 3D effects.<br><br>"
            "Qt 6 + OpenGL");
    });
}

void MainWindow::setupToolBar() {
    // ── File toolbar ──
    QToolBar* tb = addToolBar("File");
    tb->setMovable(false);
    tb->addAction(m_undoAction);
    m_undoAction->setText("↶");
    m_undoAction->setToolTip("Undo (Ctrl+Z)");
    tb->addAction(m_redoAction);
    m_redoAction->setText("↷");
    m_redoAction->setToolTip("Redo (Ctrl+Y)");
    tb->addSeparator();
    tb->addAction("New",          this, &MainWindow::newPresentation);
    tb->addAction("Open",         this, &MainWindow::openPresentation);
    tb->addAction("Save",         this, &MainWindow::savePresentation);
    tb->addSeparator();
    m_browserAction = tb->addAction("▷ Open in Browser", this, &MainWindow::openInBrowser);
    m_browserAction->setToolTip("Open exported presentation in browser\n(F = Fullscreen)");
    m_browserAction->setEnabled(false);

    // ── Format toolbar (second row) ──
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
    // m_propPanel must be updated before m_editorArea: SlideEditor2D::setSlide()
    // synchronously emits elementSelected(-1), which MainWindow::onElementSelected
    // forwards straight into PropertiesPanel::setSelectedElement(). If m_propPanel
    // still pointed at the old Presentation at that moment, it would dereference it.
    m_propPanel->setSlide(m_presentation, index);
    m_editorArea->setPresentation(m_presentation, index);
    m_slidePanel->setSelectedSlide(index);
    m_formatBar->setContext(m_presentation, index, -1);
}

void MainWindow::onSlideAdded() {
    pushUndoStep();
    m_presentation->addSlide();
    int idx = m_presentation->slides.size() - 1;
    m_slidePanel->setPresentation(m_presentation);
    onSlideSelected(idx);
    m_undoBaseline = *m_presentation;
}

void MainWindow::onSlideRemoved(int index) {
    if (m_presentation->slides.size() <= 1) {
        QMessageBox::warning(this, "Cannot Delete",
                             "The last slide cannot be deleted.");
        return;
    }
    pushUndoStep();
    m_presentation->removeSlide(index);
    m_slidePanel->setPresentation(m_presentation);
    int newSel = qMin(index, m_presentation->slides.size() - 1);
    onSlideSelected(newSel);
    m_undoBaseline = *m_presentation;
}

void MainWindow::onSlideDuplicated(int index) {
    pushUndoStep();
    m_presentation->duplicateSlide(index);
    m_slidePanel->setPresentation(m_presentation);
    onSlideSelected(index + 1);
    m_undoBaseline = *m_presentation;
}

void MainWindow::onSlideRenamed(int index, const QString& name) {
    if (Slide* s = m_presentation->slideAt(index)) {
        pushUndoStep();
        s->name = name;
        m_presentation->modified = true;
        m_slidePanel->setPresentation(m_presentation);
        updateTitle();
        m_undoBaseline = *m_presentation;
    }
}

void MainWindow::onSlideMoved(int from, int to) {
    pushUndoStep();
    m_presentation->moveSlide(from, to);
    m_slidePanel->setPresentation(m_presentation);
    onSlideSelected(to);
    m_undoBaseline = *m_presentation;
}

void MainWindow::onPresentationModified() {
    m_presentation->modified = true;
    updateTitle();
    m_slidePanel->setPresentation(m_presentation);
    m_editorArea->refresh();
    // Refresh PropertiesPanel if an element is selected
    if (m_selectedElem >= 0)
        m_propPanel->setSelectedElement(m_selectedElem);

    // Snapshot for undo: batched/debounced so continuous edits (dragging an
    // element, holding a spinbox arrow, ...) collapse into a single undo step.
    m_undoDirty = true;
    m_undoDebounce->start(kUndoDebounceMs);
}

void MainWindow::pushUndoStep() {
    commitPendingUndo(); // flush any earlier in-progress edit as its own step first
    m_undoStack.push_back(m_undoBaseline);
    if ((int)m_undoStack.size() > kMaxUndoSteps)
        m_undoStack.pop_front();
    m_redoStack.clear();
    updateUndoRedoActions();
}

void MainWindow::commitPendingUndo() {
    if (!m_undoDirty) return;
    m_undoDebounce->stop();
    m_undoDirty = false;
    m_undoStack.push_back(m_undoBaseline);
    if ((int)m_undoStack.size() > kMaxUndoSteps)
        m_undoStack.pop_front();
    m_redoStack.clear();
    m_undoBaseline = *m_presentation;
    updateUndoRedoActions();
}

void MainWindow::resetUndoHistory() {
    m_undoDebounce->stop();
    m_undoDirty = false;
    m_undoStack.clear();
    m_redoStack.clear();
    m_undoBaseline = *m_presentation;
    updateUndoRedoActions();
}

void MainWindow::updateUndoRedoActions() {
    if (m_undoAction) m_undoAction->setEnabled(!m_undoStack.empty());
    if (m_redoAction) m_redoAction->setEnabled(!m_redoStack.empty());
}

void MainWindow::undo() {
    commitPendingUndo(); // make sure an in-progress edit is undoable too
    if (m_undoStack.empty()) return;

    Presentation prev = m_undoStack.back();
    m_undoStack.pop_back();
    m_redoStack.push_back(*m_presentation);
    if ((int)m_redoStack.size() > kMaxUndoSteps)
        m_redoStack.pop_front();

    QString filePath   = m_presentation->filePath;
    QString exportPath = m_presentation->exportPath;
    *m_presentation = prev;
    m_presentation->filePath   = filePath;
    m_presentation->exportPath = exportPath;
    m_presentation->modified   = true;
    m_undoBaseline = *m_presentation;

    m_selectedSlide = qBound(0, m_selectedSlide, m_presentation->slides.size() - 1);
    refreshAll();
    onSlideSelected(m_selectedSlide);
    updateTitle();
    updateUndoRedoActions();
}

void MainWindow::redo() {
    if (m_redoStack.empty()) return;

    Presentation next = m_redoStack.back();
    m_redoStack.pop_back();
    m_undoStack.push_back(*m_presentation);
    if ((int)m_undoStack.size() > kMaxUndoSteps)
        m_undoStack.pop_front();

    QString filePath   = m_presentation->filePath;
    QString exportPath = m_presentation->exportPath;
    *m_presentation = next;
    m_presentation->filePath   = filePath;
    m_presentation->exportPath = exportPath;
    m_presentation->modified   = true;
    m_undoBaseline = *m_presentation;

    m_selectedSlide = qBound(0, m_selectedSlide, m_presentation->slides.size() - 1);
    refreshAll();
    onSlideSelected(m_selectedSlide);
    updateTitle();
    updateUndoRedoActions();
}

void MainWindow::onElementSelected(int elemIndex) {
    m_selectedElem = elemIndex;
    m_propPanel->setSelectedElement(elemIndex);
    m_formatBar->setContext(m_presentation, m_selectedSlide, elemIndex);
}

void MainWindow::refreshAll() {
    m_slidePanel->setPresentation(m_presentation);
    if (m_selectedSlide >= 0) {
        // See comment in onSlideSelected(): m_propPanel must be refreshed first.
        m_propPanel->setSlide(m_presentation, m_selectedSlide);
        m_editorArea->setPresentation(m_presentation, m_selectedSlide);
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
    auto ret = QMessageBox::warning(this, "Unsaved Changes",
        "The presentation has been modified.\nDo you want to save it?",
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
    resetUndoHistory();
    updateTitle();
}

void MainWindow::openPresentationFromFolder(const QString& folder) {
    if (folder.isEmpty()) return;

    QString errorMsg;
    Presentation* loaded = HtmlImporter::importFrom(folder, errorMsg);
    if (!loaded) {
        QMessageBox::warning(this, "Load Failed", errorMsg);
        return;
    }

    delete m_presentation;
    m_presentation  = loaded;
    m_selectedSlide = 0;
    m_selectedElem  = -1;
    refreshAll();
    onSlideSelected(0);
    resetUndoHistory();
    updateTitle();
    StartDialog::addRecentProject(folder);
    statusBar()->showMessage(
        QString("Loaded: %1  (%2 slides)").arg(folder).arg(m_presentation->slides.size()));
}

void MainWindow::openPresentation() {
    if (!maybeSave()) return;

    QString folder = QFileDialog::getExistingDirectory(
        this, "Open Presentation Folder",
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
        statusBar()->showMessage("Saved: " + m_presentation->exportPath, 4000);
    } else {
        QMessageBox::warning(this, "Save Error", result.errorMessage);
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
        QMessageBox::information(this, "No Export",
            "Please export first (File → Export),\n"
            "then the presentation can be opened in the browser.");
        return;
    }

    // Serve via http://127.0.0.1 instead of file:// — some embeds (e.g. YouTube,
    // "Error 153") refuse to initialize when the parent page has no http(s) origin.
    if (!m_previewServer)
        m_previewServer = new LocalHttpServer(this);
    QString base = m_previewServer->serve(m_presentation->exportPath);
    QString url = !base.isEmpty() ? base + "/index.html"
                                   : QUrl::fromLocalFile(indexPath).toString();

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

    // Chrome not found — fall back to default browser
    QDesktopServices::openUrl(QUrl::fromLocalFile(indexPath));
}

void MainWindow::onFormatPainterRequested() {
    if (m_selectedSlide < 0 || m_selectedElem < 0) return;
    const Slide* s = m_presentation->slideAt(m_selectedSlide);
    if (!s || m_selectedElem >= s->elements.size()) return;
    m_editorArea->activateFormatPainter(s->elements[m_selectedElem]);
}
