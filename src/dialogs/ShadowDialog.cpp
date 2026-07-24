#include "ShadowDialog.h"
#include "ShadowRenderer.h"
#include "ShapeUtils.h"
#include "IconUtils.h"
#include "TextLayoutUtils.h"
#include <QPainter>
#include <QPainterPath>
#include <QTextLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QSlider>
#include <QColorDialog>
#include <QSignalBlocker>
#include <QtMath>
#include <algorithm>

// ── Real-element preview (simplified re-render, mirrors ShadowRenderer's
//    per-type silhouette functions but with actual colors/content instead of
//    a flat black mask) ───────────────────────────────────────────────────

static void paintElementContent(QPainter& p, const SlideElement& e, const QRectF& r) {
    switch (e.type) {
    case SlideElement::Text: {
        if (r.width() < 1.0 || r.height() < 1.0) return;
        float scaleY = float(r.height()) / qMax(1.f, e.height);
        QFont font = elemFont(e, scaleY);
        font.setUnderline(e.underline || !e.hyperlink.trimmed().isEmpty());
        font.setStrikeOut(e.strikethrough);
        QTextLayout layout(layoutText(e.content), font);
        buildLayout(layout, e, font, float(r.width()));
        float vOff = textVOff(layout, float(r.height()), e.verticalAlignment);
        p.setPen(e.color.isValid() ? e.color : Qt::black);
        p.setClipRect(r, Qt::IntersectClip);
        layout.draw(&p, r.topLeft() + QPointF(0, vOff));
        break;
    }
    case SlideElement::Shape: {
        p.setPen(e.borderWidth > 0
                 ? QPen(e.borderColor.isValid() ? e.borderColor : Qt::darkGray, e.borderWidth)
                 : Qt::NoPen);
        p.setBrush(e.backgroundColor == Qt::transparent ? Qt::NoBrush : QBrush(e.backgroundColor));
        if (e.content == "line") {
            QPen pen(e.borderColor.isValid() ? e.borderColor : Qt::darkGray, qMax(1.f, e.borderWidth));
            p.setPen(pen);
            p.drawLine(r.topLeft(), r.bottomRight());
        } else if (e.content == "rect") {
            float rx = e.cornerRadius * float(r.width())  / qMax(1.f, e.width);
            float ry = e.cornerRadius * float(r.height()) / qMax(1.f, e.height);
            if (rx > 0 || ry > 0) p.drawRoundedRect(r, rx, ry);
            else                   p.drawRect(r);
        } else {
            p.drawPath(ShapeUtils::shapeToPath(e.content, r, e.customPathData));
        }
        break;
    }
    case SlideElement::Icon:
        p.setPen(Qt::NoPen);
        p.setBrush(e.color.isValid() ? e.color : Qt::black);
        p.drawPath(IconUtils::iconToPath(e.content, r));
        break;
    case SlideElement::Image: {
        bool drawn = false;
        if (!e.content.isEmpty()) {
            QImage img(e.content);
            if (!img.isNull()) { p.drawImage(r.toRect(), img); drawn = true; }
        }
        if (!drawn) {
            p.fillRect(r, QColor(180, 180, 200));
            p.setPen(Qt::darkGray);
            p.drawText(r, Qt::AlignCenter, "[Image]");
        }
        break;
    }
    default:
        p.fillRect(r, QColor(150, 160, 200));
        p.setPen(Qt::white);
        p.drawText(r, Qt::AlignCenter, "[Element]");
        break;
    }
}

// ── ShadowCanvas ─────────────────────────────────────────────────────────

ShadowCanvas::ShadowCanvas(const SlideElement& element, QWidget* parent)
    : QWidget(parent), m_element(element)
{
    setMinimumSize(360, 260);
}

QRectF ShadowCanvas::previewRect() const {
    const double margin = 46.0; // extra room so a large blur/spread isn't clipped
    QRectF avail = rect();
    avail.adjust(margin, margin, -margin, -margin);
    if (avail.width() < 10 || avail.height() < 10) return rect();
    double aspect = double(m_element.width) / qMax(1.0, double(m_element.height));
    double w = avail.width(), h = avail.height();
    if (w / h > aspect) w = h * aspect;
    else                 h = w / aspect;
    QRectF r(0, 0, w, h);
    r.moveCenter(avail.center());
    return r;
}

void ShadowCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(50, 50, 50));

    const QRectF r = previewRect();

    // Checkerboard behind the preview area
    {
        const int cell = 10;
        for (int y = 0; y < int(r.height()); y += cell) {
            for (int x = 0; x < int(r.width()); x += cell) {
                bool dark = ((x / cell) + (y / cell)) % 2 == 0;
                p.fillRect(QRectF(r.left() + x, r.top() + y, cell, cell),
                           dark ? QColor(80, 80, 80) : QColor(110, 110, 110));
            }
        }
    }

    if (m_hasShadow) {
        ShadowRenderer::SilhouettePainter silhouette =
            ShadowRenderer::buildSilhouettePainter(m_element, QRectF(QPointF(0, 0), r.size()));
        QImage shadowImg = ShadowRenderer::render(r.size().toSize(), silhouette, m_blur, m_spread, m_color);
        int pad = ShadowRenderer::padding(m_blur, m_spread);
        p.drawImage(r.topLeft() + QPointF(m_offsetX - pad, m_offsetY - pad), shadowImg);
    }

    p.save();
    paintElementContent(p, m_element, r);
    p.restore();
}

// ── ShadowDialog ─────────────────────────────────────────────────────────

static QWidget* makeSliderRow(QSlider*& sliderOut, int min, int max, int initial, QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    auto* slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(min, max);
    slider->setValue(initial);
    auto* valLabel = new QLabel(QString::number(initial), row);
    valLabel->setMinimumWidth(36);
    valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    h->addWidget(slider, 1);
    h->addWidget(valLabel);
    QObject::connect(slider, &QSlider::valueChanged, valLabel,
                      [valLabel](int v) { valLabel->setText(QString::number(v)); });
    sliderOut = slider;
    return row;
}

ShadowDialog::ShadowDialog(const SlideElement& element, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Edit Shadow");
    resize(480, 640);

    m_angle    = element.shadowAngle;
    m_distance = element.shadowDistance;
    m_offsetX  = element.shadowOffsetX;
    m_offsetY  = element.shadowOffsetY;
    m_color    = element.shadowColor.isValid() ? element.shadowColor : QColor(0, 0, 0, 160);
    m_useOffsetMode = element.shadowUseOffset;

    m_canvas = new ShadowCanvas(element, this);

    m_chkEnabled = new QCheckBox("Shadow active", this);
    m_chkEnabled->setChecked(element.hasShadow);

    m_modeToggleBtn = new QPushButton(this);

    m_angleDistRow = new QWidget(this);
    {
        auto* form = new QFormLayout(m_angleDistRow);
        form->setContentsMargins(0, 0, 0, 0);
        form->addRow("Angle:", makeSliderRow(m_angleSlider, 0, 360, int(qRound(m_angle)), m_angleDistRow));
        form->addRow("Distance:",        makeSliderRow(m_distSlider, 0, 200, int(qRound(m_distance)), m_angleDistRow));
    }
    m_offsetRow = new QWidget(this);
    {
        auto* form = new QFormLayout(m_offsetRow);
        form->setContentsMargins(0, 0, 0, 0);
        form->addRow("Offset X:", makeSliderRow(m_offXSlider, -200, 200, int(qRound(m_offsetX)), m_offsetRow));
        form->addRow("Offset Y:", makeSliderRow(m_offYSlider, -200, 200, int(qRound(m_offsetY)), m_offsetRow));
    }

    auto* mainForm = new QFormLayout();
    mainForm->addRow("Blur:", makeSliderRow(m_blurSlider, 0, 150, int(qRound(element.shadowBlur)), this));
    mainForm->addRow("Spread:", makeSliderRow(m_spreadSlider, 0, 100, int(qRound(element.shadowSpread)), this));

    m_colorBtn = new QPushButton(this);
    connect(m_colorBtn, &QPushButton::clicked, this, &ShadowDialog::onColorClicked);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_canvas, 1);
    layout->addWidget(m_chkEnabled);
    layout->addWidget(m_modeToggleBtn);
    layout->addWidget(m_angleDistRow);
    layout->addWidget(m_offsetRow);
    layout->addLayout(mainForm);
    layout->addWidget(m_colorBtn);
    layout->addWidget(btnBox);

    connect(m_chkEnabled, &QCheckBox::toggled, this, &ShadowDialog::onSlidersChanged);
    connect(m_modeToggleBtn, &QPushButton::clicked, this, &ShadowDialog::onModeToggleClicked);
    connect(m_angleSlider, &QSlider::valueChanged, this, &ShadowDialog::onAngleOrDistanceChanged);
    connect(m_distSlider,  &QSlider::valueChanged, this, &ShadowDialog::onAngleOrDistanceChanged);
    connect(m_offXSlider,  &QSlider::valueChanged, this, &ShadowDialog::onOffsetChanged);
    connect(m_offYSlider,  &QSlider::valueChanged, this, &ShadowDialog::onOffsetChanged);
    connect(m_blurSlider,   &QSlider::valueChanged, this, &ShadowDialog::onSlidersChanged);
    connect(m_spreadSlider, &QSlider::valueChanged, this, &ShadowDialog::onSlidersChanged);

    updateModeVisibility();
    updateColorButtonStyle();
    pushToCanvas();
}

