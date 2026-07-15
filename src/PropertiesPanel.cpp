#include "PropertiesPanel.h"
#include "rendering/ChartRenderer.h"
#include "dialogs/ChartEditorDialog.h"
#include "dialogs/IconPickerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QColorDialog>
#include <QToolButton>
#include <QInputDialog>
#include <QFileDialog>
#include <QFileInfo>

// ── Helpers ───────────────────────────────────────────────────────────────────

static QDoubleSpinBox* makeSpin(double min, double max, double step = 1.0) {
    auto* s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setSingleStep(step);
    s->setDecimals(1);
    return s;
}

struct SectionWidgets {
    QWidget* outer;
    QWidget* content;
};

static SectionWidgets makeSection(const QString& title, bool open, QWidget* parent) {
    auto* outer = new QWidget(parent);
    auto* outerVBox = new QVBoxLayout(outer);
    outerVBox->setContentsMargins(0, 2, 0, 0);
    outerVBox->setSpacing(0);

    auto* btn = new QToolButton(outer);
    btn->setText(title);
    btn->setCheckable(true);
    btn->setChecked(open);
    btn->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setStyleSheet(
        "QToolButton {"
        "  text-align: left; font-size: 10px; font-weight: bold;"
        "  color: #374151; background: #f3f4f6;"
        "  border: none; border-radius: 3px; padding: 3px 6px;"
        "}"
        "QToolButton:hover { background: #e5e7eb; }"
    );

    auto* content = new QWidget(outer);
    content->setVisible(open);

    outerVBox->addWidget(btn);
    outerVBox->addWidget(content);

    QObject::connect(btn, &QToolButton::toggled, [content, btn](bool on) {
        content->setVisible(on);
        btn->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
    });

    return { outer, content };
}

static QFormLayout* makeForm(QWidget* parent) {
    auto* form = new QFormLayout(parent);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setContentsMargins(4, 2, 4, 4);
    form->setSpacing(4);
    return form;
}

// Forward declaration (defined below alongside other element slots)
SlideElement* getElem(Presentation*, int, int);

// ── Constructor ───────────────────────────────────────────────────────────────

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent) {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* inner = new QWidget;
    auto* vbox  = new QVBoxLayout(inner);
    vbox->setContentsMargins(6, 6, 6, 6);
    vbox->setSpacing(8);

    auto* title = new QLabel("PROPERTIES", inner);
    title->setStyleSheet("font-weight: bold; font-size: 10px; color: #374151; padding: 8px 8px 4px 8px; letter-spacing: 1px;");
    vbox->addWidget(title);

    buildProjectGroup();
    buildSlideGroup();
    buildElementGroup();
    buildTableGroup();
    buildChartGroup();
    buildWorldObjGroup();
    vbox->addWidget(m_projectGroup);
    vbox->addWidget(m_slideGroup);
    vbox->addWidget(m_elemGroup);
    vbox->addWidget(m_tableGroup);
    vbox->addWidget(m_chartGroup);
    vbox->addWidget(m_worldObjGroup);
    vbox->addStretch();

    scroll->setWidget(inner);
    outerLayout->addWidget(scroll);
}

void PropertiesPanel::buildProjectGroup() {
    m_projectGroup = new QGroupBox("Presentation", this);
    auto* gvbox = new QVBoxLayout(m_projectGroup);
    gvbox->setContentsMargins(4, 8, 4, 4);
    gvbox->setSpacing(2);

    // ── Grundeinstellungen (offen) ─────────────────────────────────────────────
    auto sec1 = makeSection("Basic Settings", true, m_projectGroup);
    auto* form1 = makeForm(sec1.content);

    m_titleEdit = new QLineEdit(sec1.content);
    m_titleEdit->setToolTip("Title shown in the browser tab of the exported presentation.\n"
                             "Supports {name} variables (e.g. {year}, {today}, or your own global variables).");
    form1->addRow("Title:", m_titleEdit);

    m_sceneBgBtn = new QPushButton(sec1.content);
    m_sceneBgBtn->setFixedHeight(24);
    m_sceneBgBtn->setToolTip("Background color behind all slides");
    form1->addRow("Background:", m_sceneBgBtn);

    auto* sizeRow = new QHBoxLayout;
    m_slideW = makeSpin(100, 9999, 10);  m_slideW->setValue(1920);
    m_slideH = makeSpin(100, 9999, 10);  m_slideH->setValue(1080);
    sizeRow->addWidget(m_slideW);
    sizeRow->addWidget(new QLabel("×"));
    sizeRow->addWidget(m_slideH);
    form1->addRow("Slide Size:", sizeRow);

    gvbox->addWidget(sec1.outer);

    // ── Standard Sichtbarkeit (eingeklappt) ────────────────────────────────────
    auto sec2 = makeSection("Default Visibility", false, m_projectGroup);
    auto* form2 = makeForm(sec2.content);

    m_defaultInactiveOpa = makeSpin(0.0, 1.0, 0.05);
    m_defaultInactiveOpa->setDecimals(2);
    m_defaultInactiveOpa->setValue(0.3);
    m_defaultInactiveOpa->setToolTip("Default opacity for slides without their own setting\n"
                                     "(applies when no per-slide override is set)");
    form2->addRow("Inactive Default:", m_defaultInactiveOpa);

    gvbox->addWidget(sec2.outer);

    connect(m_titleEdit, &QLineEdit::textEdited, this, &PropertiesPanel::onTitleChanged);
    connect(m_sceneBgBtn, &QPushButton::clicked, this, &PropertiesPanel::onSceneBgClicked);
    connect(m_slideW, &QDoubleSpinBox::valueChanged, this, [this](double) { onSlideSizeChanged(); });
    connect(m_slideH, &QDoubleSpinBox::valueChanged, this, [this](double) { onSlideSizeChanged(); });
    connect(m_defaultInactiveOpa, &QDoubleSpinBox::valueChanged,
            this, [this](double) { onDefaultInactiveOpaChanged(); });
}

