#include "InsertCheckboxDialog.h"
#include "models/DataModel.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>

InsertCheckboxDialog::InsertCheckboxDialog(QWidget* parent, Presentation* pres, const QString& currentSlideId,
                                           const CheckboxConfig& initial)
    : QDialog(parent) {
    bool editing = !initial.boundVariableId.isEmpty();
    setWindowTitle(editing ? "Edit Checkbox" : "Insert Checkbox");
    setMinimumWidth(380);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto mkHdr = [this](const QString& text) {
        auto* l = new QLabel(text, this);
        l->setStyleSheet("font-size:14px;font-weight:bold;color:#111827;");
        return l;
    };

    layout->addWidget(mkHdr("Label"));
    m_labelEdit = new QLineEdit(initial.label, this);
    m_labelEdit->setPlaceholderText("Checkbox text …");
    m_labelEdit->setStyleSheet("background:#ffffff; color:#111827;");
    layout->addWidget(m_labelEdit);

    layout->addWidget(mkHdr("Variable (True / False)"));
    m_varCombo = new QComboBox(this);
    m_varCombo->setStyleSheet("background:#ffffff; color:#111827;");
    if (pres) {
        for (const Variable& v : pres->variables.items) {
            if (v.type != Variable::Boolean) continue;
            if (!v.scopeSlideId.isEmpty() && v.scopeSlideId != currentSlideId) continue;
            m_varCombo->addItem(v.name, v.id);
        }
    }
    if (m_varCombo->count() == 0) {
        auto* hint = new QLabel(
            "No True/False variable yet. Create one first via the \"Variables\" toolbar button.", this);
        hint->setWordWrap(true);
        hint->setStyleSheet("color:#b91c1c;");
        layout->addWidget(hint);
    }
    int idx = m_varCombo->findData(initial.boundVariableId);
    if (idx >= 0) m_varCombo->setCurrentIndex(idx);
    layout->addWidget(m_varCombo);

    auto* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bbox->button(QDialogButtonBox::Ok)->setText(editing ? "Apply" : "Insert");
    bbox->button(QDialogButtonBox::Ok)->setEnabled(m_varCombo->count() > 0);
    layout->addWidget(bbox);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_labelEdit->setFocus();
    m_labelEdit->selectAll();
}

CheckboxConfig InsertCheckboxDialog::config() const {
    CheckboxConfig c;
    c.label = m_labelEdit->text().trimmed().isEmpty() ? "Option" : m_labelEdit->text().trimmed();
    c.boundVariableId = m_varCombo->currentData().toString();
    return c;
}
