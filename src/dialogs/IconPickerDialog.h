#pragma once
#include <QDialog>
#include "IconUtils.h"

class QLineEdit;

class IconPickerDialog : public QDialog {
    Q_OBJECT
public:
    explicit IconPickerDialog(QWidget* parent = nullptr);
    QString selectedIcon() const { return m_selected; }

private:
    void rebuildGrid(const QString& filter);

    QString    m_selected;
    QWidget*   m_gridContainer = nullptr;
    QLineEdit* m_filterEdit    = nullptr;
};