void PropertiesPanel::buildSlideGroup() {
    m_slideGroup = new QGroupBox("Slide", this);
    auto* gvbox = new QVBoxLayout(m_slideGroup);
    gvbox->setContentsMargins(4, 8, 4, 4);
    gvbox->setSpacing(2);

    // ── Grundeinstellungen (offen) ─────────────────────────────────────────────
    auto secBase = makeSection("Basic Settings", true, m_slideGroup);
    auto* formBase = makeForm(secBase.content);

    m_slideName = new QLineEdit(secBase.content);
    formBase->addRow("Name:", m_slideName);

    m_bgColorBtn = new QPushButton(secBase.content);
    m_bgColorBtn->setFixedHeight(24);
    formBase->addRow("Background:", m_bgColorBtn);

    gvbox->addWidget(secBase.outer);

    // ── 3D Position (offen) ────────────────────────────────────────────────────
    auto secPos = makeSection("3D Position", true, m_slideGroup);
    auto* formPos = makeForm(secPos.content);

    m_posX = makeSpin(-99999, 99999, 100);
    m_posY = makeSpin(-99999, 99999, 100);
    m_posZ = makeSpin(-99999, 99999, 100);
    formPos->addRow("X:", m_posX);
    formPos->addRow("Y:", m_posY);
    formPos->addRow("Z:", m_posZ);

    gvbox->addWidget(secPos.outer);

    // ── Rotation & Zoom (offen) ────────────────────────────────────────────────
    auto secRot = makeSection("Rotation & Zoom", true, m_slideGroup);
    auto* formRot = makeForm(secRot.content);

    m_rotX = makeSpin(-360, 360);
    m_rotY = makeSpin(-360, 360);
    m_rotZ = makeSpin(-360, 360);
    formRot->addRow("Rot X:", m_rotX);
    formRot->addRow("Rot Y:", m_rotY);
    formRot->addRow("Rot Z:", m_rotZ);

    m_scale = makeSpin(0.01, 10.0, 0.1);
    m_scale->setToolTip("Camera zoom on this slide:\n"
                        "1.0 = slide fills the view\n"
                        "< 1 = zoom in (less context)\n"
                        "> 1 = zoom out (more context)");
    formRot->addRow("Zoom:", m_scale);

    gvbox->addWidget(secRot.outer);

    // ── Sichtfeld (eingeklappt) ────────────────────────────────────────────────
    auto secView = makeSection("Field of View (3D only)", false, m_slideGroup);
    auto* formView = makeForm(secView.content);

    auto* viewOffRow = new QHBoxLayout;
    m_viewOffX = makeSpin(-9999, 9999, 50);
    m_viewOffX->setToolTip("Field of view center X offset\n(0 = slide centered)");
    m_viewOffY = makeSpin(-9999, 9999, 50);
    m_viewOffY->setToolTip("Field of view center Y offset\n(0 = slide centered)");
    viewOffRow->addWidget(m_viewOffX);
    viewOffRow->addWidget(new QLabel("Y:"));
    viewOffRow->addWidget(m_viewOffY);
    formView->addRow("Offset X:", viewOffRow);

    gvbox->addWidget(secView.outer);

    // ── Eigene Foliengröße (eingeklappt) ──────────────────────────────────────
    auto secSize = makeSection("Custom Slide Size", false, m_slideGroup);
    auto* formSize = makeForm(secSize.content);

    auto* ownSizeRow = new QHBoxLayout;
    m_slideOwnW = makeSpin(0, 9999, 10);
    m_slideOwnW->setDecimals(0);
    m_slideOwnW->setToolTip("Width of this slide (0 = use presentation default)");
    m_slideOwnH = makeSpin(0, 9999, 10);
    m_slideOwnH->setDecimals(0);
    m_slideOwnH->setToolTip("Height of this slide (0 = use presentation default)");
    ownSizeRow->addWidget(m_slideOwnW);
    ownSizeRow->addWidget(new QLabel("×"));
    ownSizeRow->addWidget(m_slideOwnH);
    formSize->addRow("Size:", ownSizeRow);

    auto* sizeHint = new QLabel("(0 = default)", secSize.content);
    sizeHint->setStyleSheet("color: #6b7280; font-size: 9px;");
    formSize->addRow(sizeHint);

    gvbox->addWidget(secSize.outer);

    // ── Sichtbarkeit anderer Folien (eingeklappt) ──────────────────────────────
    auto secVis = makeSection("Visibility of Other Slides", false, m_slideGroup);
    auto* visVBox = new QVBoxLayout(secVis.content);
    visVBox->setContentsMargins(4, 2, 4, 4);
    visVBox->setSpacing(2);

    auto* visHint = new QLabel("When this slide is active:", secVis.content);
    visHint->setStyleSheet("color: #6b7280; font-size: 9px;");
    visVBox->addWidget(visHint);

    m_visContainer = new QWidget(secVis.content);
    m_visLayout    = new QVBoxLayout(m_visContainer);
    m_visLayout->setContentsMargins(0, 2, 0, 2);
    m_visLayout->setSpacing(2);
    visVBox->addWidget(m_visContainer);

    gvbox->addWidget(secVis.outer);

    // Signals
    connect(m_slideName, &QLineEdit::editingFinished, this, [this]() {
        onSlideNameChanged(m_slideName->text());
    });
    connect(m_bgColorBtn, &QPushButton::clicked, this, &PropertiesPanel::onBgColorClicked);
    connect(m_posX, &QDoubleSpinBox::valueChanged, this, [this](double) { onPosChanged(); });
    connect(m_posY, &QDoubleSpinBox::valueChanged, this, [this](double) { onPosChanged(); });
    connect(m_posZ, &QDoubleSpinBox::valueChanged, this, [this](double) { onPosChanged(); });
    connect(m_rotX, &QDoubleSpinBox::valueChanged, this, [this](double) { onRotChanged(); });
    connect(m_rotY, &QDoubleSpinBox::valueChanged, this, [this](double) { onRotChanged(); });
    connect(m_rotZ, &QDoubleSpinBox::valueChanged, this, [this](double) { onRotChanged(); });
    connect(m_scale,     &QDoubleSpinBox::valueChanged, this, [this](double) { onScaleChanged(); });
    connect(m_slideOwnW, &QDoubleSpinBox::valueChanged, this, [this](double) { onSlideOwnSizeChanged(); });
    connect(m_slideOwnH, &QDoubleSpinBox::valueChanged, this, [this](double) { onSlideOwnSizeChanged(); });
    connect(m_viewOffX,  &QDoubleSpinBox::valueChanged, this, [this](double) { onViewOffsetChanged(); });
    connect(m_viewOffY,  &QDoubleSpinBox::valueChanged, this, [this](double) { onViewOffsetChanged(); });
}

void PropertiesPanel::buildElementGroup() {
    m_elemGroup = new QGroupBox("Element", this);
    auto* gvbox = new QVBoxLayout(m_elemGroup);
    gvbox->setContentsMargins(4, 8, 4, 4);
    gvbox->setSpacing(2);

    // ── Grundeinstellungen (offen) ─────────────────────────────────────────────
    auto secBase = makeSection("Basic Settings", true, m_elemGroup);
    auto* formBase = makeForm(secBase.content);

    m_elemType = new QLabel("—", secBase.content);
    m_elemType->setStyleSheet("color: #374151;");
    formBase->addRow("Type:", m_elemType);

    m_elemContent = new QLineEdit(secBase.content);
    formBase->addRow("Content:", m_elemContent);

    m_eX = makeSpin(-9999, 9999); m_eY = makeSpin(-9999, 9999);
    m_eW = makeSpin(1, 9999);     m_eH = makeSpin(1, 9999);
    formBase->addRow("X:", m_eX);
    formBase->addRow("Y:", m_eY);
    formBase->addRow("Width:", m_eW);
    formBase->addRow("Height:",   m_eH);

    gvbox->addWidget(secBase.outer);

    // ── Darstellung (offen) ────────────────────────────────────────────────────
    auto secStyle = makeSection("Appearance", true, m_elemGroup);
    auto* formStyle = makeForm(secStyle.content);

    m_eColorBtn   = new QPushButton(secStyle.content); m_eColorBtn->setFixedHeight(24);
    m_eBgColorBtn = new QPushButton(secStyle.content); m_eBgColorBtn->setFixedHeight(24);
    formStyle->addRow("Color:",       m_eColorBtn);
    formStyle->addRow("Background:", m_eBgColorBtn);

    m_eFontSize = new QSpinBox(secStyle.content);
    m_eFontSize->setRange(6, 200);
    m_eFontSize->setValue(32);
    formStyle->addRow("Font Size:", m_eFontSize);

    m_eAlign = new QComboBox(secStyle.content);
    m_eAlign->addItems({"Left", "Center", "Right"});
    formStyle->addRow("Alignment:", m_eAlign);

    m_eOpacity = makeSpin(0.0, 1.0, 0.05);
    m_eOpacity->setDecimals(2);
    m_eOpacity->setValue(1.0);
    m_eOpacity->setToolTip("Base opacity; timeline keyframes (Timeline panel) can animate this per slide-step");
    formStyle->addRow("Opacity:", m_eOpacity);

    gvbox->addWidget(secStyle.outer);

    // ── Form – nur für Shapes (offen, aber initial ausgeblendet) ──────────────
    auto secForm = makeSection("Shape", true, m_elemGroup);
    auto* formForm = makeForm(secForm.content);

    m_eBorderW = makeSpin(0, 200, 1);
    m_eBorderW->setDecimals(0);
    formForm->addRow("Border Width px:", m_eBorderW);

    m_eBorderColorBtn = new QPushButton(secForm.content);
    m_eBorderColorBtn->setFixedHeight(24);
    formForm->addRow("Border Color:", m_eBorderColorBtn);

    m_eCornerRadius = makeSpin(0, 999, 5);
    m_eCornerRadius->setDecimals(0);
    m_eCornerRadius->setToolTip("Corner rounding in slide pixels (0 = no rounding)");
    formForm->addRow("Corner Radius:", m_eCornerRadius);

    m_elemFormSection = secForm.outer;
    gvbox->addWidget(secForm.outer);

    // ── Icon – nur für Icons (offen, aber initial ausgeblendet) ────────────────
    auto secIcon = makeSection("Icon", true, m_elemGroup);
    auto* formIcon = makeForm(secIcon.content);

    m_iconChangeBtn = new QPushButton("Change Icon\xE2\x80\xA6", secIcon.content);
    formIcon->addRow(m_iconChangeBtn);

    m_elemIconSection = secIcon.outer;
    gvbox->addWidget(secIcon.outer);

    // Signals
    connect(m_elemContent, &QLineEdit::editingFinished, this, [this]() {
        onElemContentChanged(m_elemContent->text());
    });
    connect(m_eColorBtn,   &QPushButton::clicked, this, &PropertiesPanel::onElemColorClicked);
    connect(m_eBgColorBtn, &QPushButton::clicked, this, &PropertiesPanel::onElemBgColorClicked);
    connect(m_eX, &QDoubleSpinBox::valueChanged, this, [this](double) { onElemPosChanged(); });
    connect(m_eY, &QDoubleSpinBox::valueChanged, this, [this](double) { onElemPosChanged(); });
    connect(m_eW, &QDoubleSpinBox::valueChanged, this, [this](double) { onElemSizeChanged(); });
    connect(m_eH, &QDoubleSpinBox::valueChanged, this, [this](double) { onElemSizeChanged(); });
    connect(m_eFontSize, &QSpinBox::valueChanged, this, &PropertiesPanel::onElemFontSizeChanged);
    connect(m_eAlign, &QComboBox::currentIndexChanged, this, &PropertiesPanel::onElemAlignChanged);
    connect(m_eBorderW,        &QDoubleSpinBox::valueChanged, this, [this](double) { onElemBorderChanged(); });
    connect(m_eBorderColorBtn, &QPushButton::clicked,         this, &PropertiesPanel::onElemBorderColorClicked);
    connect(m_eCornerRadius,   &QDoubleSpinBox::valueChanged, this, [this](double) { onElemCornerRadiusChanged(); });
    connect(m_eOpacity, &QDoubleSpinBox::valueChanged, this, &PropertiesPanel::onElemOpacityChanged);
    connect(m_iconChangeBtn, &QPushButton::clicked, this, &PropertiesPanel::onElemChangeIconClicked);

    m_elemFormSection->setVisible(false);
    m_elemIconSection->setVisible(false);
    m_elemGroup->setEnabled(false);
}

