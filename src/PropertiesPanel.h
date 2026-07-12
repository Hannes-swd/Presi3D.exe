#pragma once
#include <QWidget>
#include <QVector>
#include "models/DataModel.h"

class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QGroupBox;
class QComboBox;
class QCheckBox;
class QVBoxLayout;
class QToolButton;

class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    void setSlide(Presentation* pres, int slideIndex);
    void setSelectedElement(int elemIndex);
    void setSelectedTableCell(int row, int col);
    void setSelectedWorldObject(int index);

signals:
    void slideModified();
    void elementModified();
    void presentationSettingsModified();
    void worldObjectModified();
    void worldObjectDeleteRequested();

private slots:
    void onTableBorderColorClicked();
    void onTableHeaderBgClicked();
    void onTableHeaderTextClicked();
    void onTableDefaultBgClicked();
    void onTableDefaultTextClicked();
    void onTableAddRow();
    void onTableDelRow();
    void onTableAddCol();
    void onTableDelCol();
    void onCellBgColorClicked();
    void onCellTextColorClicked();
    void onCellBoldChanged(bool);
    void onCellItalicChanged(bool);
    void onCellAlignChanged(int);

    void onTitleChanged(const QString&);
    void onSceneBgClicked();
    void onSlideSizeChanged();
    void onDefaultInactiveOpaChanged();
    void onSlideNameChanged(const QString&);
    void onSlideOwnSizeChanged();
    void onViewOffsetChanged();
    void onBgColorClicked();
    void onPosChanged();
    void onRotChanged();
    void onScaleChanged();

    void onElemColorClicked();
    void onElemBgColorClicked();
    void onElemPosChanged();
    void onElemSizeChanged();
    void onElemFontSizeChanged(int);
    void onElemContentChanged(const QString&);
    void onElemAlignChanged(int);
    void onElemBorderColorClicked();
    void onElemBorderChanged();
    void onElemCornerRadiusChanged();
    void onElemOpacityChanged(double);
    void onElemChangeIconClicked();

    void onWoPosChanged();
    void onWoRotChanged();
    void onWoScaleChanged();
    void onWoOpacityChanged();
    void onWoChangeModelClicked();
    void onWoDeleteClicked();

