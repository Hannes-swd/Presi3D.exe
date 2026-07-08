#pragma once
#include <QMainWindow>
#include <QAction>
#include <deque>
#include "models/DataModel.h"

class SlideListPanel;
class EditorArea;
class PropertiesPanel;
class FormatBar;
class QTimer;
class LocalHttpServer;
class UpdateChecker;
class QToolButton;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void openPresentationFromFolder(const QString& folder);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void newPresentation();
    void openPresentation();
    void savePresentation();
    void savePresentationAs();
    void exportPresentation();
    void openInBrowser();

    void openVariableManager();

    void onSlideSelected(int index);
    void onSlideAdded();
    void onSlideRemoved(int index);
    void onSlideDuplicated(int index);
    void onSlideRenamed(int index, const QString& name);
    void onSlideMoved(int from, int to);
    void onPresentationModified();
    void onElementSelected(int elemIndex);
    void onFormatPainterRequested();

    void undo();
    void redo();

    void autosave();

    void checkForUpdatesManually();
    void onUpdateAvailable(const QString& version, const QString& downloadUrl);
    void onUpdateCheckFailed(const QString& error);
    void onUpToDate();
    void downloadAndInstallUpdate();
    void onInstallerReady(const QString& installerPath);
    void onUpdateDownloadFailed(const QString& error);

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void connectSignals();
    void updateTitle();
    void updateAutosaveAction();
    bool maybeSave();
    void refreshAll();
    void setupUpdateButton();
    void refreshUpdateButtonIcon();

    // ── Undo/Redo (snapshot-based; Qt's implicit-sharing containers keep
    //    unmodified slides/elements shared between snapshots, so this stays
    //    cheap even on weak hardware) ────────────────────────────────────
    void pushUndoStep();        // call BEFORE a discrete, one-shot mutation
    void commitPendingUndo();   // flushes a batch of debounced changes (e.g. dragging)
    void resetUndoHistory();    // call when a different document is loaded
    void updateUndoRedoActions();

    Presentation*    m_presentation  = nullptr;
    int              m_selectedSlide = -1;
    QAction*         m_browserAction = nullptr;
    LocalHttpServer* m_previewServer = nullptr;

    UpdateChecker*   m_updateChecker      = nullptr;
    QToolButton*     m_updateButton       = nullptr;
    QAction*         m_checkUpdateAction  = nullptr;
    QAction*         m_downloadUpdateAction = nullptr;
    bool             m_updateAvailable    = false;
    QString          m_updateVersion;
    QString          m_updateDownloadUrl;

    SlideListPanel*  m_slidePanel    = nullptr;
    EditorArea*      m_editorArea    = nullptr;
    PropertiesPanel* m_propPanel     = nullptr;
    FormatBar*       m_formatBar     = nullptr;

    int              m_selectedElem  = -1;

    std::deque<Presentation> m_undoStack;
    std::deque<Presentation> m_redoStack;
    Presentation              m_undoBaseline;
    bool                      m_undoDirty = false;
    QTimer*                   m_undoDebounce = nullptr;
    QAction*                  m_undoAction   = nullptr;
    QAction*                  m_redoAction   = nullptr;
    static constexpr int      kMaxUndoSteps  = 50;
    static constexpr int      kUndoDebounceMs = 500;

    // ── Autosave: once a project folder (exportPath) is known, edits are
    //    written to disk automatically after a short idle debounce ────────
    QTimer*                   m_autosaveTimer   = nullptr;
    QAction*                  m_autosaveAction  = nullptr;
    bool                      m_autosaveEnabled = true;
    static constexpr int      kAutosaveDebounceMs = 2000;
};
