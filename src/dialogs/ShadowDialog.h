#pragma once
#include <QDialog>
#include <QWidget>
#include <QColor>
#include "models/DataModel.h"

class QPushButton;
class QLabel;
class QSlider;
class QCheckBox;

// Live preview canvas for the drop-shadow dialog: shows the shadow applied
// to the ACTUAL selected element (its real shape/text/image), not a generic
// placeholder, via ShadowRenderer — the same renderer SlideEditor2D uses for
// the live 2D canvas, so the preview is pixel-consistent with the real thing.
class ShadowCanvas : public QWidget {
    Q_OBJECT
public:
    explicit ShadowCanvas(const SlideElement& element, QWidget* parent = nullptr);

    void setShadowOn(bool on)        { m_hasShadow = on; update(); }
    void setOffset(float x, float y) { m_offsetX = x; m_offsetY = y; update(); }
    void setBlur(float b)            { m_blur = b; update(); }
    void setSpread(float s)          { m_spread = s; update(); }
    void setColor(const QColor& c)   { m_color = c; update(); }

    QSize sizeHint() const override { return QSize(460, 340); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QRectF previewRect() const; // inset area matching the element's aspect ratio, centered

    SlideElement m_element; // frozen snapshot, used only to reproduce the real silhouette/content
    bool   m_hasShadow = true;
    float  m_offsetX = 0.f, m_offsetY = 0.f;
    float  m_blur = 0.f, m_spread = 0.f;
    QColor m_color;
};

// Fully configurable drop-shadow editor for any slide element: enable
// toggle, angle+distance OR direct X/Y offset (kept in sync, switchable),
// blur, spread and an alpha-capable color picker, with an instant live
// preview of the real element. Opened from FormatBar's "Edit…" shadow
// button and from SlideEditor2D's right-click context menu.
class ShadowDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShadowDialog(const SlideElement& element, QWidget* parent = nullptr);

    bool   hasShadow()  const;
    bool   useOffset()  const { return m_useOffsetMode; }
    float  angle()      const { return m_angle; }
    float  distance()   const { return m_distance; }
    float  offsetX()    const { return m_offsetX; }
    float  offsetY()    const { return m_offsetY; }
    float  blur()       const;
    float  spread()     const;
    QColor color()      const { return m_color; }

private slots:
    void onModeToggleClicked();
    void onAngleOrDistanceChanged();
    void onOffsetChanged();
    void onColorClicked();
    void onSlidersChanged(); // blur/spread/enabled -> push to canvas

private:
    void recomputeOffsetFromAngle();
    void recomputeAngleFromOffset();
    void updateModeVisibility();
    void updateColorButtonStyle();
    void pushToCanvas();

    ShadowCanvas* m_canvas       = nullptr;
    QCheckBox*    m_chkEnabled   = nullptr;
    QPushButton*  m_modeToggleBtn = nullptr;
    bool          m_useOffsetMode = false;

    QWidget* m_angleDistRow = nullptr;
    QSlider* m_angleSlider  = nullptr;
    QSlider* m_distSlider   = nullptr;
    QWidget* m_offsetRow    = nullptr;
    QSlider* m_offXSlider   = nullptr;
    QSlider* m_offYSlider   = nullptr;
    QSlider* m_blurSlider   = nullptr;
    QSlider* m_spreadSlider = nullptr;
    QPushButton* m_colorBtn = nullptr;

    float  m_angle    = 45.f;
    float  m_distance = 8.f;
    float  m_offsetX  = 5.66f;
    float  m_offsetY  = 5.66f;
    QColor m_color    = QColor(0, 0, 0, 160);
};
