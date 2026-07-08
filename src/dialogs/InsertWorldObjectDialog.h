#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;

class InsertWorldObjectDialog : public QDialog {
    Q_OBJECT
public:
    explicit InsertWorldObjectDialog(QWidget* parent = nullptr);
    QString modelPath() const;

private:
    void onBrowseClicked();

    QLineEdit* m_edit;
};
