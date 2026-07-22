#include "BooleanCutDialog.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QToolButton>
#include <QPainter>
#include <QPixmap>
#include <QIcon>

namespace {

struct OpDef { ShapeBoolean::Op op; const char* label; };

// Two overlapping circles, shaded with the actual boolean-op result (reuses
// the same QPainterPath operations the real cut will perform) so the icon
// is an accurate preview, not a hand-drawn approximation.
QPixmap renderOpIcon(ShapeBoolean::Op op, int w, int h) {
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    const double r = h * 0.32;
    const QPointF ca(w * 0.42, h * 0.5), cb(w * 0.58, h * 0.5);
    QPainterPath a, b;
    a.addEllipse(ca, r, r);
    b.addEllipse(cb, r, r);

    QPainterPath result;
    switch (op) {
    case ShapeBoolean::Op::Union:     result = a.united(b); break;
    case ShapeBoolean::Op::Subtract:  result = a.subtracted(b); break;
    case ShapeBoolean::Op::Intersect: result = a.intersected(b); break;
    case ShapeBoolean::Op::Exclude:   result = a.united(b).subtracted(a.intersected(b)); break;
    }

    p.setPen(QPen(QColor(156, 163, 175), 1, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(ca, r, r);
    p.drawEllipse(cb, r, r);

    p.setPen(QPen(QColor(37, 99, 235), 1.2));
    p.setBrush(QColor(96, 165, 250));
    p.drawPath(result);

    return pm;
}

} // namespace

BooleanCutDialog::BooleanCutDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Formen verschneiden");

    static const OpDef ops[] = {
        { ShapeBoolean::Op::Union,     "Vereinigen" },
        { ShapeBoolean::Op::Subtract,  "Subtrahieren" },
        { ShapeBoolean::Op::Intersect, "Schnittmenge" },
        { ShapeBoolean::Op::Exclude,   "Exklusiv-Oder" },
    };

    static const char* BTN_SS =
        "QToolButton { border:1px solid transparent; border-radius:6px;"
        "  padding:4px; font-size:11px; color:#374151; background:transparent; }"
        "QToolButton:hover   { background:#eff6ff; border-color:#93c5fd; }"
        "QToolButton:pressed { background:#dbeafe; border-color:#3b82f6; }";

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    auto* hint = new QLabel("Verschneidungsart wählen:", this);
    hint->setStyleSheet("color:#6b7280; font-size:11px;");
    layout->addWidget(hint);

    auto* grid = new QGridLayout();
    grid->setSpacing(8);
    layout->addLayout(grid);

    const int ICON_W = 72, ICON_H = 56;
    int col = 0;
    for (const auto& def : ops) {
        auto* btn = new QToolButton(this);
        btn->setIcon(QIcon(renderOpIcon(def.op, ICON_W, ICON_H)));
        btn->setIconSize(QSize(ICON_W, ICON_H));
        btn->setText(QString::fromUtf8(def.label));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setFixedSize(96, 92);
        btn->setStyleSheet(BTN_SS);

        const ShapeBoolean::Op op = def.op;
        connect(btn, &QToolButton::clicked, this, [this, op]() {
            m_chosenOp = op;
            accept();
        });
        grid->addWidget(btn, 0, col++);
    }
}
