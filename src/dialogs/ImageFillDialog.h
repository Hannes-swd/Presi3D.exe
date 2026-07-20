#pragma once
#include <QDialog>
#include <QWidget>
#include <QString>

class QPushButton;
class QLabel;
class QSlider;

// Interactive canvas for editing a shape's image/texture fill: the picked
// image always covers the shape's bounds (no gaps), drag to pan within the
// resulting slack, slider to zoom in further. Renders a live preview via
// ImageFillRenderer, mirroring MeshGradientCanvas's structure.
class ImageFillCanvas : public QWidget {
    Q_OBJECT
public:
    explicit ImageFillCanvas(const QString& shapeType, QWidget* parent = nullptr);

    void setImagePath(const QString& path) { m_imagePath = path; update(); emit changed(); }
    const QString& imagePath() const { return m_imagePath; }

    void  setOffset(float x, float y) { m_offsetX = x; m_offsetY = y; update(); }
    float offsetX() const { return m_offsetX; }
    float offsetY() const { return m_offsetY; }

    void  setScale(float s) { m_scale = s; update(); emit changed(); }
    float scale() const { return m_scale; }

    QSize sizeHint() const override { return QSize(420, 420); }

signals:
    void changed();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    QRectF shapeRectPx() const; // drawing area inset by a margin

    QString m_shapeType;
    QString m_imagePath;
    float   m_offsetX = 0.f;
    float   m_offsetY = 0.f;
    float   m_scale   = 1.f;
    bool    m_dragging = false;
    QPointF m_lastDragPos;
};

class ImageFillDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImageFillDialog(const QString& shapeType, const QString& imagePath,
                              float offsetX, float offsetY, float scale,
                              QWidget* parent = nullptr);

    QString imagePath() const { return m_canvas->imagePath(); }
    float   offsetX()   const { return m_canvas->offsetX(); }
    float   offsetY()   const { return m_canvas->offsetY(); }
    float   scale()     const { return m_canvas->scale(); }

private slots:
    void onChooseImage();

private:
    void updateOkEnabled();

    ImageFillCanvas* m_canvas    = nullptr;
    QPushButton*     m_chooseBtn = nullptr;
    QSlider*         m_zoomSlider = nullptr;
    QPushButton*     m_okBtn     = nullptr;
    QLabel*          m_hintLabel = nullptr;
};
