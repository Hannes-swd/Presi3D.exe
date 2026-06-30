#include "InsertTableDialog.h"
#include <QPainter>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QLabel>

// ── TablePickerWidget ─────────────────────────────────────────────────────────

TablePickerWidget::TablePickerWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFixedSize(MAXC * CELL + 2, MAXR * CELL + 2);
}

void TablePickerWidget::setSelection(int rows, int cols) {
    m_rows = qBound(1, rows, MAXR);
    m_cols = qBound(1, cols, MAXC);
    update();
}

void TablePickerWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    for (int r = 0; r < MAXR; ++r) {
        for (int c = 0; c < MAXC; ++c) {
            QRect cell(c * CELL + 1, r * CELL + 1, CELL - 1, CELL - 1);
            bool sel = r < m_rows && c < m_cols;
            p.fillRect(cell, sel ? QColor(60, 120, 220) : QColor(245, 245, 245));
            p.setPen(sel ? QColor(40, 80, 180) : QColor(210, 210, 210));
            p.drawRect(cell);
        }
    }
}

void TablePickerWidget::mouseMoveEvent(QMouseEvent* e) {
    int c = qBound(1, int(e->position().x() / CELL) + 1, MAXC);
    int r = qBound(1, int(e->position().y() / CELL) + 1, MAXR);
    if (c != m_cols || r != m_rows) {
        m_cols = c; m_rows = r;
        update();
        emit selectionChanged(m_rows, m_cols);
    }
}

void TablePickerWidget::mousePressEvent(QMouseEvent*) {
    emit selectionChanged(m_rows, m_cols);
    if (auto* dlg = qobject_cast<QDialog*>(window()))
        dlg->accept();
}

// ── InsertTableDialog ─────────────────────────────────────────────────────────

InsertTableDialog::InsertTableDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Tabelle einfügen");
    setModal(true);

    auto* vbox = new QVBoxLayout(this);
    vbox->setSpacing(8);

    m_label = new QLabel("3 × 3 Tabelle", this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setStyleSheet("font-weight: bold; font-size: 13px; padding: 4px;");
    vbox->addWidget(m_label);

    m_picker = new TablePickerWidget(this);
    vbox->addWidget(m_picker, 0, Qt::AlignCenter);

    auto* hint = new QLabel("Zeiger über das Raster bewegen oder Zahlen eingeben:", this);
    hint->setStyleSheet("color: #6b7280; font-size: 10px;");
    vbox->addWidget(hint);

    auto* spinRow = new QHBoxLayout;
    spinRow->addWidget(new QLabel("Zeilen:", this));
    m_rowSpin = new QSpinBox(this);
    m_rowSpin->setRange(1, 50);
    m_rowSpin->setValue(3);
    spinRow->addWidget(m_rowSpin);
    spinRow->addSpacing(12);
    spinRow->addWidget(new QLabel("Spalten:", this));
    m_colSpin = new QSpinBox(this);
    m_colSpin->setRange(1, 20);
    m_colSpin->setValue(3);
    spinRow->addWidget(m_colSpin);
    vbox->addLayout(spinRow);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    vbox->addWidget(buttons);

    auto sync = [this](int r, int c) {
        m_label->setText(QString("%1 × %2 Tabelle").arg(r).arg(c));
        if (!m_upd) {
            m_upd = true;
            m_rowSpin->setValue(r);
            m_colSpin->setValue(c);
            m_picker->setSelection(r, c);
            m_upd = false;
        }
    };

    connect(m_picker,  &TablePickerWidget::selectionChanged, this, [this, sync](int r, int c) {
        if (!m_upd) { m_upd = true; sync(r, c); m_upd = false; }
    });
    connect(m_rowSpin, &QSpinBox::valueChanged, this, [this, sync](int v) {
        if (!m_upd) { m_upd = true; sync(v, m_colSpin->value()); m_upd = false; }
    });
    connect(m_colSpin, &QSpinBox::valueChanged, this, [this, sync](int v) {
        if (!m_upd) { m_upd = true; sync(m_rowSpin->value(), v); m_upd = false; }
    });

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

int InsertTableDialog::rows() const { return m_rowSpin->value(); }
int InsertTableDialog::cols() const { return m_colSpin->value(); }
