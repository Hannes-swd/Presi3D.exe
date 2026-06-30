#include "PropertiesPanel.h"
#include "rendering/ChartRenderer.h"
#include "dialogs/ChartEditorDialog.h"
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

    auto* title = new QLabel("EIGENSCHAFTEN", inner);
    title->setStyleSheet("font-weight: bold; font-size: 10px; color: #374151; padding: 8px 8px 4px 8px; letter-spacing: 1px;");
    vbox->addWidget(title);

    buildProjectGroup();
    buildSlideGroup();
    buildElementGroup();
    buildTableGroup();
    buildChartGroup();
    vbox->addWidget(m_projectGroup);
    vbox->addWidget(m_slideGroup);
    vbox->addWidget(m_elemGroup);
    vbox->addWidget(m_tableGroup);
    vbox->addWidget(m_chartGroup);
    vbox->addStretch();

    scroll->setWidget(inner);
    outerLayout->addWidget(scroll);
}

void PropertiesPanel::buildProjectGroup() {
    m_projectGroup = new QGroupBox("Präsentation", this);
    auto* gvbox = new QVBoxLayout(m_projectGroup);
    gvbox->setContentsMargins(4, 8, 4, 4);
    gvbox->setSpacing(2);

    // ── Grundeinstellungen (offen) ─────────────────────────────────────────────
    auto sec1 = makeSection("Grundeinstellungen", true, m_projectGroup);
    auto* form1 = makeForm(sec1.content);

    m_sceneBgBtn = new QPushButton(sec1.content);
    m_sceneBgBtn->setFixedHeight(24);
    m_sceneBgBtn->setToolTip("Hintergrundfarbe hinter allen Folien");
    form1->addRow("Hintergrund:", m_sceneBgBtn);

    auto* sizeRow = new QHBoxLayout;
    m_slideW = makeSpin(100, 9999, 10);  m_slideW->setValue(1920);
    m_slideH = makeSpin(100, 9999, 10);  m_slideH->setValue(1080);
    sizeRow->addWidget(m_slideW);
    sizeRow->addWidget(new QLabel("×"));
    sizeRow->addWidget(m_slideH);
    form1->addRow("Foliengröße:", sizeRow);

    gvbox->addWidget(sec1.outer);

    // ── Standard Sichtbarkeit (eingeklappt) ────────────────────────────────────
    auto sec2 = makeSection("Standard Sichtbarkeit", false, m_projectGroup);
    auto* form2 = makeForm(sec2.content);

    m_defaultInactiveOpa = makeSpin(0.0, 1.0, 0.05);
    m_defaultInactiveOpa->setDecimals(2);
    m_defaultInactiveOpa->setValue(0.3);
    m_defaultInactiveOpa->setToolTip("Standard-Deckkraft für Folien ohne eigene Einstellung\n"
                                     "(gilt wenn keine per-Folie-Überschreibung gesetzt ist)");
    form2->addRow("Inaktiv Standard:", m_defaultInactiveOpa);

    gvbox->addWidget(sec2.outer);

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
    auto secBase = makeSection("Grundeinstellungen", true, m_slideGroup);
    auto* formBase = makeForm(secBase.content);

    m_slideName = new QLineEdit(secBase.content);
    formBase->addRow("Name:", m_slideName);

    m_bgColorBtn = new QPushButton(secBase.content);
    m_bgColorBtn->setFixedHeight(24);
    formBase->addRow("Hintergrund:", m_bgColorBtn);

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
    m_scale->setToolTip("Zoom der Kamera auf dieser Folie:\n"
                        "1.0 = Folie füllt die Ansicht\n"
                        "< 1 = reinzoomen (weniger Kontext)\n"
                        "> 1 = rauszoomen (mehr Kontext)");
    formRot->addRow("Zoom:", m_scale);

    gvbox->addWidget(secRot.outer);

    // ── Sichtfeld (eingeklappt) ────────────────────────────────────────────────
    auto secView = makeSection("Sichtfeld (nur 3D)", false, m_slideGroup);
    auto* formView = makeForm(secView.content);

    auto* viewOffRow = new QHBoxLayout;
    m_viewOffX = makeSpin(-9999, 9999, 50);
    m_viewOffX->setToolTip("Sichtfeld-Mitte X-Verschiebung\n(0 = Folie zentriert)");
    m_viewOffY = makeSpin(-9999, 9999, 50);
    m_viewOffY->setToolTip("Sichtfeld-Mitte Y-Verschiebung\n(0 = Folie zentriert)");
    viewOffRow->addWidget(m_viewOffX);
    viewOffRow->addWidget(new QLabel("Y:"));
    viewOffRow->addWidget(m_viewOffY);
    formView->addRow("Offset X:", viewOffRow);

    gvbox->addWidget(secView.outer);

    // ── Eigene Foliengröße (eingeklappt) ──────────────────────────────────────
    auto secSize = makeSection("Eigene Foliengröße", false, m_slideGroup);
    auto* formSize = makeForm(secSize.content);

    auto* ownSizeRow = new QHBoxLayout;
    m_slideOwnW = makeSpin(0, 9999, 10);
    m_slideOwnW->setDecimals(0);
    m_slideOwnW->setToolTip("Breite dieser Folie (0 = Präsentationsstandard verwenden)");
    m_slideOwnH = makeSpin(0, 9999, 10);
    m_slideOwnH->setDecimals(0);
    m_slideOwnH->setToolTip("Höhe dieser Folie (0 = Präsentationsstandard verwenden)");
    ownSizeRow->addWidget(m_slideOwnW);
    ownSizeRow->addWidget(new QLabel("×"));
    ownSizeRow->addWidget(m_slideOwnH);
    formSize->addRow("Größe:", ownSizeRow);

    auto* sizeHint = new QLabel("(0 = Standard)", secSize.content);
    sizeHint->setStyleSheet("color: #6b7280; font-size: 9px;");
    formSize->addRow(sizeHint);

    gvbox->addWidget(secSize.outer);

    // ── Sichtbarkeit anderer Folien (eingeklappt) ──────────────────────────────
    auto secVis = makeSection("Sichtbarkeit anderer Folien", false, m_slideGroup);
    auto* visVBox = new QVBoxLayout(secVis.content);
    visVBox->setContentsMargins(4, 2, 4, 4);
    visVBox->setSpacing(2);

    auto* visHint = new QLabel("Wenn diese Folie aktiv ist:", secVis.content);
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
    auto secBase = makeSection("Grundeinstellungen", true, m_elemGroup);
    auto* formBase = makeForm(secBase.content);

    m_elemType = new QLabel("—", secBase.content);
    m_elemType->setStyleSheet("color: #374151;");
    formBase->addRow("Typ:", m_elemType);

    m_elemContent = new QLineEdit(secBase.content);
    formBase->addRow("Inhalt:", m_elemContent);

    m_eX = makeSpin(-9999, 9999); m_eY = makeSpin(-9999, 9999);
    m_eW = makeSpin(1, 9999);     m_eH = makeSpin(1, 9999);
    formBase->addRow("X:", m_eX);
    formBase->addRow("Y:", m_eY);
    formBase->addRow("Breite:", m_eW);
    formBase->addRow("Höhe:",   m_eH);

    gvbox->addWidget(secBase.outer);

    // ── Darstellung (offen) ────────────────────────────────────────────────────
    auto secStyle = makeSection("Darstellung", true, m_elemGroup);
    auto* formStyle = makeForm(secStyle.content);

    m_eColorBtn   = new QPushButton(secStyle.content); m_eColorBtn->setFixedHeight(24);
    m_eBgColorBtn = new QPushButton(secStyle.content); m_eBgColorBtn->setFixedHeight(24);
    formStyle->addRow("Farbe:",       m_eColorBtn);
    formStyle->addRow("Hintergrund:", m_eBgColorBtn);

    m_eFontSize = new QSpinBox(secStyle.content);
    m_eFontSize->setRange(6, 200);
    m_eFontSize->setValue(32);
    formStyle->addRow("Schriftgröße:", m_eFontSize);

    m_eAlign = new QComboBox(secStyle.content);
    m_eAlign->addItems({"Links", "Zentriert", "Rechts"});
    formStyle->addRow("Ausrichtung:", m_eAlign);

    gvbox->addWidget(secStyle.outer);

    // ── Form – nur für Shapes (offen, aber initial ausgeblendet) ──────────────
    auto secForm = makeSection("Form", true, m_elemGroup);
    auto* formForm = makeForm(secForm.content);

    m_eBorderW = makeSpin(0, 200, 1);
    m_eBorderW->setDecimals(0);
    formForm->addRow("Randbreite px:", m_eBorderW);

    m_eBorderColorBtn = new QPushButton(secForm.content);
    m_eBorderColorBtn->setFixedHeight(24);
    formForm->addRow("Randfarbe:", m_eBorderColorBtn);

    m_eCornerRadius = makeSpin(0, 999, 5);
    m_eCornerRadius->setDecimals(0);
    m_eCornerRadius->setToolTip("Abrundung der Ecken in Folien-Pixeln (0 = keine Abrundung)");
    formForm->addRow("Eckenradius:", m_eCornerRadius);

    m_elemFormSection = secForm.outer;
    gvbox->addWidget(secForm.outer);

    // ── Animation (eingeklappt) ────────────────────────────────────────────────
    auto secAnim = makeSection("Animation", false, m_elemGroup);
    auto* formAnim = makeForm(secAnim.content);

    m_eAnimType = new QComboBox(secAnim.content);
    m_eAnimType->addItem("Keine",          "");
    m_eAnimType->addItem("Einblenden",     "fadeIn");
    m_eAnimType->addItem("Von links",      "slideLeft");
    m_eAnimType->addItem("Von rechts",     "slideRight");
    m_eAnimType->addItem("Von oben",       "slideUp");
    m_eAnimType->addItem("Von unten",      "slideDown");
    m_eAnimType->addItem("Zoom",           "zoomIn");
    formAnim->addRow("Animation:", m_eAnimType);

    m_animDelayLabel = new QLabel("Verzögerung (s):", secAnim.content);
    m_eAnimDelay = makeSpin(0.0, 10.0, 0.1);
    m_eAnimDelay->setDecimals(1);
    m_eAnimDelay->setValue(0.3);
    formAnim->addRow(m_animDelayLabel, m_eAnimDelay);

    m_animDurLabel = new QLabel("Dauer (s):", secAnim.content);
    m_eAnimDuration = makeSpin(0.05, 10.0, 0.1);
    m_eAnimDuration->setDecimals(1);
    m_eAnimDuration->setValue(0.5);
    formAnim->addRow(m_animDurLabel, m_eAnimDuration);

    gvbox->addWidget(secAnim.outer);

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
    connect(m_eAnimType,     &QComboBox::currentIndexChanged, this, &PropertiesPanel::onElemAnimChanged);
    connect(m_eAnimDelay,    &QDoubleSpinBox::valueChanged,   this, [this](double v){ onElemAnimDelayChanged(v); });
    connect(m_eAnimDuration, &QDoubleSpinBox::valueChanged,   this, [this](double v){ onElemAnimDurationChanged(v); });

    m_elemFormSection->setVisible(false);
    m_elemGroup->setEnabled(false);
}