private:
    void buildProjectGroup();
    void buildSlideGroup();
    void buildElementGroup();
    void buildTableGroup();
    void buildChartGroup();
    void buildWorldObjGroup();
    void rebuildVisibilitySection();
    void refreshProject();
    void refreshSlide();
    void refreshElement();
    void refreshTable();
    void refreshTableCell();
    void refreshChart();
    void refreshWorldObj();
    void updateColorButton(QPushButton*, const QColor&);

    Presentation* m_pres       = nullptr;
    int           m_slideIdx   = -1;
    int           m_elemIdx    = -1;
    int           m_cellRow    = -1;
    int           m_cellCol    = -1;
    int           m_worldObjIdx = -1;
    bool          m_updating   = false;

    // Project settings
    QGroupBox*      m_projectGroup        = nullptr;
    QLineEdit*      m_titleEdit           = nullptr;
    QPushButton*    m_sceneBgBtn          = nullptr;
    QDoubleSpinBox* m_slideW              = nullptr;
    QDoubleSpinBox* m_slideH              = nullptr;
    QDoubleSpinBox* m_defaultInactiveOpa  = nullptr;

    // Slide properties
    QGroupBox*      m_slideGroup  = nullptr;
    QLineEdit*      m_slideName   = nullptr;
    QPushButton*    m_bgColorBtn  = nullptr;
    QDoubleSpinBox* m_posX        = nullptr;
    QDoubleSpinBox* m_posY        = nullptr;
    QDoubleSpinBox* m_posZ        = nullptr;
    QDoubleSpinBox* m_rotX        = nullptr;
    QDoubleSpinBox* m_rotY        = nullptr;
    QDoubleSpinBox* m_rotZ        = nullptr;
    QDoubleSpinBox* m_scale       = nullptr;
    QDoubleSpinBox* m_slideOwnW   = nullptr;
    QDoubleSpinBox* m_slideOwnH   = nullptr;
    QDoubleSpinBox* m_viewOffX    = nullptr;
    QDoubleSpinBox* m_viewOffY    = nullptr;

    // Per-slide visibility section (dynamic, rebuilt on every setSlide())
    QWidget*    m_visContainer = nullptr;
    QVBoxLayout* m_visLayout   = nullptr;

    struct VisRow {
        QString        slideId;
        QCheckBox*     check;
        QDoubleSpinBox* spin;
    };
    QVector<VisRow> m_visRows;

    // Element properties
    QGroupBox*      m_elemGroup   = nullptr;
    QLabel*         m_elemType    = nullptr;
    QLineEdit*      m_elemContent = nullptr;
    QDoubleSpinBox* m_eX          = nullptr;
    QDoubleSpinBox* m_eY          = nullptr;
    QDoubleSpinBox* m_eW          = nullptr;
    QDoubleSpinBox* m_eH          = nullptr;
    QPushButton*    m_eColorBtn   = nullptr;
    QPushButton*    m_eBgColorBtn = nullptr;
    QSpinBox*       m_eFontSize   = nullptr;
    QComboBox*      m_eAlign      = nullptr;

    // Shape-only section (hidden when a non-shape element is selected)
    QWidget*        m_elemFormSection = nullptr;
    QDoubleSpinBox* m_eBorderW        = nullptr;
    QPushButton*    m_eBorderColorBtn = nullptr;
    QDoubleSpinBox* m_eCornerRadius   = nullptr;

    // Icon-only section (hidden when a non-icon element is selected)
    QWidget*        m_elemIconSection = nullptr;
    QPushButton*    m_iconChangeBtn   = nullptr;

    // Generic opacity (also animatable via the Timeline panel's keyframes)
    QDoubleSpinBox* m_eOpacity       = nullptr;

    // Chart properties
    QGroupBox*      m_chartGroup        = nullptr;
    QLabel*         m_chartTypeLabel    = nullptr;
    QLabel*         m_chartTitleLabel   = nullptr;
    QPushButton*    m_chartEditBtn      = nullptr;

    // Table-wide properties
    QGroupBox*      m_tableGroup        = nullptr;
    QPushButton*    m_tBorderColorBtn   = nullptr;
    QDoubleSpinBox* m_tBorderWidth      = nullptr;
    QSpinBox*       m_tFontSize         = nullptr;
    QPushButton*    m_tDefaultBgBtn     = nullptr;
    QPushButton*    m_tDefaultTextBtn   = nullptr;
    QCheckBox*      m_tHasHeader        = nullptr;
    QPushButton*    m_tHeaderBgBtn      = nullptr;
    QPushButton*    m_tHeaderTextBtn    = nullptr;

    // Selected-cell overrides
    QWidget*        m_cellSection       = nullptr;
    QPushButton*    m_cellBgBtn         = nullptr;
    QPushButton*    m_cellTextBtn       = nullptr;
    QCheckBox*      m_cellBoldChk       = nullptr;
    QCheckBox*      m_cellItalicChk     = nullptr;
    QComboBox*      m_cellAlignCombo    = nullptr;

    // World object properties
    QGroupBox*      m_worldObjGroup     = nullptr;
    QLabel*         m_woModelLabel      = nullptr;
    QPushButton*    m_woChangeModelBtn  = nullptr;
    QDoubleSpinBox* m_woPosX            = nullptr;
    QDoubleSpinBox* m_woPosY            = nullptr;
    QDoubleSpinBox* m_woPosZ            = nullptr;
    QDoubleSpinBox* m_woRotX            = nullptr;
    QDoubleSpinBox* m_woRotY            = nullptr;
    QDoubleSpinBox* m_woRotZ            = nullptr;
    QDoubleSpinBox* m_woScale           = nullptr;
    QDoubleSpinBox* m_woOpacity         = nullptr;
    QPushButton*    m_woDeleteBtn       = nullptr;
};
