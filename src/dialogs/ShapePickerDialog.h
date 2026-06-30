#pragma once
#include <QDialog>
#include "ShapeUtils.h"

class ShapePickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit ShapePickerDialog(QWidget* parent = nullptr);
    QString selectedShape() const { return m_selected; }

private:
    QString m_selected;
};
