#pragma once
#include <QWidget>
#include "models/DataModel.h"

class QStackedWidget;
class QPushButton;
class QToolButton;
class QDoubleSpinBox;
class QSpinBox;
class SlideEditor2D;
class SlideEditor3D;
class TimelinePanel;

class EditorArea : public QWidget {
    Q_OBJECT
public:
    explicit EditorArea(QWidget* parent = nullptr);

    void setPresentation(Presentation* pres, int slideIndex);
    void refresh();
    void activateFormatPainter(const SlideElement& source);

    // Timeline panel + keyframe edit session, driven by MainWindow.
    void setSelectedElementForTimeline(int elemIndex);
    void setKeyframeEditActive(bool active, const QString& label = {});
    void ensure2DMode();
    void selectElement(int index);

signals:
    void presentationModified();
    void elementSelected(int elemIndex);
    void tableCellSelected(int row, int col);
    void worldObjectSelected(int index);
    void keyframeEditRequested(int elemIndex, bool isEntry);
    void keyframeEditDone();
    void textSelectionChanged(int cursorPos, int selAnchor);

private slots:
    void switchTo2D();
    void switchTo3D();

private:
    Presentation*    m_pres       = nullptr;
    int              m_slideIndex = -1;

    QStackedWidget*  m_stack      = nullptr;
    SlideEditor2D*   m_editor2D   = nullptr;
    SlideEditor3D*   m_editor3D   = nullptr;
    TimelinePanel*   m_timelinePanel = nullptr;
    QPushButton*     m_btnTimelineToggle = nullptr;

    QPushButton*     m_btn2D      = nullptr;
    QPushButton*     m_btn3D      = nullptr;

    // 2D element toolbar buttons
    QPushButton*     m_btnText     = nullptr;
    QPushButton*     m_btnShape    = nullptr;
    QPushButton*     m_btnIcon     = nullptr;
    QPushButton*     m_btnImage    = nullptr;
    QPushButton*     m_btnTable    = nullptr;
    QPushButton*     m_btnChart    = nullptr;
    QPushButton*     m_btnFormula  = nullptr;
    QPushButton*     m_btnIFrame   = nullptr;
    QToolButton*     m_btnInteractive = nullptr; // dropdown: Button / Checkbox / Slider
    QPushButton*     m_btnDelete   = nullptr;
    // Layer buttons
    QPushButton*     m_btnFront    = nullptr;
    QPushButton*     m_btnForward  = nullptr;
    QPushButton*     m_btnBackward = nullptr;
    QPushButton*     m_btnBack     = nullptr;
    // Zoom controls (2D canvas)
    QPushButton*     m_btnZoomOut   = nullptr;
    QPushButton*     m_btnZoomIn    = nullptr;
    QPushButton*     m_btnZoomReset = nullptr;
    QSpinBox*        m_zoomSpin     = nullptr;
    QWidget*         m_elemToolbar  = nullptr;

    // 3D gizmo toolbar
    QPushButton*     m_btnGizmoMove      = nullptr;
    QPushButton*     m_btnGizmoRotate    = nullptr;
    QPushButton*     m_btnGizmoScale     = nullptr;
    QPushButton*     m_btnInsertWorldObj = nullptr;
    QDoubleSpinBox*  m_distanceSpin      = nullptr;
    QWidget*         m_gizmoToolbar      = nullptr;
};