void PropertiesPanel::buildTableGroup() {
    m_tableGroup = new QGroupBox("Table", this);
    auto* gvbox = new QVBoxLayout(m_tableGroup);
    gvbox->setContentsMargins(4, 8, 4, 4);
    gvbox->setSpacing(2);

    // ── Tabellendesign ────────────────────────────────────────────────────────
    auto secDesign = makeSection("Table Design", true, m_tableGroup);
    auto* formD    = makeForm(secDesign.content);

    m_tBorderColorBtn = new QPushButton(secDesign.content);
    m_tBorderColorBtn->setFixedHeight(24);
    formD->addRow("Border Color:", m_tBorderColorBtn);

    m_tBorderWidth = makeSpin(0, 20, 0.5);
    m_tBorderWidth->setDecimals(1);
    formD->addRow("Border Width:", m_tBorderWidth);

    m_tFontSize = new QSpinBox(secDesign.content);
    m_tFontSize->setRange(6, 120);
    m_tFontSize->setValue(20);
    formD->addRow("Font Size:", m_tFontSize);

    m_tDefaultBgBtn = new QPushButton(secDesign.content);
    m_tDefaultBgBtn->setFixedHeight(24);
    formD->addRow("Cell Background:", m_tDefaultBgBtn);

    m_tDefaultTextBtn = new QPushButton(secDesign.content);
    m_tDefaultTextBtn->setFixedHeight(24);
    formD->addRow("Cell Text Color:", m_tDefaultTextBtn);

    // Header row
    m_tHasHeader = new QCheckBox("Header Row", secDesign.content);
    formD->addRow(m_tHasHeader);

    m_tHeaderBgBtn = new QPushButton(secDesign.content);
    m_tHeaderBgBtn->setFixedHeight(24);
    formD->addRow("Header Background:", m_tHeaderBgBtn);

    m_tHeaderTextBtn = new QPushButton(secDesign.content);
    m_tHeaderTextBtn->setFixedHeight(24);
    formD->addRow("Header Text Color:", m_tHeaderTextBtn);

    gvbox->addWidget(secDesign.outer);

    // ── Zeilen / Spalten ──────────────────────────────────────────────────────
    auto secRC = makeSection("Rows / Columns", true, m_tableGroup);
    auto* rcVbox = new QVBoxLayout(secRC.content);
    rcVbox->setContentsMargins(4, 2, 4, 4);
    rcVbox->setSpacing(4);

    auto* rowBtnRow = new QHBoxLayout;
    auto* addRowBtn = new QPushButton("+ Row", secRC.content);
    auto* delRowBtn = new QPushButton("- Row", secRC.content);
    auto* addColBtn = new QPushButton("+ Column", secRC.content);
    auto* delColBtn = new QPushButton("- Column", secRC.content);
    rowBtnRow->addWidget(addRowBtn);
    rowBtnRow->addWidget(delRowBtn);
    rowBtnRow->addWidget(addColBtn);
    rowBtnRow->addWidget(delColBtn);
    rcVbox->addLayout(rowBtnRow);

    gvbox->addWidget(secRC.outer);

    // ── Ausgewählte Zelle ─────────────────────────────────────────────────────
    auto secCell = makeSection("Selected Cell", true, m_tableGroup);
    auto* formC  = makeForm(secCell.content);

    m_cellBgBtn = new QPushButton(secCell.content);
    m_cellBgBtn->setFixedHeight(24);
    formC->addRow("Background:", m_cellBgBtn);

    m_cellTextBtn = new QPushButton(secCell.content);
    m_cellTextBtn->setFixedHeight(24);
    formC->addRow("Text Color:", m_cellTextBtn);

    m_cellBoldChk   = new QCheckBox("Bold",         secCell.content);
    m_cellItalicChk = new QCheckBox("Italic",       secCell.content);
    auto* styleRow  = new QHBoxLayout;
    styleRow->addWidget(m_cellBoldChk);
    styleRow->addWidget(m_cellItalicChk);
    formC->addRow(styleRow);

    m_cellAlignCombo = new QComboBox(secCell.content);
    m_cellAlignCombo->addItems({"Left", "Center", "Right"});
    formC->addRow("Alignment:", m_cellAlignCombo);

    m_cellSection = secCell.outer;
    gvbox->addWidget(secCell.outer);

    m_tableGroup->setEnabled(false);
    m_tableGroup->setVisible(false);

    // Signals
    connect(m_tBorderColorBtn,  &QPushButton::clicked, this, &PropertiesPanel::onTableBorderColorClicked);
    connect(m_tBorderWidth,     &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (m_updating) return;
        if (auto* e = getElem(m_pres, m_slideIdx, m_elemIdx)) {
            e->tableBorderWidth = float(v); emit elementModified();
        }
    });
    connect(m_tFontSize, &QSpinBox::valueChanged, this, [this](int v) {
        if (m_updating) return;
        if (auto* e = getElem(m_pres, m_slideIdx, m_elemIdx)) {
            e->tableFontSize = v; emit elementModified();
        }
    });
    connect(m_tDefaultBgBtn,   &QPushButton::clicked, this, &PropertiesPanel::onTableDefaultBgClicked);
    connect(m_tDefaultTextBtn, &QPushButton::clicked, this, &PropertiesPanel::onTableDefaultTextClicked);
    connect(m_tHasHeader, &QCheckBox::toggled, this, [this](bool on) {
        if (m_updating) return;
        if (auto* e = getElem(m_pres, m_slideIdx, m_elemIdx)) {
            e->tableHasHeader = on;
            m_tHeaderBgBtn->setEnabled(on);
            m_tHeaderTextBtn->setEnabled(on);
            emit elementModified();
        }
    });
    connect(m_tHeaderBgBtn,   &QPushButton::clicked, this, &PropertiesPanel::onTableHeaderBgClicked);
    connect(m_tHeaderTextBtn, &QPushButton::clicked, this, &PropertiesPanel::onTableHeaderTextClicked);
    connect(addRowBtn, &QPushButton::clicked, this, &PropertiesPanel::onTableAddRow);
    connect(delRowBtn, &QPushButton::clicked, this, &PropertiesPanel::onTableDelRow);
    connect(addColBtn, &QPushButton::clicked, this, &PropertiesPanel::onTableAddCol);
    connect(delColBtn, &QPushButton::clicked, this, &PropertiesPanel::onTableDelCol);
    connect(m_cellBgBtn,   &QPushButton::clicked, this, &PropertiesPanel::onCellBgColorClicked);
    connect(m_cellTextBtn, &QPushButton::clicked, this, &PropertiesPanel::onCellTextColorClicked);
    connect(m_cellBoldChk,    &QCheckBox::toggled,            this, &PropertiesPanel::onCellBoldChanged);
    connect(m_cellItalicChk,  &QCheckBox::toggled,            this, &PropertiesPanel::onCellItalicChanged);
    connect(m_cellAlignCombo, &QComboBox::currentIndexChanged, this, &PropertiesPanel::onCellAlignChanged);
}

