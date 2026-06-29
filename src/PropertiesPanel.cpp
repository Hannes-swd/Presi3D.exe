#include "PropertiesPanel.h"
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

// ── Helper ────────────────────────────────────────────────────────────────────

static QDoubleSpinBox* makeSpin(double min, double max, double step = 1.0) {
    auto* s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setSingleStep(step);
    s->setDecimals(1);
    return s;
}

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
    vbox->addWidget(m_projectGroup);
    vbox->addWidget(m_slideGroup);
    vbox->addWidget(m_elemGroup);
    vbox->addStretch();

    scroll->setWidget(inner);
    outerLayout->addWidget(scroll);
}

void PropertiesPanel::buildProjectGroup() {
    m_projectGroup = new QGroupBox("Präsentation", this);
    auto* form = new QFormLayout(m_projectGroup);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_sceneBgBtn = new QPushButton(m_projectGroup);
    m_sceneBgBtn->setFixedHeight(24);
    m_sceneBgBtn->setToolTip("Hintergrundfarbe hinter allen Folien");
    form->addRow("Hintergrund:", m_sceneBgBtn);

    auto* sizeRow = new QHBoxLayout;
    m_slideW = makeSpin(100, 9999, 10);  m_slideW->setValue(1920);
    m_slideH = makeSpin(100, 9999, 10);  m_slideH->setValue(1080);
    sizeRow->addWidget(m_slideW);
    sizeRow->addWidget(new QLabel("×"));
    sizeRow->addWidget(m_slideH);
    form->addRow("Foliengröße:", sizeRow);

    auto* sep = new QLabel("─── Standard Sichtbarkeit ───", m_projectGroup);
    sep->setStyleSheet("color: #374151; font-size: 10px;");
    form->addRow(sep);

    m_defaultInactiveOpa = makeSpin(0.0, 1.0, 0.05);
    m_defaultInactiveOpa->setDecimals(2);
    m_defaultInactiveOpa->setValue(0.3);
    m_defaultInactiveOpa->setToolTip("Standard-Deckkraft für Folien ohne eigene Einstellung\n"
                                     "(gilt wenn keine per-Folie-Überschreibung gesetzt ist)");
    form->addRow("Inaktiv Standard:", m_defaultInactiveOpa);

    connect(m_sceneBgBtn, &QPushButton::clicked, this, &PropertiesPanel::onSceneBgClicked);
    connect(m_slideW, &QDoubleSpinBox::valueChanged, this, [this](double) { onSlideSizeChanged(); });
    connect(m_slideH, &QDoubleSpinBox::valueChanged, this, [this](double) { onSlideSizeChanged(); });
    connect(m_defaultInactiveOpa, &QDoubleSpinBox::valueChanged,
            this, [this](double) { onDefaultInactiveOpaChanged(); });
}

