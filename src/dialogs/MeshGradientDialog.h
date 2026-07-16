#pragma once
#include <QDialog>
#include <QWidget>
#include "models/MeshGradientData.h"

class QPushButton;
class QLabel;

// Interactive canvas for editing a shape's mesh-gradient control points:
// click empty space to add a colored point, drag to move it, double-click
// to recolor (incl. alpha), Delete/right-click to remove (min. 3 points
// enforced). Renders a live preview via MeshGradientRenderer.
class MeshGradientCanvas : public QWidget {
    Q_OBJECT
public:
    explicit MeshGradientCanvas(const QString& shapeType, QWidget* parent = nullptr);

    void setPoints(const QVector<MeshGradientPoint>& pts) { m_points = pts; update(); }
    const QVector<MeshGradientPoint>& points() const { return m_points; }

    QSize sizeHint() const override { return QSize(420, 420); }

signals:
    void pointsChanged();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    QRectF shapeRectPx() const; // drawing area inset by a margin
    int    hitTestPoint(const QPointF& widgetPos) const; // -1 if none within ~9px
    void   removePoint(int idx);

    QString m_shapeType;
    QVector<MeshGradientPoint> m_points;
    int    m_dragIndex     = -1;
    int    m_selectedIndex = -1;
    QColor m_lastColor     = Qt::white;
};

class MeshGradientDialog : public QDialog {
    Q_OBJECT
public:
    explicit MeshGradientDialog(const QString& shapeType, const MeshGradientData& initial,
                                 QWidget* parent = nullptr);

    MeshGradientData meshGradient() const { return MeshGradientData{ m_canvas->points() }; }

private:
    void updateOkEnabled();

    MeshGradientCanvas* m_canvas   = nullptr;
    QPushButton*        m_okBtn    = nullptr;
    QLabel*             m_hintLabel = nullptr;
};