// ── Public API ────────────────────────────────────────────────────────────────

void PropertiesPanel::setSlide(Presentation* pres, int slideIndex) {
    m_pres     = pres;
    m_slideIdx = slideIndex;
    m_elemIdx  = -1;
    m_cellRow  = -1;
    m_cellCol  = -1;
    m_elemGroup->setEnabled(false);
    m_elemGroup->setVisible(true);
    m_tableGroup->setEnabled(false);
    m_tableGroup->setVisible(false);
    m_chartGroup->setEnabled(false);
    m_chartGroup->setVisible(false);
    rebuildVisibilitySection();
    refreshProject();
    refreshSlide();
}

void PropertiesPanel::setSelectedElement(int elemIndex) {
    m_multiElemIndices.clear();
    m_elemIdx  = elemIndex;
    m_cellRow  = -1;
    m_cellCol  = -1;
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIdx) : nullptr;
    bool isTable = s && elemIndex >= 0 && elemIndex < s->elements.size()
                   && s->elements[elemIndex].type == SlideElement::Table;
    bool isChart = s && elemIndex >= 0 && elemIndex < s->elements.size()
                   && s->elements[elemIndex].type == SlideElement::Chart;
    m_elemGroup->setEnabled(elemIndex >= 0 && !isTable && !isChart);
    m_elemGroup->setVisible(!isTable && !isChart || elemIndex < 0);
    m_tableGroup->setEnabled(isTable);
    m_tableGroup->setVisible(isTable);
    m_chartGroup->setEnabled(isChart);
    m_chartGroup->setVisible(isChart);
    m_cellSection->setVisible(false);
    if (isTable)       refreshTable();
    else if (isChart)  refreshChart();
    else               refreshElement();
}

void PropertiesPanel::setSelectedElements(const QVector<int>& indices) {
    if (indices.size() <= 1) {
        setSelectedElement(indices.isEmpty() ? -1 : indices.first());
        return;
    }
    m_multiElemIndices = indices;
    m_elemIdx = indices.first();
    m_cellRow = -1;
    m_cellCol = -1;
    m_tableGroup->setEnabled(false);
    m_tableGroup->setVisible(false);
    m_chartGroup->setEnabled(false);
    m_chartGroup->setVisible(false);
    m_cellSection->setVisible(false);
    m_elemGroup->setEnabled(true);
    m_elemGroup->setVisible(true);
    refreshElement();
}

QVector<int> PropertiesPanel::targetElemIndices() const {
    return m_multiElemIndices.isEmpty() ? QVector<int>{m_elemIdx} : m_multiElemIndices;
}

void PropertiesPanel::setSelectedTableCell(int row, int col) {
    m_cellRow = row;
    m_cellCol = col;
    bool valid = row >= 0 && col >= 0;
    m_cellSection->setVisible(valid);
    if (valid) refreshTableCell();
}

void PropertiesPanel::setSelectedWorldObject(int index) {
    m_worldObjIdx = index;
    if (index >= 0) {
        m_slideGroup->setVisible(false);
        m_elemGroup->setVisible(false);
        m_tableGroup->setVisible(false);
        m_chartGroup->setVisible(false);
        m_worldObjGroup->setVisible(true);
        m_worldObjGroup->setEnabled(true);
        refreshWorldObj();
    } else {
        m_worldObjGroup->setVisible(false);
        m_worldObjGroup->setEnabled(false);
    }
}

// ── Visibility section ────────────────────────────────────────────────────────

void PropertiesPanel::rebuildVisibilitySection() {
    m_visRows.clear();
    while (QLayoutItem* item = m_visLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) w->deleteLater();
        delete item;
    }

    if (!m_pres) return;
    const Slide* cur = m_pres->slideAt(m_slideIdx);
    if (!cur) return;

    float defOpa = m_pres->defaultInactiveOpacity;

    for (const Slide& other : m_pres->slides) {
        if (other.id == cur->id) continue;

        float opa = defOpa;
        if (cur->visibilityOverrides.contains(other.id))
            opa = cur->visibilityOverrides[other.id];

        auto* row = new QWidget(m_visContainer);
        auto* hl  = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(4);

        auto* chk = new QCheckBox(other.name.isEmpty() ? other.id.left(8) : other.name, row);
        chk->setChecked(opa > 0.0f);
        chk->setStyleSheet("font-size: 10px;");

        auto* spin = new QDoubleSpinBox(row);
        spin->setRange(0.01, 1.0);
        spin->setSingleStep(0.05);
        spin->setDecimals(2);
        spin->setValue(opa > 0.0f ? opa : defOpa);
        spin->setFixedWidth(60);
        spin->setEnabled(opa > 0.0f);

        hl->addWidget(chk, 1);
        hl->addWidget(spin);
        m_visLayout->addWidget(row);

        VisRow vr;
        vr.slideId = other.id;
        vr.check   = chk;
        vr.spin    = spin;
        m_visRows.append(vr);

        int rowIdx = m_visRows.size() - 1;

        connect(chk, &QCheckBox::toggled, this, [this, rowIdx](bool on) {
            if (m_updating) return;
            m_visRows[rowIdx].spin->setEnabled(on);
            Slide* s = m_pres ? m_pres->slideAt(m_slideIdx) : nullptr;
            if (!s) return;
            if (on)
                s->visibilityOverrides[m_visRows[rowIdx].slideId] =
                    float(m_visRows[rowIdx].spin->value());
            else
                s->visibilityOverrides[m_visRows[rowIdx].slideId] = 0.0f;
            emit slideModified();
        });

        connect(spin, &QDoubleSpinBox::valueChanged, this, [this, rowIdx](double v) {
            if (m_updating) return;
            Slide* s = m_pres ? m_pres->slideAt(m_slideIdx) : nullptr;
            if (!s || !m_visRows[rowIdx].check->isChecked()) return;
            s->visibilityOverrides[m_visRows[rowIdx].slideId] = float(v);
            emit slideModified();
        });
    }
}

