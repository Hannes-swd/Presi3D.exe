#include "ImageFillDialog.h"
#include "ShapeUtils.h"
#include "ImageFillRenderer.h"
#include <QPainter>
#include <QMouseEvent>
#include <QImageReader>
#include <QFileDialog>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <algorithm>

// ── ImageFillCanvas ────────────────────────────────────────────────────────

ImageFillCanvas::ImageFillCanvas(const QString& shapeType, QWidget* parent,
                                  const QString& customPathData)
    : QWidget(parent), m_shapeType(shapeType), m_customPathData(customPathData)
{
    setMinimumSize(360, 360);
}

QRectF ImageFillCanvas::shapeRectPx() const {
    const double margin = 20.0;
    QRectF r = rect();
    r.adjust(margin, margin, -margin, -margin);
    if (r.width() < 10 || r.height() < 10) return rect();
    return r;
}

void ImageFillCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(50, 50, 50));

    const QRectF sr = shapeRectPx();

    // Checkerboard behind the shape area so an empty/transparent fill is visible.
    {
        const int cell = 10;
        for (int y = 0; y < int(sr.height()); y += cell) {
            for (int x = 0; x < int(sr.width()); x += cell) {
                bool dark = ((x / cell) + (y / cell)) % 2 == 0;
                p.fillRect(QRectF(sr.left() + x, sr.top() + y, cell, cell),
                           dark ? QColor(80, 80, 80) : QColor(110, 110, 110));
            }
        }
    }

    if (!m_imagePath.isEmpty()) {
        QImage img = ImageFillRenderer::renderImageFill(
            m_shapeType, sr.size().toSize(), m_imagePath, m_offsetX, m_offsetY, m_scale,
            QSizeF(0, 0), m_customPathData);
        p.drawImage(sr.topLeft(), img);
    }

    p.setPen(QPen(QColor(210, 210, 210), 1.5, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawPath(ShapeUtils::shapeToPath(m_shapeType, sr, m_customPathData));
}

void ImageFillCanvas::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() != Qt::LeftButton || m_imagePath.isEmpty()) return;
    m_dragging = true;
    m_lastDragPos = ev->pos();
}

void ImageFillCanvas::mouseMoveEvent(QMouseEvent* ev) {
    if (!m_dragging) return;
    QSize imgSize = QImageReader(m_imagePath).size();
    if (!imgSize.isValid()) return;

    const QRectF sr = shapeRectPx();
    const double coverScale = std::max(sr.width() / imgSize.width(), sr.height() / imgSize.height());
    const double effScale = coverScale * std::max(0.01, double(m_scale));
    const double scaledW = imgSize.width()  * effScale;
    const double scaledH = imgSize.height() * effScale;
    const double slackX = std::max(0.0, scaledW - sr.width());
    const double slackY = std::max(0.0, scaledH - sr.height());

    QPointF delta = ev->pos() - m_lastDragPos;
    m_lastDragPos = ev->pos();
    if (slackX > 1e-6) m_offsetX = qBound(-1.0, m_offsetX + delta.x() / (slackX / 2.0), 1.0);
    if (slackY > 1e-6) m_offsetY = qBound(-1.0, m_offsetY + delta.y() / (slackY / 2.0), 1.0);
    update();
    emit changed();
}

void ImageFillCanvas::mouseReleaseEvent(QMouseEvent*) {
    m_dragging = false;
}

// ── ImageFillDialog ───────────────────────────────────────────────────────

ImageFillDialog::ImageFillDialog(const QString& shapeType, const QString& imagePath,
                                  float offsetX, float offsetY, float scale, QWidget* parent,
                                  const QString& customPathData)
    : QDialog(parent)
{
    setWindowTitle("Textur bearbeiten");
    resize(480, 620);

    m_canvas = new ImageFillCanvas(shapeType, this, customPathData);
    m_canvas->setImagePath(imagePath);
    m_canvas->setOffset(offsetX, offsetY);
    m_canvas->setScale(scale > 0.f ? scale : 1.f);

    auto* hint = new QLabel(
        "Bild wählen, dann im Vorschaubereich ziehen zum Verschieben "
        "und den Regler zum Vergrößern verwenden.", this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#6b7280; font-size:11px;");

    m_chooseBtn = new QPushButton("Bild wählen\xE2\x80\xA6", this);
    connect(m_chooseBtn, &QPushButton::clicked, this, &ImageFillDialog::onChooseImage);

    auto* zoomRow = new QWidget(this);
    auto* zoomLayout = new QHBoxLayout(zoomRow);
    zoomLayout->setContentsMargins(0, 0, 0, 0);
    zoomLayout->addWidget(new QLabel("Zoom:", zoomRow));
    m_zoomSlider = new QSlider(Qt::Horizontal, zoomRow);
    m_zoomSlider->setRange(100, 400);
    m_zoomSlider->setValue(int(m_canvas->scale() * 100));
    zoomLayout->addWidget(m_zoomSlider, 1);
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int v) {
        m_canvas->setScale(v / 100.0f);
    });

    m_hintLabel = new QLabel("Bitte ein Bild auswählen.", this);
    m_hintLabel->setStyleSheet("color:#dc2626; font-weight:bold;");

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okBtn = btnBox->button(QDialogButtonBox::Ok);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(hint);
    layout->addWidget(m_chooseBtn);
    layout->addWidget(m_canvas, 1);
    layout->addWidget(zoomRow);
    layout->addWidget(m_hintLabel);
    layout->addWidget(btnBox);

    connect(m_canvas, &ImageFillCanvas::changed, this, &ImageFillDialog::updateOkEnabled);
    updateOkEnabled();
}

void ImageFillDialog::onChooseImage() {
    QString path = QFileDialog::getOpenFileName(this, "Bild wählen",
        QFileInfo(m_canvas->imagePath()).absolutePath(),
        "Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp);;All Files (*)");
    if (path.isEmpty()) return;
    m_canvas->setImagePath(path);
    m_canvas->setOffset(0.f, 0.f);
}

void ImageFillDialog::updateOkEnabled() {
    bool ok = !m_canvas->imagePath().isEmpty();
    m_okBtn->setEnabled(ok);
    m_hintLabel->setVisible(!ok);
}
