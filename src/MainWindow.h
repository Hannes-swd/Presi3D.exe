#pragma once
#include <QMainWindow>
#include <QAction>
#include "models/DataModel.h"

class SlideListPanel;
class EditorArea;
class PropertiesPanel;
class FormatBar;

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

    void onSlideSelected(int index);
    void onSlideAdded();
    void onSlideRemoved(int index);
    void onSlideDuplicated(int index);
    void onSlideRenamed(int index, const QString& name);
    void onSlideMoved(int from, int to);
    void onPresentationModified();
    void onElementSelected(int elemIndex);
    void onFormatPainterRequested();

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void connectSignals();
    void updateTitle();
    bool maybeSave();
    void refreshAll();

    Presentation*    m_presentation  = nullptr;
    int              m_selectedSlide = -1;
    QAction*         m_browserAction = nullptr;

    SlideListPanel*  m_slidePanel    = nullptr;
    EditorArea*      m_editorArea    = nullptr;
    PropertiesPanel* m_propPanel     = nullptr;
    FormatBar*       m_formatBar     = nullptr;

    int              m_selectedElem  = -1;
};
