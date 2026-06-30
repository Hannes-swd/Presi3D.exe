#pragma once
#include <QDialog>
#include <QString>

class InsertChartDialog : public QDialog {
    Q_OBJECT
public:
    explicit InsertChartDialog(QWidget* parent = nullptr);
    QString selectedType() const { return m_type; }

private:
    QString m_type;
};
