#include "InsertFormulaDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
#include <QPushButton>

InsertFormulaDialog::InsertFormulaDialog(QWidget* parent, const QString& initialLatex)
    : QDialog(parent) {
    setWindowTitle(initialLatex.isEmpty() ? "Formel einfügen" : "Formel bearbeiten");
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* hdr = new QLabel("LaTeX-Formel", this);
    hdr->setStyleSheet("font-size:14px;font-weight:bold;color:#111827;");
    layout->addWidget(hdr);

    auto* hint = new QLabel("z. B.  E = mc^2   oder   x = (-b \\pm \\sqrt{b^2-4ac})/2a", this);
    hint->setStyleSheet("color:#6b7280; font-size:11px;");
    layout->addWidget(hint);

    m_edit = new QPlainTextEdit(this);
    m_edit->setPlainText(initialLatex);
    m_edit->setPlaceholderText("E = mc^2");
    m_edit->setFixedHeight(90);
    m_edit->setStyleSheet("background:#ffffff; color:#111827;");
    layout->addWidget(m_edit);

    auto* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bbox->button(QDialogButtonBox::Ok)->setText(initialLatex.isEmpty() ? "Einfügen" : "Übernehmen");
    layout->addWidget(bbox);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_edit->setFocus();
}

QString InsertFormulaDialog::latex() const {
    return m_edit->toPlainText().trimmed();
}
