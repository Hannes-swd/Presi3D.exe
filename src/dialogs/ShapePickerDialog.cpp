#define _USE_MATH_DEFINES
#include "ShapePickerDialog.h"
#include <cmath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QFrame>
#include <QPainter>
#include <QPixmap>
#include <QIcon>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── Internal helpers ──────────────────────────────────────────────────────────

static void regularPolygon(QPainterPath& path, int n, double cx, double cy,
                            double r, double startAngle)
{
    for (int i = 0; i < n; ++i) {
        double a = startAngle + i * 2.0 * M_PI / n;
        double x = cx + r * std::cos(a);
        double y = cy + r * std::sin(a);
        (i == 0) ? path.moveTo(x, y) : path.lineTo(x, y);
    }
    path.closeSubpath();
}

static void starPolygon(QPainterPath& path, int n, double cx, double cy,
                        double outerR, double innerR, double startAngle)
{
    for (int i = 0; i < 2 * n; ++i) {
        double r = (i % 2 == 0) ? outerR : innerR;
        double a = startAngle + i * M_PI / n;
        double x = cx + r * std::cos(a);
        double y = cy + r * std::sin(a);
        (i == 0) ? path.moveTo(x, y) : path.lineTo(x, y);
    }
    path.closeSubpath();
}

// ── ShapeUtils ────────────────────────────────────────────────────────────────

const QVector<ShapeUtils::ShapeDef>& ShapeUtils::allShapes()
{
    static const QVector<ShapeDef> list = {
        // Grundformen
        { "rect",           "Rechteck",             "Grundformen" },
        { "circle",         "Ellipse",              "Grundformen" },
        { "triangle",       "Dreieck",              "Grundformen" },
        { "right-triangle", "Rechtwinkl. Dreieck",  "Grundformen" },
        { "diamond",        "Raute",                "Grundformen" },
        { "parallelogram",  "Parallelogramm",       "Grundformen" },
        { "trapezoid",      "Trapez",               "Grundformen" },
        { "cross",          "Kreuz",                "Grundformen" },
        // Polygone
        { "pentagon",       "Fünfeck",              "Polygone"    },
        { "hexagon",        "Sechseck",             "Polygone"    },
        { "octagon",        "Achteck",              "Polygone"    },
        // Sterne
        { "star4",          "Stern (4)",            "Sterne"      },
        { "star5",          "Stern (5)",            "Sterne"      },
        { "star6",          "Stern (6)",            "Sterne"      },
        { "star8",          "Stern (8)",            "Sterne"      },
        // Pfeile
        { "arrow-right",    "Pfeil rechts",         "Pfeile"      },
        { "arrow-left",     "Pfeil links",          "Pfeile"      },
        { "arrow-up",       "Pfeil oben",           "Pfeile"      },
        { "arrow-down",     "Pfeil unten",          "Pfeile"      },
        { "arrow-lr",       "Doppelpfeil H",            "Pfeile"  },
        { "arrow-ud",       "Doppelpfeil V",            "Pfeile"  },
        { "chevron-right",  "Chevron rechts",       "Pfeile"      },
        { "chevron-left",   "Chevron links",        "Pfeile"      },
        // Sonstiges
        { "heart",          "Herz",                 "Sonstiges"   },
    };
    return list;
}

