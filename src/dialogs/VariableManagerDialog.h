#pragma once
#include <QDialog>
#include <QString>
#include <QVector>
#include "models/VariableModel.h"

class Presentation;
class QTableWidget;

// Lets the user create/edit/delete variables without any coding knowledge:
// a name, a type (Text / Number / True-False) chosen from a dropdown, a
// value, and whether it applies to the whole presentation or just the
// current slide. Used as {name} in text fields elsewhere in the editor.
class VariableManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit VariableManagerDialog(QWidget* parent, Presentation* pres, const QString& currentSlideId);

private slots:
    void addRow();
    void deleteSelectedRow();
    void onAccept();

private:
    void loadFromPresentation();
    void addRowForVariable(const Variable& v);
    bool collectAndValidate(QVector<Variable>& out, QString& errorOut) const;

    Presentation* m_pres;
    QString       m_currentSlideId;
    QTableWidget* m_table;
};