void PropertiesPanel::buildTableGroup() {
    m_tableGroup = new QGroupBox("Tabelle", this);
    auto* gvbox = new QVBoxLayout(m_tableGroup);
    gvbox->setContentsMargins(4, 8, 4, 4);
    gvbox->setSpacing(2);

    // ── Tabellendesign ────────────────────────────────────────────────────────
    auto secDesign = makeSection("Tabellendesign", true, m_tableGroup);
    auto* formD    = makeForm(secDesign.content);

    m_tBorderColorBtn = new QPushButton(secDesign.content);
    m_tBorderColorBtn->setFixedHeight(24);
    formD->addRow("Rahmenfarbe:", m_tBorderColorBtn);

    m_tBorderWidth = makeSpin(0, 20, 0.5);
    m_tBorderWidth->setDecimals(1);
    formD->addRow("Rahmenbreite:", m_tBorderWidth);

    m_tFontSize = new QSpinBox(secDesign.content);
    m_tFontSize->setRange(6, 120);
    m_tFontSize->setValue(20);
    formD->addRow("Schriftgröße:", m_tFontSize);

    m_tDefaultBgBtn = new QPushButton(secDesign.content);
    m_tDefaultBgBtn->setFixedHeight(24);
    formD->addRow("Zell-Hintergrund:", m_tDefaultBgBtn);

    m_tDefaultTextBtn = new QPushButton(secDesign.content);
    m_tDefaultTextBtn->setFixedHeight(24);
    formD->addRow("Zell-Textfarbe:", m_tDefaultTextBtn);

    // Header row
    m_tHasHeader = new QCheckBox("Kopfzeile", secDesign.content);
    formD->addRow(m_tHasHeader);

    m_tHeaderBgBtn = new QPushButton(secDesign.content);
    m_tHeaderBgBtn->setFixedHeight(24);
    formD->addRow("Kopf-Hintergrund:", m_tHeaderBgBtn);

    m_tHeaderTextBtn = new QPushButton(secDesign.content);
    m_tHeaderTextBtn->setFixedHeight(24);
    formD->addRow("Kopf-Textfarbe:", m_tHeaderTextBtn);

    gvbox->addWidget(secDesign.outer);

    // ── Zeilen / Spalten ──────────────────────────────────────────────────────
    auto secRC = makeSection("Zeilen / Spalten", true, m_tableGroup);
    auto* rcVbox = new QVBoxLayout(secRC.content);
    rcVbox->setContentsMargins(4, 2, 4, 4);
    rcVbox->setSpacing(4);

    auto* rowBtnRow = new QHBoxLayout;
    auto* addRowBtn = new QPushButton("+ Zeile", secRC.content);
    auto* delRowBtn = new QPushButton("- Zeile", secRC.content);
    auto* addColBtn = new QPushButton("+ Spalte", secRC.content);
    auto* delColBtn = new QPushButton("- Spalte", secRC.content);
    rowBtnRow->addWidget(addRowBtn);
    rowBtnRow->addWidget(delRowBtn);
    rowBtnRow->addWidget(addColBtn);
    rowBtnRow->addWidget(delColBtn);
    rcVbox->addLayout(rowBtnRow);

    gvbox->addWidget(secRC.outer);

    // ── Ausgewählte Zelle ─────────────────────────────────────────────────────
    auto secCell = makeSection("Ausgewählte Zelle", true, m_tableGroup);
    auto* formC  = makeForm(secCell.content);

    m_cellBgBtn = new QPushButton(secCell.content);
    m_cellBgBtn->setFixedHeight(24);
    formC->addRow("Hintergrund:", m_cellBgBtn);

    m_cellTextBtn = new QPushButton(secCell.content);
    m_cellTextBtn->setFixedHeight(24);
    formC->addRow("Textfarbe:", m_cellTextBtn);

    m_cellBoldChk   = new QCheckBox("Fett",         secCell.content);
    m_cellItalicChk = new QCheckBox("Kursiv",       secCell.content);
    auto* styleRow  = new QHBoxLayout;
    styleRow->addWidget(m_cellBoldChk);
    styleRow->addWidget(m_cellItalicChk);
    formC->addRow(styleRow);

    m_cellAlignCombo = new QComboBox(secCell.content);
    m_cellAlignCombo->addItems({"Links", "Zentriert", "Rechts"});
    formC->addRow("Ausrichtung:", m_cellAlignCombo);

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

void PropertiesPanel::setSelectedTableCell(int row, int col) {
    m_cellRow = row;
    m_cellCol = col;
    bool valid = row >= 0 && col >= 0;
    m_cellSection->setVisible(valid);
    if (valid) refreshTableCell();
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

    m_updating = true;

    static const char* typeNames[] = {"Text", "Form", "Bild", "Tabelle"};
    m_elemType->setText(typeNames[e.type]);

    m_elemContent->setEnabled(e.type != SlideElement::Shape);
    m_elemContent->setText(e.content);

    m_eX->setValue(e.x);
    m_eY->setValue(e.y);
    m_eW->setValue(e.width);
    m_eH->setValue(e.height);

    updateColorButton(m_eColorBtn,   e.color);
    updateColorButton(m_eBgColorBtn, e.backgroundColor == Qt::transparent
                                         ? Qt::white : e.backgroundColor);

    m_eFontSize->setEnabled(e.type == SlideElement::Text);
    m_eFontSize->setValue(e.fontSize);

    m_eAlign->setEnabled(e.type == SlideElement::Text);
    int alignIdx = (e.textAlignment == "center") ? 1
                 : (e.textAlignment == "right")  ? 2 : 0;
    m_eAlign->setCurrentIndex(alignIdx);

    bool isShape = (e.type == SlideElement::Shape);
    m_elemFormSection->setVisible(isShape);
    if (isShape) {
        m_eBorderW->setValue(e.borderWidth);
        updateColorButton(m_eBorderColorBtn, e.borderColor.isValid()
                          ? e.borderColor : Qt::darkGray);
        m_eCornerRadius->setValue(e.cornerRadius);
    }

    int animIdx = 0;
    for (int i = 0; i < m_eAnimType->count(); ++i) {
        if (m_eAnimType->itemData(i).toString() == e.entranceAnim) { animIdx = i; break; }
    }
    m_eAnimType->setCurrentIndex(animIdx);
    m_eAnimDelay->setValue(e.animDelay);
    m_eAnimDuration->setValue(e.animDuration);
    bool hasAnim = !e.entranceAnim.isEmpty();
    m_animDelayLabel->setEnabled(hasAnim);
    m_eAnimDelay->setEnabled(hasAnim);
    m_animDurLabel->setEnabled(hasAnim);
    m_eAnimDuration->setEnabled(hasAnim);

    m_updating = false;
}

void PropertiesPanel::updateColorButton(QPushButton* btn, const QColor& c) {
    if (!c.isValid() || c == Qt::transparent) {
        btn->setStyleSheet("background: transparent; border: 1px dashed #888;");
        btn->setText("Transparent");
    } else {
        QString hex = c.name(QColor::HexRgb);
        QString fg  = c.lightnessF() > 0.5f ? "#000" : "#fff";
        btn->setStyleSheet(QString("background:%1; color:%2; border:1px solid #666;").arg(hex,fg));
        btn->setText(hex);
    }
    btn->setProperty("color", c);
}

// ── Project slots ─────────────────────────────────────────────────────────────

void PropertiesPanel::onSceneBgClicked() {
    if (!m_pres) return;
    QColor c = QColorDialog::getColor(m_pres->sceneBackground, this,
                                      "Projektshintergrund");
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
    QColor c = QColorDialog::getColor(init, this, "Folienhintergrund (Alpha=0 → transparent)",
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
        emit elementModified();
    }
}

void PropertiesPanel::onElemColorClicked() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e) return;
    QColor c = QColorDialog::getColor(e->color, this, "Textfarbe");
    if (c.isValid()) {
        e->color = c;
        updateColorButton(m_eColorBtn, c);
        emit elementModified();
    }
}