QPainterPath ShapeUtils::shapeToPath(const QString& type, const QRectF& r)
{
    QPainterPath path;
    const double cx = r.center().x(), cy = r.center().y();
    const double w = r.width(),       h  = r.height();
    const double hw = w * 0.5,        hh = h * 0.5;

    if (type == "circle") {
        path.addEllipse(r);

    } else if (type == "triangle") {
        path.moveTo(cx,        r.top());
        path.lineTo(r.right(), r.bottom());
        path.lineTo(r.left(),  r.bottom());
        path.closeSubpath();

    } else if (type == "right-triangle") {
        path.moveTo(r.left(),  r.top());
        path.lineTo(r.right(), r.bottom());
        path.lineTo(r.left(),  r.bottom());
        path.closeSubpath();

    } else if (type == "diamond") {
        path.moveTo(cx,        r.top());
        path.lineTo(r.right(), cy);
        path.lineTo(cx,        r.bottom());
        path.lineTo(r.left(),  cy);
        path.closeSubpath();

    } else if (type == "parallelogram") {
        double off = w * 0.2;
        path.moveTo(r.left() + off,  r.top());
        path.lineTo(r.right(),       r.top());
        path.lineTo(r.right() - off, r.bottom());
        path.lineTo(r.left(),        r.bottom());
        path.closeSubpath();

    } else if (type == "trapezoid") {
        double off = w * 0.15;
        path.moveTo(r.left() + off,  r.top());
        path.lineTo(r.right() - off, r.top());
        path.lineTo(r.right(),       r.bottom());
        path.lineTo(r.left(),        r.bottom());
        path.closeSubpath();

    } else if (type == "cross") {
        double t = qMin(hw, hh) * 0.55;
        path.moveTo(cx - t,    r.top());
        path.lineTo(cx + t,    r.top());
        path.lineTo(cx + t,    cy - t);
        path.lineTo(r.right(), cy - t);
        path.lineTo(r.right(), cy + t);
        path.lineTo(cx + t,    cy + t);
        path.lineTo(cx + t,    r.bottom());
        path.lineTo(cx - t,    r.bottom());
        path.lineTo(cx - t,    cy + t);
        path.lineTo(r.left(),  cy + t);
        path.lineTo(r.left(),  cy - t);
        path.lineTo(cx - t,    cy - t);
        path.closeSubpath();

    } else if (type == "pentagon") {
        regularPolygon(path, 5, cx, cy, qMin(hw, hh) * 0.95, -M_PI / 2.0);

    } else if (type == "hexagon") {
        regularPolygon(path, 6, cx, cy, qMin(hw, hh) * 0.95, 0.0);

    } else if (type == "octagon") {
        regularPolygon(path, 8, cx, cy, qMin(hw, hh) * 0.95, M_PI / 8.0);

    } else if (type == "star4") {
        double R = qMin(hw, hh) * 0.95;
        starPolygon(path, 4, cx, cy, R, R * 0.40, -M_PI / 2.0);

    } else if (type == "star5") {
        double R = qMin(hw, hh) * 0.95;
        starPolygon(path, 5, cx, cy, R, R * 0.38, -M_PI / 2.0);

    } else if (type == "star6") {
        double R = qMin(hw, hh) * 0.95;
        starPolygon(path, 6, cx, cy, R, R * 0.50, -M_PI / 2.0);

    } else if (type == "star8") {
        double R = qMin(hw, hh) * 0.95;
        starPolygon(path, 8, cx, cy, R, R * 0.55, -M_PI / 2.0);

    } else if (type == "arrow-right") {
        double ahW = w * 0.35, shH = h * 0.38;
        path.moveTo(r.left(),        cy - shH);
        path.lineTo(r.right() - ahW, cy - shH);
        path.lineTo(r.right() - ahW, r.top());
        path.lineTo(r.right(),       cy);
        path.lineTo(r.right() - ahW, r.bottom());
        path.lineTo(r.right() - ahW, cy + shH);
        path.lineTo(r.left(),        cy + shH);
        path.closeSubpath();

    } else if (type == "arrow-left") {
        double ahW = w * 0.35, shH = h * 0.38;
        path.moveTo(r.right(),       cy - shH);
        path.lineTo(r.left() + ahW,  cy - shH);
        path.lineTo(r.left() + ahW,  r.top());
        path.lineTo(r.left(),        cy);
        path.lineTo(r.left() + ahW,  r.bottom());
        path.lineTo(r.left() + ahW,  cy + shH);
        path.lineTo(r.right(),       cy + shH);
        path.closeSubpath();

    } else if (type == "arrow-up") {
        double ahH = h * 0.35, shW = w * 0.38;
        path.moveTo(cx - shW,        r.bottom());
        path.lineTo(cx - shW,        r.top() + ahH);
        path.lineTo(r.left(),        r.top() + ahH);
        path.lineTo(cx,              r.top());
        path.lineTo(r.right(),       r.top() + ahH);
        path.lineTo(cx + shW,        r.top() + ahH);
        path.lineTo(cx + shW,        r.bottom());
        path.closeSubpath();

    } else if (type == "arrow-down") {
        double ahH = h * 0.35, shW = w * 0.38;
        path.moveTo(cx - shW,        r.top());
        path.lineTo(cx - shW,        r.bottom() - ahH);
        path.lineTo(r.left(),        r.bottom() - ahH);
        path.lineTo(cx,              r.bottom());
        path.lineTo(r.right(),       r.bottom() - ahH);
        path.lineTo(cx + shW,        r.bottom() - ahH);
        path.lineTo(cx + shW,        r.top());
        path.closeSubpath();

    } else if (type == "arrow-lr") {
        double ahW = w * 0.28, shH = h * 0.38;
        path.moveTo(r.left(),        cy);
        path.lineTo(r.left() + ahW,  r.top());
        path.lineTo(r.left() + ahW,  cy - shH);
        path.lineTo(r.right() - ahW, cy - shH);
        path.lineTo(r.right() - ahW, r.top());
        path.lineTo(r.right(),       cy);
        path.lineTo(r.right() - ahW, r.bottom());
        path.lineTo(r.right() - ahW, cy + shH);
        path.lineTo(r.left() + ahW,  cy + shH);
        path.lineTo(r.left() + ahW,  r.bottom());
        path.closeSubpath();

    } else if (type == "arrow-ud") {
        double ahH = h * 0.28, shW = w * 0.38;
        path.moveTo(cx,              r.top());
        path.lineTo(r.right(),       r.top() + ahH);
        path.lineTo(cx + shW,        r.top() + ahH);
        path.lineTo(cx + shW,        r.bottom() - ahH);
        path.lineTo(r.right(),       r.bottom() - ahH);
        path.lineTo(cx,              r.bottom());
        path.lineTo(r.left(),        r.bottom() - ahH);
        path.lineTo(cx - shW,        r.bottom() - ahH);
        path.lineTo(cx - shW,        r.top() + ahH);
        path.lineTo(r.left(),        r.top() + ahH);
        path.closeSubpath();

    } else if (type == "chevron-right") {
        double notch = w * 0.28;
        path.moveTo(r.left(),          r.top());
        path.lineTo(r.right() - notch, r.top());
        path.lineTo(r.right(),         cy);
        path.lineTo(r.right() - notch, r.bottom());
        path.lineTo(r.left(),          r.bottom());
        path.lineTo(r.left() + notch,  cy);
        path.closeSubpath();

    } else if (type == "chevron-left") {
        double notch = w * 0.28;
        path.moveTo(r.right(),         r.top());
        path.lineTo(r.left() + notch,  r.top());
        path.lineTo(r.left(),          cy);
        path.lineTo(r.left() + notch,  r.bottom());
        path.lineTo(r.right(),         r.bottom());
        path.lineTo(r.right() - notch, cy);
        path.closeSubpath();

    } else if (type == "heart") {
        path.moveTo(cx, r.bottom());
        path.cubicTo(r.left() + w*0.02, cy + h*0.25,
                     r.left(),           cy - h*0.05,
                     r.left() + w*0.15,  cy - h*0.22);
        path.cubicTo(r.left() + w*0.28,  cy - h*0.38,
                     cx - w*0.03,        cy - h*0.38,
                     cx,                 cy - h*0.22);
        path.cubicTo(cx + w*0.03,         cy - h*0.38,
                     r.right() - w*0.28,  cy - h*0.38,
                     r.right() - w*0.15,  cy - h*0.22);
        path.cubicTo(r.right(),           cy - h*0.05,
                     r.right() - w*0.02, cy + h*0.25,
                     cx,                 r.bottom());
        path.closeSubpath();

    } else {
        // "rect" and any unknown type
        path.addRect(r);
    }
    return path;
}

