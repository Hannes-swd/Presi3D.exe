#pragma once
#include <QDialog>
#include <QString>

class Presentation;
class QLineEdit;
class QComboBox;
class QStackedWidget;
class QDoubleSpinBox;
class QCheckBox;
class QWidget;

// What a Button does when clicked. Kept as a plain struct (not the dialog
// itself) so callers can pass/receive the whole configuration in one go
// instead of half a dozen loose parameters.
struct ButtonConfig {
    QString label;
    QString action = "navigate"; // "navigate" | "changeVariable"
    QString targetSlideId;

    QString boundVariableId;
    QString varOp       = "inc"; // "inc" | "dec" | "set" | "toggle"
    double  varOpNumber = 1.0;
    QString varOpText;
    bool    varOpBool   = true;
};

class InsertButtonDialog : public QDialog {
    Q_OBJECT
public:
    // currentSlideId: used to filter which variables are selectable (global + local-to-this-slide)
    explicit InsertButtonDialog(QWidget* parent, Presentation* pres, const QString& currentSlideId,
                                 const ButtonConfig& initial = {});

    ButtonConfig config() const;

private:
    void refreshOpOptionsForSelectedVariable();
    void refreshValuePageVisibility();

    Presentation* m_pres;

    QLineEdit*      m_labelEdit;
    QComboBox*      m_actionCombo;     // "Go to Slide" | "Change a Variable"
    QStackedWidget* m_actionStack;

    // "Go to Slide" page
    QComboBox* m_targetCombo;

    // "Change a Variable" page
    QComboBox*      m_varCombo;
    QComboBox*      m_opCombo;
    QWidget*        m_valueRow;
    QStackedWidget* m_valueStack;
    QDoubleSpinBox* m_numberValueEdit;
    QCheckBox*      m_boolValueEdit;
    QLineEdit*      m_textValueEdit;
};
