#include "InsertButtonDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>

InsertButtonDialog::InsertButtonDialog(QWidget* parent, const QVector<QPair<QString, QString>>& slides,
                                       const QString& initialLabel, const QString& initialTargetId)
    : QDialog(parent) {
    bool editing = !initialTargetId.isEmpty() || !initialLabel.isEmpty();
    setWindowTitle(editing ? "Edit Button" : "Insert Button");
    setMinimumWidth(380);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* lblHdr = new QLabel("Label", this);
    lblHdr->setStyleSheet("font-size:14px;font-weight:bold;color:#111827;");
    layout->addWidget(lblHdr);

    m_labelEdit = new QLineEdit(this);
    m_labelEdit->setText(initialLabel.isEmpty() ? "Next" : initialLabel);
    m_labelEdit->setPlaceholderText("Button text …");
    m_labelEdit->setStyleSheet("background:#ffffff; color:#111827;");
    layout->addWidget(m_labelEdit);

    auto* targetHdr = new QLabel("Jumps to Slide", this);
    targetHdr->setStyleSheet("font-size:14px;font-weight:bold;color:#111827; margin-top:6px;");
    layout->addWidget(targetHdr);

    m_targetCombo = new QComboBox(this);
    m_targetCombo->setStyleSheet("background:#ffffff; color:#111827;");
    for (const auto& [id, name] : slides)
        m_targetCombo->addItem(name, id);
    int idx = m_targetCombo->findData(initialTargetId);
    if (idx >= 0) m_targetCombo->setCurrentIndex(idx);
    layout->addWidget(m_targetCombo);

    auto* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bbox->button(QDialogButtonBox::Ok)->setText(editing ? "Apply" : "Insert");
    layout->addWidget(bbox);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_labelEdit->setFocus();
    m_labelEdit->selectAll();
}

QString InsertButtonDialog::label() const {
    return m_labelEdit->text().trimmed();
}

QString InsertButtonDialog::targetSlideId() const {
    return m_targetCombo->currentData().toString();
}
