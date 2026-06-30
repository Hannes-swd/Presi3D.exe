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

signals:
    void slideModified();
    void elementModified();
    void presentationSettingsModified();

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
    void onElemAnimChanged(int);
    void onElemAnimDelayChanged(double);
    void onElemAnimDurationChanged(double);

private:
    void buildProjectGroup();
    void buildSlideGroup();
    void buildElementGroup();
    void buildTableGroup();
    void rebuildVisibilitySection();
    void refreshProject();
    void refreshSlide();
    void refreshElement();
    void refreshTable();
    void refreshTableCell();
    void updateColorButton(QPushButton*, const QColor&);

    Presentation* m_pres       = nullptr;
    int           m_slideIdx   = -1;
    int           m_elemIdx    = -1;
    int           m_cellRow    = -1;
    int           m_cellCol    = -1;
    bool          m_updating   = false;

    // Project settings
    QGroupBox*      m_projectGroup        = nullptr;
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

    // Animation controls
    QComboBox*      m_eAnimType      = nullptr;
    QLabel*         m_animDelayLabel = nullptr;
    QDoubleSpinBox* m_eAnimDelay     = nullptr;
    QLabel*         m_animDurLabel   = nullptr;
    QDoubleSpinBox* m_eAnimDuration  = nullptr;

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
};
