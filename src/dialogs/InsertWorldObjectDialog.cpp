#include "InsertWorldObjectDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>

InsertWorldObjectDialog::InsertWorldObjectDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Insert World Object");
    setMinimumWidth(440);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* hdr = new QLabel("3D Model (.gltf / .glb)", this);
    hdr->setStyleSheet("font-size:14px;font-weight:bold;color:#111827;");
    layout->addWidget(hdr);

    auto* hint = new QLabel(
        "World objects are free-floating 3D models placed in space between slides — "
        "visible only in 3D Mode, never part of the slide sequence.", this);
    hint->setStyleSheet("color:#6b7280; font-size:11px;");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* row = new QHBoxLayout();
    m_edit = new QLineEdit(this);
    m_edit->setReadOnly(true);
    m_edit->setPlaceholderText("No file selected…");
    m_edit->setStyleSheet("background:#ffffff; color:#111827;");
    row->addWidget(m_edit, 1);
    auto* browseBtn = new QPushButton("Browse…", this);
    connect(browseBtn, &QPushButton::clicked, this, &InsertWorldObjectDialog::onBrowseClicked);
    row->addWidget(browseBtn);
    layout->addLayout(row);

    auto* warn = new QLabel(
        "Downloaded models (e.g. from Sketchfab) may carry their own license terms — "
        "check before using one in a presentation you distribute.", this);
    warn->setStyleSheet("color:#b45309; font-size:11px;");
    warn->setWordWrap(true);
    layout->addWidget(warn);

    auto* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    bbox->button(QDialogButtonBox::Ok)->setText("Insert");
    bbox->button(QDialogButtonBox::Ok)->setEnabled(false);
    layout->addWidget(bbox);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_edit, &QLineEdit::textChanged, this, [bbox](const QString& t) {
        bbox->button(QDialogButtonBox::Ok)->setEnabled(!t.isEmpty());
    });
}

void InsertWorldObjectDialog::onBrowseClicked() {
    QString path = QFileDialog::getOpenFileName(
        this, "Select 3D Model", {}, "glTF Models (*.gltf *.glb)");
    if (!path.isEmpty())
        m_edit->setText(path);
}

QString InsertWorldObjectDialog::modelPath() const {
    return m_edit->text();
}
