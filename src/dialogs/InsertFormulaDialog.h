#pragma once
#include <QDialog>
#include <QString>

class QPlainTextEdit;

class InsertFormulaDialog : public QDialog {
    Q_OBJECT
public:
    explicit InsertFormulaDialog(QWidget* parent = nullptr, const QString& initialLatex = {});
    QString latex() const;

private:
    QPlainTextEdit* m_edit;
};
