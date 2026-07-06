#pragma once
#include <QDialog>
#include <QString>

class Presentation;
class QComboBox;
class QDoubleSpinBox;

struct SliderConfig {
    QString boundVariableId; // must reference a Number variable
    double  min  = 0.0;
    double  max  = 100.0;
    double  step = 1.0;
};

// Slider binds to a Number variable: shows its current value as a filled
// track and lets the viewer drag it when presenting.
class InsertSliderDialog : public QDialog {
    Q_OBJECT
public:
    explicit InsertSliderDialog(QWidget* parent, Presentation* pres, const QString& currentSlideId,
                                 const SliderConfig& initial = {});
    SliderConfig config() const;

private:
    QComboBox*      m_varCombo;
    QDoubleSpinBox* m_minEdit;
    QDoubleSpinBox* m_maxEdit;
    QDoubleSpinBox* m_stepEdit;
};