QString ShapeUtils::shapeToCssStyle(const QString& type)
{
    if (type == "circle")
        return "border-radius:50%";
    if (type == "triangle")
        return "clip-path:polygon(50% 0%,100% 100%,0% 100%)";
    if (type == "right-triangle")
        return "clip-path:polygon(0% 0%,100% 100%,0% 100%)";
    if (type == "diamond")
        return "clip-path:polygon(50% 0%,100% 50%,50% 100%,0% 50%)";
    if (type == "parallelogram")
        return "clip-path:polygon(20% 0%,100% 0%,80% 100%,0% 100%)";
    if (type == "trapezoid")
        return "clip-path:polygon(15% 0%,85% 0%,100% 100%,0% 100%)";
    if (type == "cross")
        return "clip-path:polygon(35% 0%,65% 0%,65% 35%,100% 35%,100% 65%,65% 65%,65% 100%,35% 100%,35% 65%,0% 65%,0% 35%,35% 35%)";
    if (type == "pentagon")
        return "clip-path:polygon(50% 0%,100% 38%,82% 100%,18% 100%,0% 38%)";
    if (type == "hexagon")
        return "clip-path:polygon(50% 0%,100% 25%,100% 75%,50% 100%,0% 75%,0% 25%)";
    if (type == "octagon")
        return "clip-path:polygon(30% 0%,70% 0%,100% 30%,100% 70%,70% 100%,30% 100%,0% 70%,0% 30%)";
    if (type == "star4")
        return "clip-path:polygon(50% 0%,61% 39%,100% 50%,61% 61%,50% 100%,39% 61%,0% 50%,39% 39%)";
    if (type == "star5")
        return "clip-path:polygon(50% 0%,61% 35%,98% 35%,68% 57%,79% 91%,50% 70%,21% 91%,32% 57%,2% 35%,39% 35%)";
    if (type == "star6")
        return "clip-path:polygon(50% 0%,59% 30%,89% 18%,74% 47%,95% 68%,64% 65%,50% 100%,36% 65%,5% 68%,26% 47%,11% 18%,41% 30%)";
    if (type == "star8")
        return "clip-path:polygon(50% 0%,57% 27%,74% 6%,72% 33%,93% 20%,78% 44%,100% 50%,78% 56%,93% 80%,72% 67%,74% 94%,57% 73%,50% 100%,43% 73%,26% 94%,28% 67%,7% 80%,22% 56%,0% 50%,22% 44%,7% 20%,28% 33%,26% 6%,43% 27%)";
    if (type == "arrow-right")
        return "clip-path:polygon(0% 31%,65% 31%,65% 0%,100% 50%,65% 100%,65% 69%,0% 69%)";
    if (type == "arrow-left")
        return "clip-path:polygon(100% 31%,35% 31%,35% 0%,0% 50%,35% 100%,35% 69%,100% 69%)";
    if (type == "arrow-up")
        return "clip-path:polygon(31% 100%,31% 35%,0% 35%,50% 0%,100% 35%,69% 35%,69% 100%)";
    if (type == "arrow-down")
        return "clip-path:polygon(31% 0%,31% 65%,0% 65%,50% 100%,100% 65%,69% 65%,69% 0%)";
    if (type == "arrow-lr")
        return "clip-path:polygon(0% 50%,28% 0%,28% 31%,72% 31%,72% 0%,100% 50%,72% 100%,72% 69%,28% 69%,28% 100%)";
    if (type == "arrow-ud")
        return "clip-path:polygon(50% 0%,100% 28%,69% 28%,69% 72%,100% 72%,50% 100%,0% 72%,31% 72%,31% 28%,0% 28%)";
    if (type == "chevron-right")
        return "clip-path:polygon(0% 0%,72% 0%,100% 50%,72% 100%,0% 100%,28% 50%)";
    if (type == "chevron-left")
        return "clip-path:polygon(100% 0%,28% 0%,0% 50%,28% 100%,100% 100%,72% 50%)";
    return ""; // rect or unknown — no special clip
}

