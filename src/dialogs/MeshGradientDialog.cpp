#include "MeshGradientDialog.h"
#include "ShapeUtils.h"
#include "MeshGradientRenderer.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QColorDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QLineF>

// ── MeshGradientCanvas ────────────────────────────────────────────────────────

MeshGradientCanvas::MeshGradientCanvas(const QString& shapeType, QWidget* parent)
    : QWidget(parent), m_shapeType(shapeType)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(360, 360);
}

QRectF MeshGradientCanvas::shapeRectPx() const {
    const double margin = 20.0;
    QRectF r = rect();
    r.adjust(margin, margin, -margin, -margin);
    if (r.width() < 10 || r.height() < 10) return rect();
    return r;
}

int MeshGradientCanvas::hitTestPoint(const QPointF& widgetPos) const {
    const QRectF sr = shapeRectPx();
    const double hitR = 9.0;
    for (int i = m_points.size() - 1; i >= 0; --i) {
        QPointF center = sr.topLeft() + QPointF(m_points[i].x * sr.width(), m_points[i].y * sr.height());
        if (QLineF(center, widgetPos).length() <= hitR) return i;
    }
    return -1;
}

void MeshGradientCanvas::removePoint(int idx) {
    if (idx < 0 || idx >= m_points.size()) return;
    if (m_points.size() <= 3) return; // minimum 3 points enforced
    m_points.removeAt(idx);
    m_dragIndex = -1;
    m_selectedIndex = -1;
    update();
    emit pointsChanged();
}

void MeshGradientCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(50, 50, 50));

    const QRectF sr = shapeRectPx();

    // Checkerboard behind the shape area so fill transparency is visible.
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

    if (m_points.size() >= 3) {
        MeshGradientData mesh{ m_points };
        QImage img = MeshGradientRenderer::renderMeshGradient(m_shapeType, sr.size().toSize(), mesh);
        p.drawImage(sr.topLeft(), img);
    }

    p.setPen(QPen(QColor(210, 210, 210), 1.5, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawPath(ShapeUtils::shapeToPath(m_shapeType, sr));

    // Point handles: mini checkerboard swatch behind the point's own RGBA
    // so alpha is visible even when the point sits over an opaque mesh area.
    for (int i = 0; i < m_points.size(); ++i) {
        const auto& pt = m_points[i];
        QPointF center = sr.topLeft() + QPointF(pt.x * sr.width(), pt.y * sr.height());
        const double R = 8.0;

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(200, 200, 200));
        p.drawEllipse(center, R, R);
        p.setBrush(QColor(120, 120, 120));
        p.drawPie(QRectF(center.x() - R, center.y() - R, R * 2, R * 2), 0, 180 * 16);

        p.setBrush(pt.toQColor());
        p.drawEllipse(center, R, R);

        p.setPen(QPen(i == m_selectedIndex ? QColor(0, 153, 255) : Qt::white,
                      i == m_selectedIndex ? 2.5 : 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(center, R, R);
    }
}

void MeshGradientCanvas::mousePressEvent(QMouseEvent* ev) {
    setFocus();
    if (ev->button() == Qt::RightButton) {
        int idx = hitTestPoint(ev->pos());
        if (idx >= 0) removePoint(idx);
        return;
    }
    if (ev->button() != Qt::LeftButton) return;

    int idx = hitTestPoint(ev->pos());
    if (idx >= 0) {
        m_selectedIndex = idx;
        m_dragIndex = idx;
        update();
        return;
    }

    const QRectF sr = shapeRectPx();
    double nx = qBound(0.0, (ev->pos().x() - sr.left()) / sr.width(),  1.0);
    double ny = qBound(0.0, (ev->pos().y() - sr.top())  / sr.height(), 1.0);
    m_points << MeshGradientPoint::fromQColor(nx, ny, m_lastColor);
    m_selectedIndex = m_points.size() - 1;
    m_dragIndex = m_selectedIndex;
    update();
    emit pointsChanged();
}

void MeshGradientCanvas::mouseMoveEvent(QMouseEvent* ev) {
    if (m_dragIndex < 0 || m_dragIndex >= m_points.size()) return;
    const QRectF sr = shapeRectPx();
    double nx = qBound(0.0, (ev->pos().x() - sr.left()) / sr.width(),  1.0);
    double ny = qBound(0.0, (ev->pos().y() - sr.top())  / sr.height(), 1.0);
    m_points[m_dragIndex].x = nx;
    m_points[m_dragIndex].y = ny;
    update();
    emit pointsChanged();
}

void MeshGradientCanvas::mouseReleaseEvent(QMouseEvent*) {
    m_dragIndex = -1;
}

void MeshGradientCanvas::mouseDoubleClickEvent(QMouseEvent* ev) {
    int idx = hitTestPoint(ev->pos());
    if (idx < 0) return;
    QColor c = QColorDialog::getColor(m_points[idx].toQColor(), this, "Punktfarbe",
                                       QColorDialog::ShowAlphaChannel);
    if (!c.isValid()) return;
    m_points[idx] = MeshGradientPoint::fromQColor(m_points[idx].x, m_points[idx].y, c);
    m_lastColor = c;
    m_selectedIndex = idx;
    update();
    emit pointsChanged();
}

void MeshGradientCanvas::keyPressEvent(QKeyEvent* ev) {
    if ((ev->key() == Qt::Key_Delete || ev->key() == Qt::Key_Backspace) && m_selectedIndex >= 0) {
        removePoint(m_selectedIndex);
        return;
    }
    QWidget::keyPressEvent(ev);
}

// ── MeshGradientDialog ────────────────────────────────────────────────────────

MeshGradientDialog::MeshGradientDialog(const QString& shapeType, const MeshGradientData& initial,
                                       QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Farbverlauf bearbeiten");
    resize(480, 580);

    m_canvas = new MeshGradientCanvas(shapeType, this);
    QVector<MeshGradientPoint> pts = initial.points;
    if (pts.size() < 3) {
        pts = {
            MeshGradientPoint::fromQColor(0.5, 0.08, Qt::white),
            MeshGradientPoint::fromQColor(0.9, 0.9,  Qt::white),
            MeshGradientPoint::fromQColor(0.1, 0.9,  Qt::white),
        };
    }
    m_canvas->setPoints(pts);

    auto* hint = new QLabel(
        "Klicken = Punkt hinzufügen · Ziehen = verschieben · "
        "Doppelklick = Farbe ändern · Rechtsklick/Entf = löschen "
        "(mindestens 3 Punkte)", this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#6b7280; font-size:11px;");

    m_hintLabel = new QLabel("Mindestens 3 Punkte erforderlich.", this);
    m_hintLabel->setStyleSheet("color:#dc2626; font-weight:bold;");

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okBtn = btnBox->button(QDialogButtonBox::Ok);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(hint);
    layout->addWidget(m_canvas, 1);
    layout->addWidget(m_hintLabel);
    layout->addWidget(btnBox);

    connect(m_canvas, &MeshGradientCanvas::pointsChanged, this, &MeshGradientDialog::updateOkEnabled);
    updateOkEnabled();
}

void MeshGradientDialog::updateOkEnabled() {
    bool ok = m_canvas->points().size() >= 3;
    m_okBtn->setEnabled(ok);
    m_hintLabel->setVisible(!ok);
}
