#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;

class InsertIFrameDialog : public QDialog {
    Q_OBJECT
public:
    explicit InsertIFrameDialog(QWidget* parent = nullptr, const QString& initialUrl = {});
    QString url() const;

private:
    QLineEdit* m_edit;
};