// ── ShapePickerDialog ─────────────────────────────────────────────────────────

ShapePickerDialog::ShapePickerDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Form einfügen");
    setMinimumSize(560, 460);
    resize(580, 520);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(6);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(scroll);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(4, 4, 4, 8);
    contentLayout->setSpacing(2);

    const int COLS   = 6;
    const int ICON_W = 56, ICON_H = 44;
    const int BTN_W  = 84, BTN_H  = 76;

    static const char* BTN_SS =
        "QToolButton { border:1px solid transparent; border-radius:5px;"
        "  padding:1px; font-size:9px; color:#374151; background:transparent; }"
        "QToolButton:hover   { background:#eff6ff; border-color:#93c5fd; }"
        "QToolButton:pressed { background:#dbeafe; border-color:#3b82f6; }";

    QString currentCat;
    QGridLayout* grid = nullptr;
    int col = 0;

    for (const auto& def : ShapeUtils::allShapes()) {
        if (def.category != currentCat) {
            currentCat = def.category;
            col = 0;

            auto* catLbl = new QLabel(def.category, content);
            QFont cf = catLbl->font();
            cf.setBold(true);
            catLbl->setFont(cf);
            catLbl->setStyleSheet("color:#6b7280; margin-top:8px; font-size:11px;");
            contentLayout->addWidget(catLbl);

            auto* sep = new QFrame(content);
            sep->setFrameShape(QFrame::HLine);
            sep->setStyleSheet("border:none; border-top:1px solid #e5e7eb; margin-bottom:2px;");
            contentLayout->addWidget(sep);

            auto* gridWidget = new QWidget(content);
            grid = new QGridLayout(gridWidget);
            grid->setContentsMargins(0, 0, 0, 0);
            grid->setSpacing(3);
            contentLayout->addWidget(gridWidget);
        }

        // Render shape icon
        QPixmap pm(ICON_W, ICON_H);
        pm.fill(Qt::transparent);
        {
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            QPainterPath spath = ShapeUtils::shapeToPath(
                def.id, QRectF(3, 3, ICON_W - 6, ICON_H - 6));
            p.setBrush(QColor(96, 165, 250));
            p.setPen(QPen(QColor(37, 99, 235), 1.2));
            p.drawPath(spath);
        }

        auto* btn = new QToolButton(content);
        btn->setIcon(QIcon(pm));
        btn->setIconSize(QSize(ICON_W, ICON_H));
        btn->setText(def.label);
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setFixedSize(BTN_W, BTN_H);
        btn->setStyleSheet(BTN_SS);

        const QString shapeId = def.id;
        connect(btn, &QToolButton::clicked, this, [this, shapeId]() {
            m_selected = shapeId;
            accept();
        });

        grid->addWidget(btn, col / COLS, col % COLS);
        ++col;
    }

    contentLayout->addStretch();
    scroll->setWidget(content);
    mainLayout->addWidget(scroll, 1);

    // Bottom bar
    auto* bottomBar = new QHBoxLayout();
    bottomBar->addStretch();
    auto* cancelBtn = new QPushButton("Abbrechen", this);
    cancelBtn->setFixedWidth(110);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    bottomBar->addWidget(cancelBtn);
    mainLayout->addLayout(bottomBar);
}