void PropertiesPanel::buildSlideGroup() {
    m_slideGroup = new QGroupBox("Slide", this);
    auto* form   = new QFormLayout(m_slideGroup);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_slideName = new QLineEdit(m_slideGroup);
    form->addRow("Name:", m_slideName);

    m_bgColorBtn = new QPushButton(m_slideGroup);
    m_bgColorBtn->setFixedHeight(24);
    form->addRow("Hintergrund:", m_bgColorBtn);

    auto* sep = new QLabel("─── 3D Position ───", m_slideGroup);
    sep->setStyleSheet("color: #374151; font-size: 10px;");
    form->addRow(sep);

    m_posX = makeSpin(-99999, 99999, 100);
    m_posY = makeSpin(-99999, 99999, 100);
    m_posZ = makeSpin(-99999, 99999, 100);
    form->addRow("X:", m_posX);
    form->addRow("Y:", m_posY);
    form->addRow("Z:", m_posZ);

    auto* sep2 = new QLabel("─── Rotation (°) ───", m_slideGroup);
    sep2->setStyleSheet("color: #374151; font-size: 10px;");
    form->addRow(sep2);

    m_rotX = makeSpin(-360, 360);
    m_rotY = makeSpin(-360, 360);
    m_rotZ = makeSpin(-360, 360);
    form->addRow("Rot X:", m_rotX);
    form->addRow("Rot Y:", m_rotY);
    form->addRow("Rot Z:", m_rotZ);

    m_scale = makeSpin(0.01, 10.0, 0.1);
    m_scale->setToolTip("Zoom der Kamera auf dieser Folie:\n"
                        "1.0 = Folie füllt die Ansicht\n"
                        "< 1 = reinzoomen (weniger Kontext)\n"
                        "> 1 = rauszoomen (mehr Kontext)");
    form->addRow("Zoom:", m_scale);

    auto* sepView = new QLabel("─── Sichtfeld (nur 3D) ───", m_slideGroup);
    sepView->setStyleSheet("color: #374151; font-size: 10px;");
    form->addRow(sepView);

    auto* viewOffRow = new QHBoxLayout;
    m_viewOffX = makeSpin(-9999, 9999, 50);
    m_viewOffX->setToolTip("Sichtfeld-Mitte X-Verschiebung\n(0 = Folie zentriert)");
    m_viewOffY = makeSpin(-9999, 9999, 50);
    m_viewOffY->setToolTip("Sichtfeld-Mitte Y-Verschiebung\n(0 = Folie zentriert)");
    viewOffRow->addWidget(m_viewOffX);
    viewOffRow->addWidget(new QLabel("Y:"));
    viewOffRow->addWidget(m_viewOffY);
    form->addRow("Offset X:", viewOffRow);

    auto* sepSize = new QLabel("─── Eigene Foliengröße ───", m_slideGroup);
    sepSize->setStyleSheet("color: #374151; font-size: 10px;");
    form->addRow(sepSize);

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
    form->addRow("Größe:", ownSizeRow);

    auto* sizeHint = new QLabel("(0 = Standard)", m_slideGroup);
    sizeHint->setStyleSheet("color: #6b7280; font-size: 9px;");
    form->addRow(sizeHint);

    // ── Per-slide visibility section ──────────────────────────────────────────
    auto* sepVis = new QLabel("─── Sichtbarkeit anderer Folien ───", m_slideGroup);
    sepVis->setStyleSheet("color: #374151; font-size: 10px;");
    form->addRow(sepVis);

    auto* visHint = new QLabel("Wenn diese Folie aktiv ist:", m_slideGroup);
    visHint->setStyleSheet("color: #6b7280; font-size: 9px;");
    form->addRow(visHint);

    m_visContainer = new QWidget(m_slideGroup);
    m_visLayout    = new QVBoxLayout(m_visContainer);
    m_visLayout->setContentsMargins(0, 2, 0, 2);
    m_visLayout->setSpacing(2);
    form->addRow(m_visContainer);

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
    connect(m_scale,      &QDoubleSpinBox::valueChanged, this, [this](double) { onScaleChanged(); });
    connect(m_slideOwnW, &QDoubleSpinBox::valueChanged, this, [this](double) { onSlideOwnSizeChanged(); });
    connect(m_slideOwnH, &QDoubleSpinBox::valueChanged, this, [this](double) { onSlideOwnSizeChanged(); });
    connect(m_viewOffX,  &QDoubleSpinBox::valueChanged, this, [this](double) { onViewOffsetChanged(); });
    connect(m_viewOffY,  &QDoubleSpinBox::valueChanged, this, [this](double) { onViewOffsetChanged(); });
}

