#pragma once
#include <QDialog>
#include "models/DataModel.h"

class QLineEdit;
class QLabel;
class QPushButton;
class QListWidget;
class QStackedWidget;
class QCheckBox;
class QComboBox;
class QFrame;

class ExportDialog : public QDialog {
    Q_OBJECT
public:
    ExportDialog(Presentation* pres, QWidget* parent = nullptr);

private slots:
    void browseFolder();
    void doExport();
    void updatePreview();

    void browseImageFolder();
    void doExportImages();

    void browsePdfFile();
    void doExportPdf();

    void browseHandoutFile();
    void doExportHandout();

private:
    QWidget* buildSavePage(const QString& initParent, const QString& initName);
    QWidget* buildImagesPage(const QString& initParent, const QString& initName);
    QWidget* buildPdfPage(const QString& initParent, const QString& initName);
    QWidget* buildHandoutPage(const QString& initParent, const QString& initName);
    QListWidget* buildSlideCheckList(); // populated from m_pres, one fresh instance per page

    static void         checkAllSlides(QListWidget* list, bool checked);
    static QVector<int> checkedSlideIndices(QListWidget* list);

    Presentation* m_pres = nullptr;

    // Left-hand navigation + right-hand page stack
    QListWidget*    m_navList = nullptr;
    QStackedWidget* m_stack   = nullptr;

    // Page: Export as HTML (this is also the presentation's Save location)
    QLineEdit*    m_nameEdit       = nullptr;
    QLineEdit*    m_parentEdit     = nullptr;
    QFrame*       m_saveLocFrame   = nullptr; // red border until a save location exists
    QLabel*       m_saveLocNotice  = nullptr; // "no save location yet" hint inside the frame
    QLabel*       m_previewLbl     = nullptr;
    QLabel*       m_status         = nullptr;
    QLabel*       m_statusIcon     = nullptr;
    QPushButton*  m_expBtn         = nullptr;
    QPushButton*  m_openBtn        = nullptr;

    // Page: Export as Images
    QListWidget*  m_slideList      = nullptr;
    QLineEdit*    m_imgFolderEdit  = nullptr;
    QPushButton*  m_imgExportBtn   = nullptr;
    QLabel*       m_imgStatus      = nullptr;

    // Page: Export as PDF
    QListWidget*  m_pdfSlideList   = nullptr;
    QLineEdit*    m_pdfFileEdit    = nullptr;
    QPushButton*  m_pdfExportBtn   = nullptr;
    QLabel*       m_pdfStatus      = nullptr;

    // Page: Export as Handout (Word)
    QListWidget*  m_handoutSlideList        = nullptr;
    QCheckBox*    m_handoutIncludeText      = nullptr;
    QCheckBox*    m_handoutIncludeShapeText = nullptr;
    QCheckBox*    m_handoutIncludeImages    = nullptr;
    QCheckBox*    m_handoutIncludeTables    = nullptr;
    QCheckBox*    m_handoutIncludeIcons     = nullptr;
    QComboBox*    m_handoutHeadingMode      = nullptr;
    QComboBox*    m_handoutOrderMode        = nullptr;
    QLineEdit*    m_handoutFileEdit         = nullptr;
    QPushButton*  m_handoutExportBtn        = nullptr;
    QLabel*       m_handoutStatus           = nullptr;
};