// ── Refresh helpers ───────────────────────────────────────────────────────────

void PropertiesPanel::refreshProject() {
    if (!m_pres) return;
    m_updating = true;
    m_titleEdit->setText(m_pres->title);
    updateColorButton(m_sceneBgBtn, m_pres->sceneBackground);
    m_slideW->setValue(m_pres->slideWidth);
    m_slideH->setValue(m_pres->slideHeight);
    m_defaultInactiveOpa->setValue(m_pres->defaultInactiveOpacity);
    m_updating = false;
}

void PropertiesPanel::refreshSlide() {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIdx) : nullptr;
    if (!s) { m_slideGroup->setEnabled(false); return; }
    m_slideGroup->setEnabled(true);
    m_updating = true;
    m_slideName->setText(s->name);
    updateColorButton(m_bgColorBtn, s->backgroundColor);
    m_posX->setValue(s->posX);
    m_posY->setValue(s->posY);
    m_posZ->setValue(s->posZ);
    m_rotX->setValue(s->rotX);
    m_rotY->setValue(s->rotY);
    m_rotZ->setValue(s->rotZ);
    m_scale->setValue(s->scale);
    m_slideOwnW->setValue(s->slideWidth);
    m_slideOwnH->setValue(s->slideHeight);
    m_viewOffX->setValue(s->viewOffsetX);
    m_viewOffY->setValue(s->viewOffsetY);

    float defOpa = m_pres->defaultInactiveOpacity;
    for (VisRow& vr : m_visRows) {
        float opa = s->visibilityOverrides.contains(vr.slideId)
                    ? s->visibilityOverrides[vr.slideId]
                    : defOpa;
        vr.check->setChecked(opa > 0.0f);
        vr.spin->setValue(opa > 0.0f ? opa : defOpa);
        vr.spin->setEnabled(opa > 0.0f);
    }

    m_updating = false;
}

void PropertiesPanel::refreshElement() {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIdx) : nullptr;
    if (!s || m_elemIdx < 0 || m_elemIdx >= s->elements.size()) return;
    const SlideElement& e = s->elements[m_elemIdx];
    const bool multi = m_multiElemIndices.size() > 1;

    m_updating = true;

    static const char* typeNames[] = {"Text", "Shape", "Image", "Table", "Chart", "Formula", "iFrame", "Button", "Checkbox", "Slider", "Icon"};
    m_elemType->setText(multi ? QString("Gruppe (%1 Elemente)").arg(m_multiElemIndices.size())
                              : typeNames[e.type]);

    // Per-instance fields (content, position/size, font, alignment) only make
    // sense for one element — a group's members can differ in all of these,
    // and its position/size is already edited by dragging on the canvas.
    m_elemContent->setEnabled(!multi && e.type != SlideElement::Shape && e.type != SlideElement::Icon);
    m_elemContent->setText(multi ? QString() : e.content);

    m_eX->setEnabled(!multi); m_eY->setEnabled(!multi);
    m_eW->setEnabled(!multi); m_eH->setEnabled(!multi);
    m_eX->setValue(e.x);
    m_eY->setValue(e.y);
    m_eW->setValue(e.width);
    m_eH->setValue(e.height);

    updateColorButton(m_eColorBtn,   e.color);
    updateColorButton(m_eBgColorBtn, e.backgroundColor == Qt::transparent
                                         ? Qt::white : e.backgroundColor);

    m_eFontSize->setEnabled(!multi && (e.type == SlideElement::Text || e.type == SlideElement::Formula
                             || e.type == SlideElement::Button || e.type == SlideElement::Checkbox
                             || e.type == SlideElement::Slider));
    m_eFontSize->setValue(e.fontSize);

    m_eAlign->setEnabled(!multi && e.type == SlideElement::Text);
    int alignIdx = (e.textAlignment == "center") ? 1
                 : (e.textAlignment == "right")  ? 2 : 0;
    m_eAlign->setCurrentIndex(alignIdx);

    // Fill/border/corner-radius/opacity below all exist on every SlideElement
    // regardless of type, so — unlike the fields above — they stay available
    // and apply to every member when a group/multi-selection is active (this
    // is the "abrunden stylen usw." group-styling behavior).
    bool isShape = (e.type == SlideElement::Shape);
    m_elemFormSection->setVisible(isShape || multi);
    if (isShape || multi) {
        m_eBorderW->setValue(e.borderWidth);
        updateColorButton(m_eBorderColorBtn, e.borderColor.isValid()
                          ? e.borderColor : Qt::darkGray);
        m_eCornerRadius->setValue(e.cornerRadius);
    }

    m_elemIconSection->setVisible(!multi && e.type == SlideElement::Icon);

    m_eOpacity->setValue(e.opacity);

    m_updating = false;
}

void PropertiesPanel::updateColorButton(QPushButton* btn, const QColor& c) {
    static const QString kToolTipSS =
        "QToolTip { background:#1f2937; color:#f9fafb; border:none; padding:4px 8px; "
        "           border-radius:4px; font-size:10px; }";
    if (!c.isValid() || c == Qt::transparent) {
        btn->setStyleSheet("background: transparent; border: 1px dashed #888;" + kToolTipSS);
        btn->setText("Transparent");
    } else {
        QString hex = c.name(QColor::HexRgb);
        QString fg  = c.lightnessF() > 0.5f ? "#000" : "#fff";
        btn->setStyleSheet(QString("background:%1; color:%2; border:1px solid #666;").arg(hex,fg) + kToolTipSS);
        btn->setText(hex);
    }
    btn->setProperty("color", c);
}

// ── Project slots ─────────────────────────────────────────────────────────────

void PropertiesPanel::onTitleChanged(const QString& text) {
    if (m_updating || !m_pres) return;
    m_pres->title = text;
    emit presentationSettingsModified();
}

void PropertiesPanel::onSceneBgClicked() {
    if (!m_pres) return;
    QColor c = QColorDialog::getColor(m_pres->sceneBackground, this,
                                      "Project Background");
    if (!c.isValid()) return;
    m_pres->sceneBackground = c;
    updateColorButton(m_sceneBgBtn, c);
    emit presentationSettingsModified();
}

void PropertiesPanel::onSlideSizeChanged() {
    if (m_updating || !m_pres) return;
    m_pres->slideWidth  = float(m_slideW->value());
    m_pres->slideHeight = float(m_slideH->value());
    emit presentationSettingsModified();
}

void PropertiesPanel::onDefaultInactiveOpaChanged() {
    if (m_updating || !m_pres) return;
    m_pres->defaultInactiveOpacity = float(m_defaultInactiveOpa->value());
    emit presentationSettingsModified();
}

// ── Slide slots ───────────────────────────────────────────────────────────────

void PropertiesPanel::onSlideNameChanged(const QString& name) {
    if (m_updating) return;
    Slide* s = m_pres ? m_pres->slideAt(m_slideIdx) : nullptr;
    if (!s || name.isEmpty()) return;
    s->name = name;
    emit slideModified();
}

void PropertiesPanel::onBgColorClicked() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIdx) : nullptr;
    if (!s) return;
    QColor init = (s->backgroundColor.isValid() && s->backgroundColor != Qt::transparent)
                  ? s->backgroundColor : Qt::white;
    QColor c = QColorDialog::getColor(init, this, "Slide Background (Alpha=0 → transparent)",
                                      QColorDialog::ShowAlphaChannel);
    if (c.isValid()) {
        s->backgroundColor = (c.alpha() == 0) ? Qt::transparent : c;
        updateColorButton(m_bgColorBtn, s->backgroundColor);
        emit slideModified();
    }
}

