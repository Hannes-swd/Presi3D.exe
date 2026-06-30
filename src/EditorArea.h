#pragma once
#include <QWidget>
#include "models/DataModel.h"

class QStackedWidget;
class QPushButton;
class QDoubleSpinBox;
class SlideEditor2D;
class SlideEditor3D;

class EditorArea : public QWidget {
    Q_OBJECT
public:
    explicit EditorArea(QWidget* parent = nullptr);

    void setPresentation(Presentation* pres, int slideIndex);
    void refresh();
    void activateFormatPainter(const SlideElement& source);

signals:
    void presentationModified();
    void elementSelected(int elemIndex);
    void tableCellSelected(int row, int col);

private slots:
    void switchTo2D();
    void switchTo3D();

private:
    Presentation*    m_pres       = nullptr;
    int              m_slideIndex = -1;

    QStackedWidget*  m_stack      = nullptr;
    SlideEditor2D*   m_editor2D   = nullptr;
    SlideEditor3D*   m_editor3D   = nullptr;

    QPushButton*     m_btn2D      = nullptr;
    QPushButton*     m_btn3D      = nullptr;

    // 2D element toolbar buttons
    QPushButton*     m_btnText     = nullptr;
    QPushButton*     m_btnRect     = nullptr;
    QPushButton*     m_btnCircle   = nullptr;
    QPushButton*     m_btnImage    = nullptr;
    QPushButton*     m_btnTable    = nullptr;
    QPushButton*     m_btnDelete   = nullptr;
    // Layer buttons
    QPushButton*     m_btnFront    = nullptr;
    QPushButton*     m_btnForward  = nullptr;
    QPushButton*     m_btnBackward = nullptr;
    QPushButton*     m_btnBack     = nullptr;
    QWidget*         m_elemToolbar  = nullptr;

    // 3D gizmo toolbar
    QPushButton*     m_btnGizmoMove   = nullptr;
    QPushButton*     m_btnGizmoRotate = nullptr;
    QDoubleSpinBox*  m_distanceSpin   = nullptr;
    QWidget*         m_gizmoToolbar   = nullptr;
};
