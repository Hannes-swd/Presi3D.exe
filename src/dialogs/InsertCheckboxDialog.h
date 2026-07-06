#pragma once
#include <QDialog>
#include <QString>

class Presentation;
class QLineEdit;
class QComboBox;

struct CheckboxConfig {
    QString label = "Option";
    QString boundVariableId; // must reference a Boolean variable
};

// Checkbox binds to a True/False variable: shows its current state and lets
// the viewer toggle it when presenting.
class InsertCheckboxDialog : public QDialog {
    Q_OBJECT
public:
    explicit InsertCheckboxDialog(QWidget* parent, Presentation* pres, const QString& currentSlideId,
                                   const CheckboxConfig& initial = {});
    CheckboxConfig config() const;

private:
    QLineEdit* m_labelEdit;
    QComboBox* m_varCombo;
};