void PropertiesPanel::onPosChanged() {
    if (m_updating) return;
    Slide* s = m_pres ? m_pres->slideAt(m_slideIdx) : nullptr;
    if (!s) return;
    s->posX = float(m_posX->value());
    s->posY = float(m_posY->value());
    s->posZ = float(m_posZ->value());
    emit slideModified();
}

void PropertiesPanel::onRotChanged() {
    if (m_updating) return;
    Slide* s = m_pres ? m_pres->slideAt(m_slideIdx) : nullptr;
    if (!s) return;
    s->rotX = float(m_rotX->value());
    s->rotY = float(m_rotY->value());
    s->rotZ = float(m_rotZ->value());
    emit slideModified();
}

void PropertiesPanel::onScaleChanged() {
    if (m_updating) return;
    Slide* s = m_pres ? m_pres->slideAt(m_slideIdx) : nullptr;
    if (!s) return;
    s->scale = float(m_scale->value());
    emit slideModified();
}

void PropertiesPanel::onSlideOwnSizeChanged() {
    if (m_updating) return;
    Slide* s = m_pres ? m_pres->slideAt(m_slideIdx) : nullptr;
    if (!s) return;
    s->slideWidth  = float(m_slideOwnW->value());
    s->slideHeight = float(m_slideOwnH->value());
    emit slideModified();
}

void PropertiesPanel::onViewOffsetChanged() {
    if (m_updating) return;
    Slide* s = m_pres ? m_pres->slideAt(m_slideIdx) : nullptr;
    if (!s) return;
    s->viewOffsetX = float(m_viewOffX->value());
    s->viewOffsetY = float(m_viewOffY->value());
    emit slideModified();
}

// ── Element slots ─────────────────────────────────────────────────────────────

SlideElement* getElem(Presentation* p, int si, int ei) {
    Slide* s = p ? p->slideAt(si) : nullptr;
    return (s && ei >= 0 && ei < s->elements.size()) ? &s->elements[ei] : nullptr;
}

void PropertiesPanel::onElemContentChanged(const QString& text) {
    if (m_updating) return;
    if (auto* e = getElem(m_pres, m_slideIdx, m_elemIdx)) {
        e->content = text;
        // This is a wholesale replacement (no diff against the old content),
        // so any existing code-span offsets would point at the wrong text.
        e->codeSpans.clear();
        emit elementModified();
    }
}

void PropertiesPanel::onElemColorClicked() {
    auto* primary = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!primary) return;
    QColor c = QColorDialog::getColor(primary->color, this, "Text Color");
    if (!c.isValid()) return;
    for (int idx : targetElemIndices())
        if (auto* e = getElem(m_pres, m_slideIdx, idx)) e->color = c;
    updateColorButton(m_eColorBtn, c);
    emit elementModified();
}

void PropertiesPanel::onElemBgColorClicked() {
    auto* primary = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!primary) return;
    QColor c = QColorDialog::getColor(
        primary->backgroundColor == Qt::transparent ? Qt::white : primary->backgroundColor,
        this, "Background Color");
    if (!c.isValid()) return;
    for (int idx : targetElemIndices())
        if (auto* e = getElem(m_pres, m_slideIdx, idx)) e->backgroundColor = c;
    updateColorButton(m_eBgColorBtn, c);
    emit elementModified();
}

void PropertiesPanel::onElemPosChanged() {
    if (m_updating) return;
    if (auto* e = getElem(m_pres, m_slideIdx, m_elemIdx)) {
        e->x = float(m_eX->value());
        e->y = float(m_eY->value());
        emit elementModified();
    }
}

void PropertiesPanel::onElemSizeChanged() {
    if (m_updating) return;
    if (auto* e = getElem(m_pres, m_slideIdx, m_elemIdx)) {
        e->width  = float(m_eW->value());
        e->height = float(m_eH->value());
        emit elementModified();
    }
}

void PropertiesPanel::onElemFontSizeChanged(int sz) {
    if (m_updating) return;
    if (auto* e = getElem(m_pres, m_slideIdx, m_elemIdx)) {
        e->fontSize = sz;
        emit elementModified();
    }
}

void PropertiesPanel::onElemAlignChanged(int idx) {
    if (m_updating) return;
    if (auto* e = getElem(m_pres, m_slideIdx, m_elemIdx)) {
        e->textAlignment = idx == 1 ? "center" : idx == 2 ? "right" : "left";
        emit elementModified();
    }
}

void PropertiesPanel::onElemBorderChanged() {
    if (m_updating) return;
    float w = float(m_eBorderW->value());
    bool any = false;
    for (int idx : targetElemIndices())
        if (auto* e = getElem(m_pres, m_slideIdx, idx)) { e->borderWidth = w; any = true; }
    if (any) emit elementModified();
}

void PropertiesPanel::onElemBorderColorClicked() {
    auto* primary = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!primary) return;
    QColor init = primary->borderColor.isValid() ? primary->borderColor : Qt::darkGray;
    QColor c = QColorDialog::getColor(init, this, "Border Color");
    if (!c.isValid()) return;
    for (int idx : targetElemIndices())
        if (auto* e = getElem(m_pres, m_slideIdx, idx)) e->borderColor = c;
    updateColorButton(m_eBorderColorBtn, c);
    emit elementModified();
}

void PropertiesPanel::onElemChangeIconClicked() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e || e->type != SlideElement::Icon) return;
    IconPickerDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted && !dlg.selectedIcon().isEmpty()) {
        e->content = dlg.selectedIcon();
        emit elementModified();
    }
}

void PropertiesPanel::onElemCornerRadiusChanged() {
    if (m_updating) return;
    float r = float(m_eCornerRadius->value());
    bool any = false;
    for (int idx : targetElemIndices())
        if (auto* e = getElem(m_pres, m_slideIdx, idx)) { e->cornerRadius = r; any = true; }
    if (any) emit elementModified();
}

void PropertiesPanel::onElemOpacityChanged(double v) {
    if (m_updating) return;
    float op = float(v);
    bool any = false;
    for (int idx : targetElemIndices())
        if (auto* e = getElem(m_pres, m_slideIdx, idx)) { e->opacity = op; any = true; }
    if (any) emit elementModified();
}

// ── Table refresh ─────────────────────────────────────────────────────────────

void PropertiesPanel::refreshTable() {
    const auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e || e->type != SlideElement::Table) return;
    m_updating = true;
    updateColorButton(m_tBorderColorBtn, e->tableBorderColor);
    m_tBorderWidth->setValue(e->tableBorderWidth);
    m_tFontSize->setValue(e->tableFontSize);
    updateColorButton(m_tDefaultBgBtn,   e->tableDefaultBg);
    updateColorButton(m_tDefaultTextBtn, e->tableDefaultText);
    m_tHasHeader->setChecked(e->tableHasHeader);
    m_tHeaderBgBtn->setEnabled(e->tableHasHeader);
    m_tHeaderTextBtn->setEnabled(e->tableHasHeader);
    updateColorButton(m_tHeaderBgBtn,   e->tableHeaderBg);
    updateColorButton(m_tHeaderTextBtn, e->tableHeaderText);
    m_updating = false;
}

void PropertiesPanel::refreshTableCell() {
    const auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e || e->type != SlideElement::Table) return;
    if (m_cellRow < 0 || m_cellRow >= e->tableRows ||
        m_cellCol < 0 || m_cellCol >= e->tableCols) return;
    const TableCell& cell = e->tableCells[m_cellRow][m_cellCol];
    m_updating = true;
    updateColorButton(m_cellBgBtn,
        cell.bgColor.isValid() ? cell.bgColor : e->tableDefaultBg);
    updateColorButton(m_cellTextBtn,
        cell.textColor.isValid() ? cell.textColor : e->tableDefaultText);
    m_cellBoldChk->setChecked(cell.bold);
    m_cellItalicChk->setChecked(cell.italic);
    int alignIdx = (cell.textAlign == "center") ? 1 : (cell.textAlign == "right") ? 2 : 0;
    m_cellAlignCombo->setCurrentIndex(alignIdx);
    m_updating = false;
}