void PropertiesPanel::onElemBgColorClicked() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e) return;
    QColor c = QColorDialog::getColor(
        e->backgroundColor == Qt::transparent ? Qt::white : e->backgroundColor,
        this, "Hintergrundfarbe");
    if (c.isValid()) {
        e->backgroundColor = c;
        updateColorButton(m_eBgColorBtn, c);
        emit elementModified();
    }
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
    if (auto* e = getElem(m_pres, m_slideIdx, m_elemIdx)) {
        e->borderWidth = float(m_eBorderW->value());
        emit elementModified();
    }
}

void PropertiesPanel::onElemBorderColorClicked() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e) return;
    QColor init = e->borderColor.isValid() ? e->borderColor : Qt::darkGray;
    QColor c = QColorDialog::getColor(init, this, "Randfarbe");
    if (c.isValid()) {
        e->borderColor = c;
        updateColorButton(m_eBorderColorBtn, c);
        emit elementModified();
    }
}

void PropertiesPanel::onElemCornerRadiusChanged() {
    if (m_updating) return;
    if (auto* e = getElem(m_pres, m_slideIdx, m_elemIdx)) {
        e->cornerRadius = float(m_eCornerRadius->value());
        emit elementModified();
    }
}

void PropertiesPanel::onElemAnimChanged(int idx) {
    if (m_updating) return;
    if (auto* e = getElem(m_pres, m_slideIdx, m_elemIdx)) {
        e->entranceAnim = m_eAnimType->itemData(idx).toString();
        bool hasAnim = !e->entranceAnim.isEmpty();
        m_animDelayLabel->setEnabled(hasAnim);
        m_eAnimDelay->setEnabled(hasAnim);
        m_animDurLabel->setEnabled(hasAnim);
        m_eAnimDuration->setEnabled(hasAnim);
        emit elementModified();
    }
}

