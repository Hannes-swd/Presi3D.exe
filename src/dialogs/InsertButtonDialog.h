#pragma once
#include <QDialog>
#include <QString>
#include <QVector>
#include <QPair>

class QLineEdit;
class QComboBox;

class InsertButtonDialog : public QDialog {
    Q_OBJECT
public:
    // slides: list of (slideId, slideName) in presentation order
    explicit InsertButtonDialog(QWidget* parent, const QVector<QPair<QString, QString>>& slides,
                                 const QString& initialLabel = {}, const QString& initialTargetId = {});

    QString label() const;
    QString targetSlideId() const;

private:
    QLineEdit* m_labelEdit;
    QComboBox* m_targetCombo;
};