// ── Table slots ───────────────────────────────────────────────────────────────

static TableCell* getCell(Presentation* p, int si, int ei, int row, int col) {
    Slide* s = p ? p->slideAt(si) : nullptr;
    if (!s || ei < 0 || ei >= s->elements.size()) return nullptr;
    SlideElement& e = s->elements[ei];
    if (e.type != SlideElement::Table) return nullptr;
    if (row < 0 || row >= e.tableRows || col < 0 || col >= e.tableCols) return nullptr;
    return &e.tableCells[row][col];
}

void PropertiesPanel::onTableBorderColorClicked() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e) return;
    QColor c = QColorDialog::getColor(e->tableBorderColor, this, "Border Color");
    if (c.isValid()) { e->tableBorderColor = c; updateColorButton(m_tBorderColorBtn, c); emit elementModified(); }
}
void PropertiesPanel::onTableHeaderBgClicked() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e) return;
    QColor c = QColorDialog::getColor(e->tableHeaderBg, this, "Header Row Background");
    if (c.isValid()) { e->tableHeaderBg = c; updateColorButton(m_tHeaderBgBtn, c); emit elementModified(); }
}
void PropertiesPanel::onTableHeaderTextClicked() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e) return;
    QColor c = QColorDialog::getColor(e->tableHeaderText, this, "Header Row Text Color");
    if (c.isValid()) { e->tableHeaderText = c; updateColorButton(m_tHeaderTextBtn, c); emit elementModified(); }
}
void PropertiesPanel::onTableDefaultBgClicked() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e) return;
    QColor c = QColorDialog::getColor(e->tableDefaultBg, this, "Default Cell Background");
    if (c.isValid()) { e->tableDefaultBg = c; updateColorButton(m_tDefaultBgBtn, c); emit elementModified(); }
}
void PropertiesPanel::onTableDefaultTextClicked() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e) return;
    QColor c = QColorDialog::getColor(e->tableDefaultText, this, "Default Cell Text Color");
    if (c.isValid()) { e->tableDefaultText = c; updateColorButton(m_tDefaultTextBtn, c); emit elementModified(); }
}

void PropertiesPanel::onTableAddRow() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e || e->type != SlideElement::Table) return;
    e->tableCells.append(QVector<TableCell>(e->tableCols));
    float newFrac = 1.f / float(e->tableRows + 1);
    for (float& f : e->tableRowFracs) f *= float(e->tableRows) / float(e->tableRows + 1);
    e->tableRowFracs.append(newFrac);
    e->tableRows++;
    emit elementModified();
}
void PropertiesPanel::onTableDelRow() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e || e->type != SlideElement::Table || e->tableRows <= 1) return;
    float removed = e->tableRowFracs.last();
    e->tableRowFracs.removeLast();
    e->tableCells.removeLast();
    e->tableRows--;
    float total = 0; for (float f : e->tableRowFracs) total += f;
    if (total > 0) for (float& f : e->tableRowFracs) f /= total;
    Q_UNUSED(removed);
    emit elementModified();
}
void PropertiesPanel::onTableAddCol() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e || e->type != SlideElement::Table) return;
    for (auto& row : e->tableCells) row.append(TableCell{});
    float newFrac = 1.f / float(e->tableCols + 1);
    for (float& f : e->tableColFracs) f *= float(e->tableCols) / float(e->tableCols + 1);
    e->tableColFracs.append(newFrac);
    e->tableCols++;
    emit elementModified();
}
void PropertiesPanel::onTableDelCol() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e || e->type != SlideElement::Table || e->tableCols <= 1) return;
    for (auto& row : e->tableCells) row.removeLast();
    e->tableColFracs.removeLast();
    e->tableCols--;
    float total = 0; for (float f : e->tableColFracs) total += f;
    if (total > 0) for (float& f : e->tableColFracs) f /= total;
    emit elementModified();
}

void PropertiesPanel::onCellBgColorClicked() {
    auto* cell = getCell(m_pres, m_slideIdx, m_elemIdx, m_cellRow, m_cellCol);
    if (!cell) return;
    const auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    QColor init = cell->bgColor.isValid() ? cell->bgColor : (e ? e->tableDefaultBg : Qt::white);
    QColor c = QColorDialog::getColor(init, this, "Cell Background");
    if (c.isValid()) { cell->bgColor = c; updateColorButton(m_cellBgBtn, c); emit elementModified(); }
}
void PropertiesPanel::onCellTextColorClicked() {
    auto* cell = getCell(m_pres, m_slideIdx, m_elemIdx, m_cellRow, m_cellCol);
    if (!cell) return;
    const auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    QColor init = cell->textColor.isValid() ? cell->textColor : (e ? e->tableDefaultText : Qt::black);
    QColor c = QColorDialog::getColor(init, this, "Cell Text Color");
    if (c.isValid()) { cell->textColor = c; updateColorButton(m_cellTextBtn, c); emit elementModified(); }
}
void PropertiesPanel::onCellBoldChanged(bool on) {
    if (m_updating) return;
    if (auto* cell = getCell(m_pres, m_slideIdx, m_elemIdx, m_cellRow, m_cellCol))
        { cell->bold = on; emit elementModified(); }
}
void PropertiesPanel::onCellItalicChanged(bool on) {
    if (m_updating) return;
    if (auto* cell = getCell(m_pres, m_slideIdx, m_elemIdx, m_cellRow, m_cellCol))
        { cell->italic = on; emit elementModified(); }
}
void PropertiesPanel::onCellAlignChanged(int idx) {
    if (m_updating) return;
    if (auto* cell = getCell(m_pres, m_slideIdx, m_elemIdx, m_cellRow, m_cellCol)) {
        cell->textAlign = idx == 1 ? "center" : idx == 2 ? "right" : "left";
        emit elementModified();
    }
}

// ── Chart group ───────────────────────────────────────────────────────────────

void PropertiesPanel::buildChartGroup() {
    m_chartGroup = new QGroupBox("Chart", this);
    auto* vbox = new QVBoxLayout(m_chartGroup);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(4);

    m_chartTypeLabel  = new QLabel(this);
    m_chartTitleLabel = new QLabel(this);
    m_chartTitleLabel->setWordWrap(true);
    form->addRow("Type:", m_chartTypeLabel);
    form->addRow("Title:", m_chartTitleLabel);
    vbox->addLayout(form);

    m_chartEditBtn = new QPushButton("Edit Chart...", this);
    m_chartEditBtn->setStyleSheet(
        "QPushButton { background:#eff6ff; color:#2563eb; border:1px solid #bfdbfe; "
        "              border-radius:4px; padding:4px 8px; }"
        "QPushButton:hover { background:#dbeafe; }");
    vbox->addWidget(m_chartEditBtn);

    m_chartGroup->setEnabled(false);
    m_chartGroup->setVisible(false);

    connect(m_chartEditBtn, &QPushButton::clicked, this, [this]() {
        auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
        if (!e || e->type != SlideElement::Chart) return;
        ChartEditorDialog dlg(e->chartData, this);
        if (dlg.exec() == QDialog::Accepted) {
            e->chartData = dlg.chartData();
            refreshChart();
            emit elementModified();
        }
    });
}

void PropertiesPanel::refreshChart() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e || e->type != SlideElement::Chart) return;
    m_chartTypeLabel->setText(ChartRenderer::typeName(e->chartData.type));
    m_chartTitleLabel->setText(e->chartData.title.isEmpty()
                               ? "(no title)" : e->chartData.title);
}

// ── World object group ──────────────────────────────────────────────────────