void PropertiesPanel::onElemAnimDelayChanged(double v) {
    if (m_updating) return;
    if (auto* e = getElem(m_pres, m_slideIdx, m_elemIdx)) {
        e->animDelay = float(v);
        emit elementModified();
    }
}

void PropertiesPanel::onElemAnimDurationChanged(double v) {
    if (m_updating) return;
    if (auto* e = getElem(m_pres, m_slideIdx, m_elemIdx)) {
        e->animDuration = float(v);
        emit elementModified();
    }
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
    QColor c = QColorDialog::getColor(e->tableBorderColor, this, "Rahmenfarbe");
    if (c.isValid()) { e->tableBorderColor = c; updateColorButton(m_tBorderColorBtn, c); emit elementModified(); }
}
void PropertiesPanel::onTableHeaderBgClicked() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e) return;
    QColor c = QColorDialog::getColor(e->tableHeaderBg, this, "Kopfzeilen-Hintergrund");
    if (c.isValid()) { e->tableHeaderBg = c; updateColorButton(m_tHeaderBgBtn, c); emit elementModified(); }
}
void PropertiesPanel::onTableHeaderTextClicked() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e) return;
    QColor c = QColorDialog::getColor(e->tableHeaderText, this, "Kopfzeilen-Textfarbe");
    if (c.isValid()) { e->tableHeaderText = c; updateColorButton(m_tHeaderTextBtn, c); emit elementModified(); }
}
void PropertiesPanel::onTableDefaultBgClicked() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e) return;
    QColor c = QColorDialog::getColor(e->tableDefaultBg, this, "Zellen-Standardhintergrund");
    if (c.isValid()) { e->tableDefaultBg = c; updateColorButton(m_tDefaultBgBtn, c); emit elementModified(); }
}
void PropertiesPanel::onTableDefaultTextClicked() {
    auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    if (!e) return;
    QColor c = QColorDialog::getColor(e->tableDefaultText, this, "Zellen-Standardtextfarbe");
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
    QColor c = QColorDialog::getColor(init, this, "Zellenhintergrund");
    if (c.isValid()) { cell->bgColor = c; updateColorButton(m_cellBgBtn, c); emit elementModified(); }
}
void PropertiesPanel::onCellTextColorClicked() {
    auto* cell = getCell(m_pres, m_slideIdx, m_elemIdx, m_cellRow, m_cellCol);
    if (!cell) return;
    const auto* e = getElem(m_pres, m_slideIdx, m_elemIdx);
    QColor init = cell->textColor.isValid() ? cell->textColor : (e ? e->tableDefaultText : Qt::black);
    QColor c = QColorDialog::getColor(init, this, "Zellenfarbe");
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
    m_chartGroup = new QGroupBox("Diagramm", this);
    auto* vbox = new QVBoxLayout(m_chartGroup);
    vbox->setContentsMargins(8, 8, 8, 8);
    vbox->setSpacing(6);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(4);

    m_chartTypeLabel  = new QLabel(this);
    m_chartTitleLabel = new QLabel(this);
    m_chartTitleLabel->setWordWrap(true);
    form->addRow("Typ:", m_chartTypeLabel);
    form->addRow("Titel:", m_chartTitleLabel);
    vbox->addLayout(form);

    m_chartEditBtn = new QPushButton("Diagramm bearbeiten...", this);
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
                               ? "(kein Titel)" : e->chartData.title);
}
