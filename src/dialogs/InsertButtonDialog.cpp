#include "InsertButtonDialog.h"
#include "models/DataModel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QStackedWidget>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>

namespace {
QString typeLabel(Variable::Type t) {
    switch (t) {
    case Variable::Number:  return "Number";
    case Variable::Boolean: return "True/False";
    case Variable::Text:
    default:                return "Text";
    }
}
} // namespace

InsertButtonDialog::InsertButtonDialog(QWidget* parent, Presentation* pres, const QString& currentSlideId,
                                       const ButtonConfig& initial)
    : QDialog(parent), m_pres(pres) {
    bool editing = !initial.label.isEmpty() || !initial.targetSlideId.isEmpty()
                   || !initial.boundVariableId.isEmpty();
    setWindowTitle(editing ? "Edit Button" : "Insert Button");
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto mkHdr = [this](const QString& text) {
        auto* l = new QLabel(text, this);
        l->setStyleSheet("font-size:14px;font-weight:bold;color:#111827; margin-top:6px;");
        return l;
    };

    layout->addWidget(mkHdr("Label"));
    m_labelEdit = new QLineEdit(this);
    m_labelEdit->setText(initial.label.isEmpty() ? "Next" : initial.label);
    m_labelEdit->setPlaceholderText("Button text …");
    m_labelEdit->setStyleSheet("background:#ffffff; color:#111827;");
    layout->addWidget(m_labelEdit);

    layout->addWidget(mkHdr("When clicked"));
    m_actionCombo = new QComboBox(this);
    m_actionCombo->addItem("Go to Slide",        "navigate");
    m_actionCombo->addItem("Change a Variable",  "changeVariable");
    m_actionCombo->setStyleSheet("background:#ffffff; color:#111827;");
    layout->addWidget(m_actionCombo);

    m_actionStack = new QStackedWidget(this);
    layout->addWidget(m_actionStack);

    // ── Page 0: Go to Slide ──────────────────────────────────────────────
    auto* navPage = new QWidget(m_actionStack);
    auto* navLayout = new QVBoxLayout(navPage);
    navLayout->setContentsMargins(0, 4, 0, 0);
    m_targetCombo = new QComboBox(navPage);
    m_targetCombo->setStyleSheet("background:#ffffff; color:#111827;");
    if (pres) {
        for (const Slide& s : pres->slides)
            m_targetCombo->addItem(s.name.isEmpty() ? QString("Slide %1").arg(m_targetCombo->count() + 1) : s.name, s.id);
    }
    int tIdx = m_targetCombo->findData(initial.targetSlideId);
    if (tIdx >= 0) m_targetCombo->setCurrentIndex(tIdx);
    navLayout->addWidget(m_targetCombo);
    m_actionStack->addWidget(navPage);

    // ── Page 1: Change a Variable ────────────────────────────────────────
    auto* varPage = new QWidget(m_actionStack);
    auto* varLayout = new QVBoxLayout(varPage);
    varLayout->setContentsMargins(0, 4, 0, 0);
    varLayout->setSpacing(6);

    m_varCombo = new QComboBox(varPage);
    m_varCombo->setStyleSheet("background:#ffffff; color:#111827;");
    if (pres) {
        for (const Variable& v : pres->variables.items) {
            if (!v.scopeSlideId.isEmpty() && v.scopeSlideId != currentSlideId) continue;
            m_varCombo->addItem(v.name + "  (" + typeLabel(v.type) + ")", v.id);
        }
    }
    if (m_varCombo->count() == 0) {
        auto* hint = new QLabel("No variables yet. Create one first via the \"Variables\" toolbar button.", varPage);
        hint->setWordWrap(true);
        hint->setStyleSheet("color:#b91c1c;");
        varLayout->addWidget(hint);
    }
    varLayout->addWidget(m_varCombo);

    m_opCombo = new QComboBox(varPage);
    m_opCombo->setStyleSheet("background:#ffffff; color:#111827;");
    varLayout->addWidget(m_opCombo);

    m_valueRow = new QWidget(varPage);
    auto* valueRowLayout = new QHBoxLayout(m_valueRow);
    valueRowLayout->setContentsMargins(0, 0, 0, 0);
    valueRowLayout->addWidget(new QLabel("Value:", m_valueRow));
    m_valueStack = new QStackedWidget(m_valueRow);
    m_numberValueEdit = new QDoubleSpinBox(m_valueStack);
    m_numberValueEdit->setRange(-1'000'000'000.0, 1'000'000'000.0);
    m_numberValueEdit->setDecimals(4);
    m_numberValueEdit->setValue(initial.varOpNumber);
    m_boolValueEdit = new QCheckBox("True", m_valueStack);
    m_boolValueEdit->setChecked(initial.varOpBool);
    m_textValueEdit = new QLineEdit(initial.varOpText, m_valueStack);
    m_valueStack->addWidget(m_numberValueEdit); // 0 == Variable::Number
    m_valueStack->addWidget(m_boolValueEdit);   // 1 == Variable::Boolean ("Set to")
    m_valueStack->addWidget(m_textValueEdit);   // 2 == Variable::Text
    valueRowLayout->addWidget(m_valueStack, 1);
    varLayout->addWidget(m_valueRow);

    varLayout->addStretch();
    m_actionStack->addWidget(varPage);

    int aIdx = m_actionCombo->findData(initial.action);
    m_actionCombo->setCurrentIndex(aIdx >= 0 ? aIdx : 0);
    m_actionStack->setCurrentIndex(m_actionCombo->currentIndex());
    connect(m_actionCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        m_actionStack->setCurrentIndex(idx);
    });

    int vIdx = m_varCombo->findData(initial.boundVariableId);
    if (vIdx >= 0) m_varCombo->setCurrentIndex(vIdx);
    connect(m_varCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        refreshOpOptionsForSelectedVariable();
    });
    refreshOpOptionsForSelectedVariable(); // builds op list, then we can select the initial op
    int opIdx = m_opCombo->findData(initial.varOp);
    if (opIdx >= 0) m_opCombo->setCurrentIndex(opIdx);
    connect(m_opCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        refreshValuePageVisibility();
    });
    refreshValuePageVisibility();

    auto* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bbox->button(QDialogButtonBox::Ok)->setText(editing ? "Apply" : "Insert");
    layout->addWidget(bbox);
    connect(bbox, &QDialogButtonBox::accepted, this, [this]() {
        if (m_actionCombo->currentData().toString() == "changeVariable" && m_varCombo->count() == 0) {
            QMessageBox::warning(this, "No Variable Selected",
                "Create a variable first (via the \"Variables\" toolbar button), or choose \"Go to Slide\" instead.");
            return;
        }
        accept();
    });
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_labelEdit->setFocus();
    m_labelEdit->selectAll();
}