void PropertiesPanel::buildWorldObjGroup() {
    m_worldObjGroup = new QGroupBox("World Object", this);
    auto* gvbox = new QVBoxLayout(m_worldObjGroup);
    gvbox->setContentsMargins(4, 8, 4, 4);
    gvbox->setSpacing(2);

    // ── Model (open) ────────────────────────────────────────────────────────
    auto secModel = makeSection("Model", true, m_worldObjGroup);
    auto* formModel = makeForm(secModel.content);

    m_woModelLabel = new QLabel(secModel.content);
    m_woModelLabel->setWordWrap(true);
    m_woModelLabel->setStyleSheet("color:#374151;");
    formModel->addRow("File:", m_woModelLabel);

    m_woChangeModelBtn = new QPushButton("Change Model...", secModel.content);
    formModel->addRow("", m_woChangeModelBtn);

    gvbox->addWidget(secModel.outer);

    // ── Transform (open) ───────────────────────────────────────────────────
    auto secXform = makeSection("Transform", true, m_worldObjGroup);
    auto* formXform = makeForm(secXform.content);

    m_woPosX = makeSpin(-99999, 99999, 100);
    m_woPosY = makeSpin(-99999, 99999, 100);
    m_woPosZ = makeSpin(-99999, 99999, 100);
    formXform->addRow("X:", m_woPosX);
    formXform->addRow("Y:", m_woPosY);
    formXform->addRow("Z:", m_woPosZ);

    m_woRotX = makeSpin(-360, 360, 5);
    m_woRotY = makeSpin(-360, 360, 5);
    m_woRotZ = makeSpin(-360, 360, 5);
    formXform->addRow("Rot X:", m_woRotX);
    formXform->addRow("Rot Y:", m_woRotY);
    formXform->addRow("Rot Z:", m_woRotZ);

    m_woScale = makeSpin(0.01, 1000, 0.1);
    m_woScale->setDecimals(2);
    formXform->addRow("Scale:", m_woScale);

    gvbox->addWidget(secXform.outer);

    // ── Appearance (open) ──────────────────────────────────────────────────
    auto secApp = makeSection("Appearance", true, m_worldObjGroup);
    auto* formApp = makeForm(secApp.content);

    m_woOpacity = makeSpin(0.0, 1.0, 0.05);
    m_woOpacity->setDecimals(2);
    formApp->addRow("Opacity:", m_woOpacity);

    gvbox->addWidget(secApp.outer);

    m_woDeleteBtn = new QPushButton("Delete World Object", m_worldObjGroup);
    m_woDeleteBtn->setStyleSheet(
        "QPushButton { background:#fef2f2; color:#b91c1c; border:1px solid #fecaca; "
        "              border-radius:4px; padding:4px 8px; }"
        "QPushButton:hover { background:#fee2e2; }");
    gvbox->addWidget(m_woDeleteBtn);

    m_worldObjGroup->setEnabled(false);
    m_worldObjGroup->setVisible(false);

    connect(m_woChangeModelBtn, &QPushButton::clicked, this, &PropertiesPanel::onWoChangeModelClicked);
    connect(m_woDeleteBtn,      &QPushButton::clicked, this, &PropertiesPanel::onWoDeleteClicked);
    connect(m_woPosX, &QDoubleSpinBox::valueChanged, this, [this](double) { onWoPosChanged(); });
    connect(m_woPosY, &QDoubleSpinBox::valueChanged, this, [this](double) { onWoPosChanged(); });
    connect(m_woPosZ, &QDoubleSpinBox::valueChanged, this, [this](double) { onWoPosChanged(); });
    connect(m_woRotX, &QDoubleSpinBox::valueChanged, this, [this](double) { onWoRotChanged(); });
    connect(m_woRotY, &QDoubleSpinBox::valueChanged, this, [this](double) { onWoRotChanged(); });
    connect(m_woRotZ, &QDoubleSpinBox::valueChanged, this, [this](double) { onWoRotChanged(); });
    connect(m_woScale, &QDoubleSpinBox::valueChanged, this, [this](double) { onWoScaleChanged(); });
    connect(m_woOpacity, &QDoubleSpinBox::valueChanged, this, [this](double) { onWoOpacityChanged(); });
}

void PropertiesPanel::refreshWorldObj() {
    WorldObject* w = (m_pres && m_worldObjIdx >= 0 && m_worldObjIdx < m_pres->worldObjects.size())
                     ? &m_pres->worldObjects[m_worldObjIdx] : nullptr;
    if (!w) { m_worldObjGroup->setEnabled(false); return; }
    m_worldObjGroup->setEnabled(true);
    m_updating = true;
    m_woModelLabel->setText(QFileInfo(w->modelPath).fileName());
    m_woModelLabel->setToolTip(w->modelPath);
    m_woPosX->setValue(w->posX);
    m_woPosY->setValue(w->posY);
    m_woPosZ->setValue(w->posZ);
    m_woRotX->setValue(w->rotX);
    m_woRotY->setValue(w->rotY);
    m_woRotZ->setValue(w->rotZ);
    m_woScale->setValue(w->scale);
    m_woOpacity->setValue(w->opacity);
    m_updating = false;
}

void PropertiesPanel::onWoPosChanged() {
    if (m_updating) return;
    WorldObject* w = (m_pres && m_worldObjIdx >= 0 && m_worldObjIdx < m_pres->worldObjects.size())
                     ? &m_pres->worldObjects[m_worldObjIdx] : nullptr;
    if (!w) return;
    w->posX = float(m_woPosX->value());
    w->posY = float(m_woPosY->value());
    w->posZ = float(m_woPosZ->value());
    emit worldObjectModified();
}

void PropertiesPanel::onWoRotChanged() {
    if (m_updating) return;
    WorldObject* w = (m_pres && m_worldObjIdx >= 0 && m_worldObjIdx < m_pres->worldObjects.size())
                     ? &m_pres->worldObjects[m_worldObjIdx] : nullptr;
    if (!w) return;
    w->rotX = float(m_woRotX->value());
    w->rotY = float(m_woRotY->value());
    w->rotZ = float(m_woRotZ->value());
    emit worldObjectModified();
}

void PropertiesPanel::onWoScaleChanged() {
    if (m_updating) return;
    WorldObject* w = (m_pres && m_worldObjIdx >= 0 && m_worldObjIdx < m_pres->worldObjects.size())
                     ? &m_pres->worldObjects[m_worldObjIdx] : nullptr;
    if (!w) return;
    w->scale = float(m_woScale->value());
    emit worldObjectModified();
}

void PropertiesPanel::onWoOpacityChanged() {
    if (m_updating) return;
    WorldObject* w = (m_pres && m_worldObjIdx >= 0 && m_worldObjIdx < m_pres->worldObjects.size())
                     ? &m_pres->worldObjects[m_worldObjIdx] : nullptr;
    if (!w) return;
    w->opacity = float(m_woOpacity->value());
    emit worldObjectModified();
}

void PropertiesPanel::onWoChangeModelClicked() {
    WorldObject* w = (m_pres && m_worldObjIdx >= 0 && m_worldObjIdx < m_pres->worldObjects.size())
                     ? &m_pres->worldObjects[m_worldObjIdx] : nullptr;
    if (!w) return;
    QString path = QFileDialog::getOpenFileName(
        this, "Select 3D Model", QFileInfo(w->modelPath).absolutePath(),
        "glTF Models (*.gltf *.glb)");
    if (path.isEmpty()) return;
    w->modelPath = path;
    m_woModelLabel->setText(QFileInfo(path).fileName());
    m_woModelLabel->setToolTip(path);
    emit worldObjectModified();
}

void PropertiesPanel::onWoDeleteClicked() {
    if (!m_pres || m_worldObjIdx < 0 || m_worldObjIdx >= m_pres->worldObjects.size()) return;
    m_pres->removeWorldObjectAt(m_worldObjIdx);
    m_worldObjIdx = -1;
    m_worldObjGroup->setVisible(false);
    m_worldObjGroup->setEnabled(false);
    emit worldObjectDeleteRequested();
}