void PropertiesPanel::buildElementGroup() {
    m_elemGroup = new QGroupBox("Element", this);
    auto* form  = new QFormLayout(m_elemGroup);
    form->setLabelAlignment(Qt::AlignRight);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_elemType = new QLabel("—", m_elemGroup);
    m_elemType->setStyleSheet("color: #374151;");
    form->addRow("Typ:", m_elemType);

    m_elemContent = new QLineEdit(m_elemGroup);
    form->addRow("Inhalt:", m_elemContent);

    m_eX = makeSpin(-9999, 9999); m_eY = makeSpin(-9999, 9999);
    m_eW = makeSpin(1, 9999);     m_eH = makeSpin(1, 9999);
    form->addRow("X:", m_eX);
    form->addRow("Y:", m_eY);
    form->addRow("Breite:", m_eW);
    form->addRow("Höhe:",   m_eH);

    m_eColorBtn   = new QPushButton(m_elemGroup);  m_eColorBtn->setFixedHeight(24);
    m_eBgColorBtn = new QPushButton(m_elemGroup);  m_eBgColorBtn->setFixedHeight(24);
    form->addRow("Farbe:",       m_eColorBtn);
    form->addRow("Hintergrund:", m_eBgColorBtn);

    m_eFontSize = new QSpinBox(m_elemGroup);
    m_eFontSize->setRange(6, 200);
    m_eFontSize->setValue(32);
    form->addRow("Schriftgröße:", m_eFontSize);

    m_eAlign = new QComboBox(m_elemGroup);
    m_eAlign->addItems({"Links", "Zentriert", "Rechts"});
    form->addRow("Ausrichtung:", m_eAlign);

    // ── Shape-only controls ───────────────────────────────────────────────────
    m_borderWLabel = new QLabel("Randbreite px:", m_elemGroup);
    m_eBorderW = makeSpin(0, 200, 1);
    m_eBorderW->setDecimals(0);
    form->addRow(m_borderWLabel, m_eBorderW);

    m_borderColorLabel = new QLabel("Randfarbe:", m_elemGroup);
    m_eBorderColorBtn  = new QPushButton(m_elemGroup);
    m_eBorderColorBtn->setFixedHeight(24);
    form->addRow(m_borderColorLabel, m_eBorderColorBtn);

    m_cornerRadLabel = new QLabel("Eckenradius:", m_elemGroup);
    m_eCornerRadius  = makeSpin(0, 999, 5);
    m_eCornerRadius->setDecimals(0);
    m_eCornerRadius->setToolTip("Abrundung der Ecken in Folien-Pixeln (0 = keine Abrundung)");
    form->addRow(m_cornerRadLabel, m_eCornerRadius);

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

    // ── Entrance animation ────────────────────────────────────────────────────
    m_animSepLabel = new QLabel("─── Eingangs-Animation ───", m_elemGroup);
    m_animSepLabel->setStyleSheet("color: #374151; font-size: 10px;");
    form->addRow(m_animSepLabel);

    m_eAnimType = new QComboBox(m_elemGroup);
    m_eAnimType->addItem("Keine",          "");
    m_eAnimType->addItem("Einblenden",     "fadeIn");
    m_eAnimType->addItem("Von links",      "slideLeft");
    m_eAnimType->addItem("Von rechts",     "slideRight");
    m_eAnimType->addItem("Von oben",       "slideUp");
    m_eAnimType->addItem("Von unten",      "slideDown");
    m_eAnimType->addItem("Zoom",           "zoomIn");
    form->addRow("Animation:", m_eAnimType);

    m_animDelayLabel = new QLabel("Verzögerung (s):", m_elemGroup);
    m_eAnimDelay = makeSpin(0.0, 10.0, 0.1);
    m_eAnimDelay->setDecimals(1);
    m_eAnimDelay->setValue(0.3);
    form->addRow(m_animDelayLabel, m_eAnimDelay);

    m_animDurLabel = new QLabel("Dauer (s):", m_elemGroup);
    m_eAnimDuration = makeSpin(0.05, 10.0, 0.1);
    m_eAnimDuration->setDecimals(1);
    m_eAnimDuration->setValue(0.5);
    form->addRow(m_animDurLabel, m_eAnimDuration);

    connect(m_eAnimType,     &QComboBox::currentIndexChanged, this, &PropertiesPanel::onElemAnimChanged);
    connect(m_eAnimDelay,    &QDoubleSpinBox::valueChanged,   this, [this](double v){ onElemAnimDelayChanged(v); });
    connect(m_eAnimDuration, &QDoubleSpinBox::valueChanged,   this, [this](double v){ onElemAnimDurationChanged(v); });

    // Start hidden; shown when a Shape element is selected
    m_borderWLabel->hide();    m_eBorderW->hide();
    m_borderColorLabel->hide();m_eBorderColorBtn->hide();
    m_cornerRadLabel->hide();  m_eCornerRadius->hide();

    m_elemGroup->setEnabled(false);
}

// ── Public API ────────────────────────────────────────────────────────────────

void PropertiesPanel::setSlide(Presentation* pres, int slideIndex) {
    m_pres     = pres;
    m_slideIdx = slideIndex;
    m_elemIdx  = -1;
    m_elemGroup->setEnabled(false);
    rebuildVisibilitySection();
    refreshProject();
    refreshSlide();
}

void PropertiesPanel::setSelectedElement(int elemIndex) {
    m_elemIdx = elemIndex;
    m_elemGroup->setEnabled(elemIndex >= 0);
    refreshElement();
}

// ── Visibility section ────────────────────────────────────────────────────────

void PropertiesPanel::rebuildVisibilitySection() {
    // Clear old rows
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

        // Determine current opacity for this other slide
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

        // Capture index for the lambdas
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

    // Refresh visibility rows
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

    static const char* typeNames[] = {"Text", "Form", "Bild"};
    m_elemType->setText(typeNames[e.type]);

    m_elemContent->setEnabled(e.type != SlideElement::Shape);
    m_elemContent->setText(e.type == SlideElement::Shape ? e.content : e.content);

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
    m_borderWLabel->setVisible(isShape);
    m_eBorderW->setVisible(isShape);
    m_borderColorLabel->setVisible(isShape);
    m_eBorderColorBtn->setVisible(isShape);
    m_cornerRadLabel->setVisible(isShape);
    m_eCornerRadius->setVisible(isShape);
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
