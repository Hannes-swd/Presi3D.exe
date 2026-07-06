#include "InsertSliderDialog.h"
#include "models/DataModel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>

InsertSliderDialog::InsertSliderDialog(QWidget* parent, Presentation* pres, const QString& currentSlideId,
                                       const SliderConfig& initial)
    : QDialog(parent) {
    bool editing = !initial.boundVariableId.isEmpty();
    setWindowTitle(editing ? "Edit Slider" : "Insert Slider");
    setMinimumWidth(380);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto mkHdr = [this](const QString& text) {
        auto* l = new QLabel(text, this);
        l->setStyleSheet("font-size:14px;font-weight:bold;color:#111827; margin-top:6px;");
        return l;
    };

    layout->addWidget(mkHdr("Variable (Number)"));
    m_varCombo = new QComboBox(this);
    m_varCombo->setStyleSheet("background:#ffffff; color:#111827;");
    if (pres) {
        for (const Variable& v : pres->variables.items) {
            if (v.type != Variable::Number) continue;
            if (!v.scopeSlideId.isEmpty() && v.scopeSlideId != currentSlideId) continue;
            m_varCombo->addItem(v.name, v.id);
        }
    }
    if (m_varCombo->count() == 0) {
        auto* hint = new QLabel(
            "No Number variable yet. Create one first via the \"Variables\" toolbar button.", this);
        hint->setWordWrap(true);
        hint->setStyleSheet("color:#b91c1c;");
        layout->addWidget(hint);
    }
    int idx = m_varCombo->findData(initial.boundVariableId);
    if (idx >= 0) m_varCombo->setCurrentIndex(idx);
    layout->addWidget(m_varCombo);

    auto* rangeRow = new QHBoxLayout();
    auto mkSpin = [this](double value) {
        auto* s = new QDoubleSpinBox(this);
        s->setRange(-1'000'000'000.0, 1'000'000'000.0);
        s->setDecimals(2);
        s->setValue(value);
        return s;
    };
    m_minEdit  = mkSpin(initial.min);
    m_maxEdit  = mkSpin(initial.max);
    m_stepEdit = mkSpin(initial.step);
    rangeRow->addWidget(new QLabel("Min:", this));  rangeRow->addWidget(m_minEdit);
    rangeRow->addWidget(new QLabel("Max:", this));  rangeRow->addWidget(m_maxEdit);
    rangeRow->addWidget(new QLabel("Step:", this)); rangeRow->addWidget(m_stepEdit);
    layout->addWidget(mkHdr("Range"));
    layout->addLayout(rangeRow);

    auto* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bbox->button(QDialogButtonBox::Ok)->setText(editing ? "Apply" : "Insert");
    bbox->button(QDialogButtonBox::Ok)->setEnabled(m_varCombo->count() > 0);
    layout->addWidget(bbox);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

SliderConfig InsertSliderDialog::config() const {
    SliderConfig c;
    c.boundVariableId = m_varCombo->currentData().toString();
    c.min  = m_minEdit->value();
    c.max  = m_maxEdit->value();
    c.step = m_stepEdit->value();
    return c;
}
