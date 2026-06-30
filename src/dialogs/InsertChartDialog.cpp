#include "InsertChartDialog.h"
#include "rendering/ChartRenderer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFrame>
#include <QPainter>

// ─── Mini preview button for one chart type ───────────────────────────────────

class ChartTypeButton : public QPushButton {
public:
    const QString chartType; // public so lambda can read it

    ChartTypeButton(const QString& type, QWidget* parent = nullptr)
        : QPushButton(parent), chartType(type) {
        setCheckable(true);
        setFixedSize(110, 88);

        // Render a small preview once
        m_preview = QPixmap(90, 56);
        m_preview.fill(Qt::white);
        QPainter p(&m_preview);
        p.setRenderHint(QPainter::Antialiasing);
        ChartData def = ChartData::createDefault(type);
        ChartRenderer::paint(p, QRectF(1, 1, 88, 54), def);
        p.end();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        bool sel = isChecked(), hov = underMouse();

        QColor bg = sel ? QColor(239,246,255) : (hov ? QColor(249,249,249) : Qt::white);
        QColor bord = sel ? QColor(59,130,246) : QColor(209,213,219);
        p.fillRect(rect(), bg);
        p.setPen(QPen(bord, sel ? 2.f : 1.f));
        p.drawRoundedRect(rect().adjusted(1,1,-1,-1), 6, 6);

        p.drawPixmap(QPoint((width() - m_preview.width()) / 2, 4), m_preview);

        p.setPen(sel ? QColor(37,99,235) : QColor(55,65,81));
        QFont f("Arial", 8); f.setBold(sel);
        p.setFont(f);
        p.drawText(QRectF(2, m_preview.height() + 6, width()-4, 20),
                   Qt::AlignHCenter | Qt::AlignTop, ChartRenderer::typeName(chartType));
    }

private:
    QPixmap m_preview;
};

// ─── Dialog ───────────────────────────────────────────────────────────────────

InsertChartDialog::InsertChartDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Diagramm einfügen");
    setMinimumWidth(500);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(16, 16, 16, 12);

    auto* hdr = new QLabel("Diagrammtyp auswählen", this);
    hdr->setStyleSheet("font-size:14px;font-weight:bold;color:#111827;");
    layout->addWidget(hdr);

    static const struct { const char* type; const char* cat; } TYPES[] = {
        {"bar",       "Datendiagramme"},
        {"bar_h",     "Datendiagramme"},
        {"line",      "Datendiagramme"},
        {"area",      "Datendiagramme"},
        {"pie",       "Datendiagramme"},
        {"donut",     "Datendiagramme"},
        {"scatter",   "Datendiagramme"},
        {"flowchart", "Strukturdiagramme"},
        {"mindmap",   "Strukturdiagramme"},
        {"orgchart",  "Strukturdiagramme"},
        {"uml",       "Strukturdiagramme"},
        {"timeline",  "Spezialdiagramme"},
        {"gantt",     "Spezialdiagramme"},
        {"venn",      "Spezialdiagramme"},
        {nullptr, nullptr}
    };

    // Collect all buttons first so we can wire up mutual exclusion after
    QVector<ChartTypeButton*> allBtns;

    QString curCat;
    QHBoxLayout* rowLayout = nullptr;

    for (int i = 0; TYPES[i].type; ++i) {
        if (curCat != TYPES[i].cat) {
            curCat = TYPES[i].cat;
            auto* lbl = new QLabel(curCat, this);
            lbl->setStyleSheet("font-size:10px;font-weight:bold;color:#6b7280;"
                               "letter-spacing:1px;margin-top:4px;");
            layout->addWidget(lbl);
            auto* rowW = new QWidget(this);
            rowLayout = new QHBoxLayout(rowW);
            rowLayout->setContentsMargins(0,0,0,0);
            rowLayout->setSpacing(5);
            layout->addWidget(rowW);
        }
        auto* btn = new ChartTypeButton(TYPES[i].type, this);
        allBtns.append(btn);
        rowLayout->addWidget(btn);
        if (!TYPES[i+1].type || curCat != TYPES[i+1].cat)
            rowLayout->addStretch();
    }

    // Mutual exclusion + type tracking
    for (ChartTypeButton* btn : allBtns) {
        connect(btn, &QPushButton::clicked, this, [this, btn, allBtns]() {
            for (auto* b : allBtns) if (b != btn) b->setChecked(false);
            btn->setChecked(true);
            m_type = btn->chartType;
        });
    }

    // Pre-select bar
    if (!allBtns.isEmpty()) {
        allBtns.first()->setChecked(true);
        m_type = allBtns.first()->chartType;
    }

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color:#e5e7eb;");
    layout->addWidget(sep);

    auto* bbox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bbox->button(QDialogButtonBox::Ok)->setText("Einfügen");
    layout->addWidget(bbox);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
