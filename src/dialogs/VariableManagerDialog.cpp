#include "VariableManagerDialog.h"
#include "models/DataModel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QStackedWidget>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QLabel>
#include <QUuid>

namespace {
constexpr int ColName  = 0;
constexpr int ColType  = 1;
constexpr int ColValue = 2;
constexpr int ColScope = 3;

QComboBox* typeComboAt(QTableWidget* table, int row) {
    return qobject_cast<QComboBox*>(table->cellWidget(row, ColType));
}
QStackedWidget* valueStackAt(QTableWidget* table, int row) {
    return qobject_cast<QStackedWidget*>(table->cellWidget(row, ColValue));
}
QComboBox* scopeComboAt(QTableWidget* table, int row) {
    return qobject_cast<QComboBox*>(table->cellWidget(row, ColScope));
}

// row-in-table lookup for a cell widget, since row indices shift as rows are deleted
int rowOfWidget(QTableWidget* table, QWidget* w, int col) {
    for (int r = 0; r < table->rowCount(); ++r)
        if (table->cellWidget(r, col) == w) return r;
    return -1;
}
} // namespace

VariableManagerDialog::VariableManagerDialog(QWidget* parent, Presentation* pres, const QString& currentSlideId)
    : QDialog(parent), m_pres(pres), m_currentSlideId(currentSlideId) {
    setWindowTitle("Variables");
    resize(720, 420);

    auto* layout = new QVBoxLayout(this);

    auto* hint = new QLabel(
        "Create a variable, give it a name, then use it anywhere you can type text, e.g. {price} or {price + 10}.",
        this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#4b5563;");
    layout->addWidget(hint);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({"Name", "Type", "Value", "Applies to"});
    m_table->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColType, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColValue, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColScope, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_table, 1);

    auto* rowBtns = new QHBoxLayout();
    auto* addBtn = new QPushButton("Add Variable", this);
    auto* delBtn = new QPushButton("Delete Selected", this);
    connect(addBtn, &QPushButton::clicked, this, &VariableManagerDialog::addRow);
    connect(delBtn, &QPushButton::clicked, this, &VariableManagerDialog::deleteSelectedRow);
    rowBtns->addWidget(addBtn);
    rowBtns->addWidget(delBtn);
    rowBtns->addStretch();
    layout->addLayout(rowBtns);

    auto* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(bbox);
    connect(bbox, &QDialogButtonBox::accepted, this, &VariableManagerDialog::onAccept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadFromPresentation();
    if (m_table->rowCount() == 0) addRow();
}

void VariableManagerDialog::loadFromPresentation() {
    if (!m_pres) return;
    for (const Variable& v : m_pres->variables.items)
        addRowForVariable(v);
}

void VariableManagerDialog::addRow() {
    Variable v;
    v.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    v.scopeSlideId = m_currentSlideId; // new variables default to "this slide" scope
    addRowForVariable(v);
}

void VariableManagerDialog::addRowForVariable(const Variable& v) {
    int row = m_table->rowCount();
    m_table->insertRow(row);

    auto* nameItem = new QTableWidgetItem(v.name);
    nameItem->setData(Qt::UserRole, v.id);
    m_table->setItem(row, ColName, nameItem);

    auto* typeCombo = new QComboBox(m_table);
    typeCombo->addItem("Text",         int(Variable::Text));
    typeCombo->addItem("Number",       int(Variable::Number));
    typeCombo->addItem("True / False", int(Variable::Boolean));
    typeCombo->setCurrentIndex(typeCombo->findData(int(v.type)));
    m_table->setCellWidget(row, ColType, typeCombo);

    auto* valueStack = new QStackedWidget(m_table);
    auto* textEdit = new QLineEdit(v.textValue, valueStack);
    auto* numberEdit = new QDoubleSpinBox(valueStack);
    numberEdit->setRange(-1'000'000'000.0, 1'000'000'000.0);
    numberEdit->setDecimals(4);
    numberEdit->setValue(v.numberValue);
    auto* boolEdit = new QCheckBox("True", valueStack);
    boolEdit->setChecked(v.boolValue);
    valueStack->addWidget(textEdit);   // index 0 == Variable::Text
    valueStack->addWidget(numberEdit); // index 1 == Variable::Number
    valueStack->addWidget(boolEdit);   // index 2 == Variable::Boolean
    valueStack->setCurrentIndex(int(v.type));
    m_table->setCellWidget(row, ColValue, valueStack);

    connect(typeCombo, &QComboBox::currentIndexChanged, this, [this, typeCombo](int) {
        int r = rowOfWidget(m_table, typeCombo, ColType);
        if (r < 0) return;
        if (auto* stack = valueStackAt(m_table, r))
            stack->setCurrentIndex(typeCombo->currentData().toInt());
    });

    auto* scopeCombo = new QComboBox(m_table);
    scopeCombo->addItem("Whole Presentation", QString());
    if (m_pres) {
        for (const Slide& s : m_pres->slides) {
            QString label = "Only on: " + (s.name.isEmpty() ? QString("Slide %1").arg(scopeCombo->count()) : s.name);
            scopeCombo->addItem(label, s.id);
        }
    }
    int scopeIdx = scopeCombo->findData(v.scopeSlideId);
    scopeCombo->setCurrentIndex(scopeIdx >= 0 ? scopeIdx : 0);
    m_table->setCellWidget(row, ColScope, scopeCombo);
}

void VariableManagerDialog::deleteSelectedRow() {
    int row = m_table->currentRow();
    if (row >= 0) m_table->removeRow(row);
}

bool VariableManagerDialog::collectAndValidate(QVector<Variable>& out, QString& errorOut) const {
    out.clear();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        Variable v;
        v.id = m_table->item(row, ColName)->data(Qt::UserRole).toString();
        v.name = m_table->item(row, ColName)->text().trimmed();

        if (v.name.isEmpty()) {
            errorOut = QString("Row %1: please enter a name.").arg(row + 1);
            return false;
        }
        bool validChars = v.name.at(0).isLetter() || v.name.at(0) == '_';
        for (const QChar& c : v.name)
            validChars = validChars && (c.isLetterOrNumber() || c == '_');
        if (!validChars) {
            errorOut = QString("Row %1 (\"%2\"): only letters, numbers and _ are allowed, "
                                "and the name must start with a letter.").arg(row + 1).arg(v.name);
            return false;
        }

        auto* typeCombo = typeComboAt(m_table, row);
        v.type = Variable::Type(typeCombo->currentData().toInt());

        auto* stack = valueStackAt(m_table, row);
        switch (v.type) {
        case Variable::Text:
            v.textValue = qobject_cast<QLineEdit*>(stack->widget(0))->text();
            break;
        case Variable::Number:
            v.numberValue = qobject_cast<QDoubleSpinBox*>(stack->widget(1))->value();
            break;
        case Variable::Boolean:
            v.boolValue = qobject_cast<QCheckBox*>(stack->widget(2))->isChecked();
            break;
        }

        auto* scopeCombo = scopeComboAt(m_table, row);
        v.scopeSlideId = scopeCombo->currentData().toString();

        for (const Variable& other : out) {
            if (other.scopeSlideId == v.scopeSlideId && other.name.compare(v.name, Qt::CaseInsensitive) == 0) {
                errorOut = QString("The name \"%1\" is used more than once in the same scope.").arg(v.name);
                return false;
            }
        }
        out << v;
    }
    return true;
}

void VariableManagerDialog::onAccept() {
    QVector<Variable> result;
    QString error;
    if (!collectAndValidate(result, error)) {
        QMessageBox::warning(this, "Invalid Variables", error);
        return;
    }
    if (m_pres) m_pres->variables.items = result;
    accept();
}