void InsertButtonDialog::refreshOpOptionsForSelectedVariable() {
    QString varId = m_varCombo->currentData().toString();
    const Variable* v = (m_pres && !varId.isEmpty()) ? m_pres->variables.findById(varId) : nullptr;

    QString keepOp = m_opCombo->currentData().toString();
    m_opCombo->clear();
    Variable::Type type = v ? v->type : Variable::Number;
    switch (type) {
    case Variable::Number:
        m_opCombo->addItem("Increase by", "inc");
        m_opCombo->addItem("Decrease by", "dec");
        m_opCombo->addItem("Set to",      "set");
        break;
    case Variable::Boolean:
        m_opCombo->addItem("Toggle",  "toggle");
        m_opCombo->addItem("Set to",  "set");
        break;
    case Variable::Text:
        m_opCombo->addItem("Set to", "set");
        break;
    }
    int idx = m_opCombo->findData(keepOp);
    m_opCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    m_valueStack->setCurrentIndex(int(type));
    refreshValuePageVisibility();
}

void InsertButtonDialog::refreshValuePageVisibility() {
    bool needsValue = m_opCombo->currentData().toString() != "toggle";
    m_valueRow->setVisible(needsValue);
}

ButtonConfig InsertButtonDialog::config() const {
    ButtonConfig c;
    c.label          = m_labelEdit->text().trimmed();
    c.action         = m_actionCombo->currentData().toString();
    c.targetSlideId  = m_targetCombo->currentData().toString();
    c.boundVariableId = m_varCombo->currentData().toString();
    c.varOp          = m_opCombo->currentData().toString();
    c.varOpNumber    = m_numberValueEdit->value();
    c.varOpText      = m_textValueEdit->text();
    c.varOpBool      = m_boolValueEdit->isChecked();
    return c;
}
