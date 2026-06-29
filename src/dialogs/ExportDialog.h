#pragma once
#include <QDialog>
#include "models/DataModel.h"

class QLineEdit;
class QLabel;
class QPushButton;

class ExportDialog : public QDialog {
    Q_OBJECT
public:
    ExportDialog(Presentation* pres, QWidget* parent = nullptr);

private slots:
    void browseFolder();
    void doExport();
    void updatePreview();

private:
    Presentation* m_pres        = nullptr;
    QLineEdit*    m_nameEdit    = nullptr;
    QLineEdit*    m_parentEdit  = nullptr;
    QLabel*       m_previewLbl  = nullptr;
    QLabel*       m_status      = nullptr;
    QPushButton*  m_expBtn      = nullptr;
    QPushButton*  m_openBtn     = nullptr;
};