bool ShadowDialog::hasShadow() const { return m_chkEnabled->isChecked(); }
float ShadowDialog::blur()   const { return float(m_blurSlider->value()); }
float ShadowDialog::spread() const { return float(m_spreadSlider->value()); }

void ShadowDialog::recomputeOffsetFromAngle() {
    double rad = qDegreesToRadians(double(m_angle));
    m_offsetX = float(double(m_distance) * qCos(rad));
    m_offsetY = float(double(m_distance) * qSin(rad));
    const QSignalBlocker bx(m_offXSlider), by(m_offYSlider);
    m_offXSlider->setValue(int(qRound(m_offsetX)));
    m_offYSlider->setValue(int(qRound(m_offsetY)));
}

void ShadowDialog::recomputeAngleFromOffset() {
    m_distance = float(qSqrt(double(m_offsetX) * m_offsetX + double(m_offsetY) * m_offsetY));
    double ang = qRadiansToDegrees(qAtan2(double(m_offsetY), double(m_offsetX)));
    if (ang < 0) ang += 360.0;
    m_angle = float(ang);
    const QSignalBlocker ba(m_angleSlider), bd(m_distSlider);
    m_angleSlider->setValue(int(qRound(m_angle)));
    m_distSlider->setValue(int(qRound(m_distance)));
}

void ShadowDialog::updateModeVisibility() {
    m_angleDistRow->setVisible(!m_useOffsetMode);
    m_offsetRow->setVisible(m_useOffsetMode);
    m_modeToggleBtn->setText(m_useOffsetMode
        ? "Mode: X/Y Offset (switch to Angle/Distance)"
        : "Mode: Angle/Distance (switch to X/Y Offset)");
}

void ShadowDialog::onModeToggleClicked() {
    m_useOffsetMode = !m_useOffsetMode;
    updateModeVisibility();
    pushToCanvas();
}

void ShadowDialog::onAngleOrDistanceChanged() {
    m_angle    = float(m_angleSlider->value());
    m_distance = float(m_distSlider->value());
    recomputeOffsetFromAngle();
    pushToCanvas();
}

void ShadowDialog::onOffsetChanged() {
    m_offsetX = float(m_offXSlider->value());
    m_offsetY = float(m_offYSlider->value());
    recomputeAngleFromOffset();
    pushToCanvas();
}

void ShadowDialog::onColorClicked() {
    QColor c = QColorDialog::getColor(m_color, this, "Shadow Color", QColorDialog::ShowAlphaChannel);
    if (!c.isValid()) return;
    m_color = c;
    updateColorButtonStyle();
    pushToCanvas();
}

void ShadowDialog::onSlidersChanged() {
    pushToCanvas();
}

void ShadowDialog::updateColorButtonStyle() {
    QString hex = m_color.name(QColor::HexArgb);
    QString fg  = m_color.lightnessF() > 0.5f ? "#000" : "#fff";
    m_colorBtn->setStyleSheet(QString("background:%1; color:%2; border:1px solid #666;").arg(hex, fg));
    m_colorBtn->setText("Color: " + hex);
}

void ShadowDialog::pushToCanvas() {
    m_canvas->setShadowOn(m_chkEnabled->isChecked());
    m_canvas->setOffset(m_offsetX, m_offsetY);
    m_canvas->setBlur(float(m_blurSlider->value()));
    m_canvas->setSpread(float(m_spreadSlider->value()));
    m_canvas->setColor(m_color);
}
