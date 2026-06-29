#include "FormatBar.h"
#include <QHBoxLayout>
#include <QFontComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QFrame>
#include <QColorDialog>

static const QString ALIGN_BTN =
    "QPushButton { border:1px solid #d1d5db; padding:2px 5px; min-width:26px; font-size:12px; background:#ffffff; color:#111827; }"
    "QPushButton:hover   { background:#f3f4f6; border-color:#9ca3af; }"
    "QPushButton:checked { background:#eff6ff; color:#2563eb; border-color:#2563eb; }"
    "QPushButton:disabled { color:#9ca3af; }";

static QFrame* makeSep(QWidget* parent) {
    auto* sep = new QFrame(parent);
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedWidth(1);
    sep->setStyleSheet("color:#e5e7eb;");
    return sep;
}

static QLabel* lbl(const QString& t, QWidget* p) {
    auto* l = new QLabel(t, p);
    l->setStyleSheet("color:#9ca3af; font-size:10px;");
    return l;
}

FormatBar::FormatBar(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(4, 2, 4, 2);
    row->setSpacing(4);

    // ── Text formatting group ──────────────────────────────────────────────
    m_textGroup = new QWidget(this);
    auto* tr = new QHBoxLayout(m_textGroup);
    tr->setContentsMargins(0, 0, 0, 0);
    tr->setSpacing(4);

    m_fontCombo = new QFontComboBox(m_textGroup);
    m_fontCombo->setFixedWidth(180);
    m_fontCombo->setFocusPolicy(Qt::StrongFocus);
    tr->addWidget(m_fontCombo);

    tr->addWidget(lbl("Gr:", m_textGroup));
    m_fontSize = new QSpinBox(m_textGroup);
    m_fontSize->setRange(6, 400);
    m_fontSize->setValue(32);
    m_fontSize->setFixedWidth(58);
    m_fontSize->setSuffix("pt");
    m_fontSize->setToolTip("Schriftgröße – Mausrad = live Größe ändern");
    tr->addWidget(m_fontSize);

    tr->addWidget(makeSep(m_textGroup));

    // Color buttons — their style is set dynamically by updateColorSwatch
    m_colorBtn = new QPushButton("A", m_textGroup);
    m_colorBtn->setToolTip("Textfarbe");
    m_colorBtn->setFixedWidth(32);
    tr->addWidget(m_colorBtn);

    m_bgColorBtn = new QPushButton("■", m_textGroup);
    m_bgColorBtn->setToolTip("Hintergrundfarbe");
    m_bgColorBtn->setFixedWidth(32);
    tr->addWidget(m_bgColorBtn);

    tr->addWidget(makeSep(m_textGroup));

    m_alignLeft   = new QPushButton("≡L", m_textGroup);
    m_alignCenter = new QPushButton("≡≡", m_textGroup);
    m_alignRight  = new QPushButton("R≡", m_textGroup);
    for (auto* b : {m_alignLeft, m_alignCenter, m_alignRight}) {
        b->setCheckable(true);
        b->setFixedWidth(30);
        b->setStyleSheet(ALIGN_BTN);
        tr->addWidget(b);
    }
    m_alignLeft->setToolTip("Linksbündig");
    m_alignCenter->setToolTip("Zentriert");
    m_alignRight->setToolTip("Rechtsbündig");

    m_vAlignTop    = new QPushButton("┬", m_textGroup); // ┬
    m_vAlignMiddle = new QPushButton("┼", m_textGroup); // ┼
    m_vAlignBottom = new QPushButton("┴", m_textGroup); // ┴
    for (auto* b : {m_vAlignTop, m_vAlignMiddle, m_vAlignBottom}) {
        b->setCheckable(true);
        b->setFixedWidth(30);
        b->setStyleSheet(ALIGN_BTN);
        tr->addWidget(b);
    }
    m_vAlignTop->setToolTip("Vertikal oben");
    m_vAlignMiddle->setToolTip("Vertikal zentriert");
    m_vAlignBottom->setToolTip("Vertikal unten");

    tr->addWidget(makeSep(m_textGroup));

    m_listBullet   = new QPushButton("•", m_textGroup);
    m_listNumbered = new QPushButton("1.",     m_textGroup);
    for (auto* b : {m_listBullet, m_listNumbered}) {
        b->setCheckable(true);
        b->setFixedWidth(30);
        b->setStyleSheet(ALIGN_BTN);
        tr->addWidget(b);
    }
    m_listBullet->setToolTip("Aufzählung (•)");
    m_listNumbered->setToolTip("Nummerierte Liste (1. 2. 3.)");

    tr->addWidget(makeSep(m_textGroup));

    // B / I / U / S̶
    m_boldBtn      = new QPushButton("B",  m_textGroup);
    m_italicBtn    = new QPushButton("I",  m_textGroup);
    m_underlineBtn = new QPushButton("U",  m_textGroup);
    m_strikeBtn    = new QPushButton("S̶", m_textGroup);
    for (auto* b : {m_boldBtn, m_italicBtn, m_underlineBtn, m_strikeBtn}) {
        b->setCheckable(true);
        b->setFixedWidth(28);
        b->setStyleSheet(ALIGN_BTN);
        tr->addWidget(b);
    }
    m_boldBtn->setStyleSheet(ALIGN_BTN + "QPushButton { font-weight:bold; }");
    m_italicBtn->setStyleSheet(ALIGN_BTN + "QPushButton { font-style:italic; }");
    m_boldBtn->setToolTip("Fett (B)");
    m_italicBtn->setToolTip("Kursiv (I)");
    m_underlineBtn->setToolTip("Unterstrichen (U)");
    m_strikeBtn->setToolTip("Durchgestrichen");

    tr->addWidget(makeSep(m_textGroup));

    // Underline color + style (only meaningful when underline is on)
    m_ulColorBtn = new QPushButton("U", m_textGroup);
    m_ulColorBtn->setToolTip("Unterstreichungsfarbe wählen");
    m_ulColorBtn->setFixedWidth(28);
    tr->addWidget(m_ulColorBtn);

    m_ulStyleCombo = new QComboBox(m_textGroup);
    m_ulStyleCombo->addItem("─── Durchgezogen");
    m_ulStyleCombo->addItem("─ ─ Gestrichelt");
    m_ulStyleCombo->addItem("··· Gepunktet");
    m_ulStyleCombo->addItem("~~~ Wellig");
    m_ulStyleCombo->setFixedWidth(130);
    m_ulStyleCombo->setToolTip("Stil der Unterstreichung");
    tr->addWidget(m_ulStyleCombo);

    tr->addWidget(makeSep(m_textGroup));

    m_fmtPainterBtn = new QPushButton("◈ Format", m_textGroup);
    m_fmtPainterBtn->setToolTip("Format auf anderes Element übertragen");
    m_fmtPainterBtn->setFixedWidth(80);
    m_fmtPainterBtn->setStyleSheet(ALIGN_BTN);
    tr->addWidget(m_fmtPainterBtn);

    row->addWidget(m_textGroup);
    row->addWidget(makeSep(this));

    // ── Geometry group ─────────────────────────────────────────────────────
    m_geomGroup = new QWidget(this);
    auto* gr = new QHBoxLayout(m_geomGroup);
    gr->setContentsMargins(0, 0, 0, 0);
    gr->setSpacing(4);

    auto mkDSpin = [&](const QString& label) -> QDoubleSpinBox* {
        gr->addWidget(lbl(label, m_geomGroup));
        auto* sb = new QDoubleSpinBox(m_geomGroup);
        sb->setRange(-99999, 99999);
        sb->setDecimals(0);
        sb->setFixedWidth(70);
        gr->addWidget(sb);
        return sb;
    };
    m_posX  = mkDSpin("X:");  m_posX->setToolTip("Position X");
    m_posY  = mkDSpin("Y:");  m_posY->setToolTip("Position Y");
    m_sizeW = mkDSpin("B:");  m_sizeW->setToolTip("Breite (Mausrad = live)");
    m_sizeH = mkDSpin("H:");  m_sizeH->setToolTip("Höhe (Mausrad = live)");

    row->addWidget(m_geomGroup);
    row->addStretch();

    // ── Signal connections ─────────────────────────────────────────────────
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, &FormatBar::onFontChanged);
    connect(m_fontSize,  qOverload<int>(&QSpinBox::valueChanged), this, &FormatBar::onFontSizeChanged);
    connect(m_colorBtn,   &QPushButton::clicked, this, &FormatBar::onColorClicked);
    connect(m_bgColorBtn, &QPushButton::clicked, this, &FormatBar::onBgColorClicked);
    connect(m_alignLeft,   &QPushButton::clicked, this, &FormatBar::onAlignLeft);
    connect(m_alignCenter, &QPushButton::clicked, this, &FormatBar::onAlignCenter);
    connect(m_alignRight,  &QPushButton::clicked, this, &FormatBar::onAlignRight);
    connect(m_vAlignTop,    &QPushButton::clicked, this, &FormatBar::onVAlignTop);
    connect(m_vAlignMiddle, &QPushButton::clicked, this, &FormatBar::onVAlignMiddle);
    connect(m_vAlignBottom, &QPushButton::clicked, this, &FormatBar::onVAlignBottom);
    connect(m_listBullet,   &QPushButton::clicked, this, &FormatBar::onListBullet);
    connect(m_listNumbered, &QPushButton::clicked, this, &FormatBar::onListNumbered);
    connect(m_boldBtn,      &QPushButton::clicked, this, &FormatBar::onBold);
    connect(m_italicBtn,    &QPushButton::clicked, this, &FormatBar::onItalic);
    connect(m_underlineBtn, &QPushButton::clicked, this, &FormatBar::onUnderline);
    connect(m_strikeBtn,    &QPushButton::clicked, this, &FormatBar::onStrikethrough);
    connect(m_ulColorBtn,   &QPushButton::clicked, this, &FormatBar::onUnderlineColorClicked);
    connect(m_ulStyleCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &FormatBar::onUnderlineStyleChanged);
    connect(m_fmtPainterBtn, &QPushButton::clicked, this, &FormatBar::formatPainterRequested);
    connect(m_posX,  qOverload<double>(&QDoubleSpinBox::valueChanged), this, &FormatBar::onXChanged);
    connect(m_posY,  qOverload<double>(&QDoubleSpinBox::valueChanged), this, &FormatBar::onYChanged);
    connect(m_sizeW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &FormatBar::onWChanged);
    connect(m_sizeH, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &FormatBar::onHChanged);

    setContext(nullptr, -1, -1);
}

void FormatBar::setContext(Presentation* pres, int slideIdx, int elemIdx) {
    m_pres = pres; m_slideIdx = slideIdx; m_elemIdx = elemIdx;
    refresh();
}

SlideElement* FormatBar::currentElem() {
    if (!m_pres || m_slideIdx < 0 || m_elemIdx < 0) return nullptr;
    if (m_slideIdx >= m_pres->slides.size()) return nullptr;
    auto& elems = m_pres->slides[m_slideIdx].elements;
    if (m_elemIdx >= elems.size()) return nullptr;
    return &elems[m_elemIdx];
}

void FormatBar::refresh() {
    m_updating = true;
    SlideElement* e = currentElem();
    bool hasElem = (e != nullptr);
    bool isText  = hasElem && e->type == SlideElement::Text;

    m_textGroup->setEnabled(isText);
    m_geomGroup->setEnabled(hasElem);

    if (isText) {
        m_fontCombo->setCurrentFont(QFont(e->fontFamily));
        m_fontSize->setValue(e->fontSize);
        updateColorSwatch(m_colorBtn,   e->color);
        updateColorSwatch(m_bgColorBtn, e->backgroundColor);
        m_alignLeft->setChecked(e->textAlignment.isEmpty() || e->textAlignment == "left");
        m_alignCenter->setChecked(e->textAlignment == "center");
        m_alignRight->setChecked(e->textAlignment == "right");
        m_vAlignTop->setChecked(e->verticalAlignment.isEmpty() || e->verticalAlignment == "top");
        m_vAlignMiddle->setChecked(e->verticalAlignment == "middle");
        m_vAlignBottom->setChecked(e->verticalAlignment == "bottom");
        m_listBullet->setChecked(e->listStyle == SlideElement::Bullets);
        m_listNumbered->setChecked(e->listStyle == SlideElement::Numbered);
        m_boldBtn->setChecked(e->bold);
        m_italicBtn->setChecked(e->italic);
        m_underlineBtn->setChecked(e->underline);
        m_strikeBtn->setChecked(e->strikethrough);
        updateColorSwatch(m_ulColorBtn,
            e->underlineColor.isValid() ? e->underlineColor : e->color);
        m_ulStyleCombo->setCurrentIndex(qBound(0, e->underlineStyle, 3));
        m_ulStyleCombo->setEnabled(e->underline);
        m_ulColorBtn->setEnabled(e->underline);
        m_fmtPainterBtn->setEnabled(true);
    } else {
        updateColorSwatch(m_colorBtn,   Qt::black);
        updateColorSwatch(m_bgColorBtn, Qt::transparent);
        m_alignLeft->setChecked(false);
        m_alignCenter->setChecked(false);
        m_alignRight->setChecked(false);
        m_vAlignTop->setChecked(false);
        m_vAlignMiddle->setChecked(false);
        m_vAlignBottom->setChecked(false);
        m_listBullet->setChecked(false);
        m_listNumbered->setChecked(false);
        m_boldBtn->setChecked(false);
        m_italicBtn->setChecked(false);
        m_underlineBtn->setChecked(false);
        m_strikeBtn->setChecked(false);
        m_ulStyleCombo->setEnabled(false);
        m_ulColorBtn->setEnabled(false);
        m_fmtPainterBtn->setEnabled(hasElem);
    }

    if (hasElem) {
        m_posX->setValue(e->x);  m_posY->setValue(e->y);
        m_sizeW->setValue(e->width); m_sizeH->setValue(e->height);
    }

    m_updating = false;
}

void FormatBar::updateColorSwatch(QPushButton* btn, const QColor& c) {
    QColor col = (c.isValid() && c != Qt::transparent) ? c : QColor(248, 248, 248);
    QString hex = QString("#%1%2%3")
        .arg(col.red(),   2, 16, QChar('0'))
        .arg(col.green(), 2, 16, QChar('0'))
        .arg(col.blue(),  2, 16, QChar('0'));
    QString fg = col.lightnessF() > 0.5f ? "#000" : "#fff";
    btn->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:1px solid #d1d5db; "
                "padding:2px 5px; min-width:28px; font-weight:bold; border-radius:4px; }"
                "QPushButton:hover { border:1px solid #9ca3af; }"
                "QPushButton:disabled { opacity:0.4; }").arg(hex, fg));
}

void FormatBar::onFontChanged(const QFont& f) {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->fontFamily = f.family(); emit modified();
}
void FormatBar::onFontSizeChanged(int v) {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->fontSize = v; emit modified();
}
void FormatBar::onColorClicked() {
    auto* e = currentElem(); if (!e) return;
    QColor c = QColorDialog::getColor(e->color, this, "Textfarbe");
    if (!c.isValid()) return;
    e->color = c; updateColorSwatch(m_colorBtn, c); emit modified();
}
void FormatBar::onBgColorClicked() {
    auto* e = currentElem(); if (!e) return;
    QColor init = e->backgroundColor.isValid() ? e->backgroundColor : Qt::white;
    QColor c = QColorDialog::getColor(init, this, "Hintergrundfarbe",
                                      QColorDialog::ShowAlphaChannel);
    if (!c.isValid()) return;
    e->backgroundColor = c; updateColorSwatch(m_bgColorBtn, c); emit modified();
}
void FormatBar::onAlignLeft() {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->textAlignment = "left";
    m_updating = true;
    m_alignLeft->setChecked(true); m_alignCenter->setChecked(false); m_alignRight->setChecked(false);
    m_updating = false; emit modified();
}
void FormatBar::onAlignCenter() {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->textAlignment = "center";
    m_updating = true;
    m_alignLeft->setChecked(false); m_alignCenter->setChecked(true); m_alignRight->setChecked(false);
    m_updating = false; emit modified();
}
void FormatBar::onAlignRight() {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->textAlignment = "right";
    m_updating = true;
    m_alignLeft->setChecked(false); m_alignCenter->setChecked(false); m_alignRight->setChecked(true);
    m_updating = false; emit modified();
}
void FormatBar::onXChanged(double v) { if (!m_updating) { auto* e=currentElem(); if(e){e->x=float(v); emit modified();} } }
void FormatBar::onYChanged(double v) { if (!m_updating) { auto* e=currentElem(); if(e){e->y=float(v); emit modified();} } }
void FormatBar::onWChanged(double v) { if (!m_updating) { auto* e=currentElem(); if(e){e->width=float(v); emit modified();} } }
void FormatBar::onHChanged(double v) { if (!m_updating) { auto* e=currentElem(); if(e){e->height=float(v); emit modified();} } }
void FormatBar::onListBullet() {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->listStyle = (e->listStyle == SlideElement::Bullets) ? SlideElement::NoList : SlideElement::Bullets;
    m_updating = true;
    m_listBullet->setChecked(e->listStyle == SlideElement::Bullets);
    m_listNumbered->setChecked(false);
    m_updating = false;
    emit modified();
}
void FormatBar::onListNumbered() {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->listStyle = (e->listStyle == SlideElement::Numbered) ? SlideElement::NoList : SlideElement::Numbered;
    m_updating = true;
    m_listNumbered->setChecked(e->listStyle == SlideElement::Numbered);
    m_listBullet->setChecked(false);
    m_updating = false;
    emit modified();
}
void FormatBar::onBold() {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->bold = m_boldBtn->isChecked(); emit modified();
}
void FormatBar::onItalic() {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->italic = m_italicBtn->isChecked(); emit modified();
}
void FormatBar::onUnderline() {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->underline = m_underlineBtn->isChecked();
    m_ulStyleCombo->setEnabled(e->underline);
    m_ulColorBtn->setEnabled(e->underline);
    emit modified();
}
void FormatBar::onStrikethrough() {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->strikethrough = m_strikeBtn->isChecked(); emit modified();
}
void FormatBar::onUnderlineColorClicked() {
    auto* e = currentElem(); if (!e) return;
    QColor init = e->underlineColor.isValid() ? e->underlineColor : e->color;
    QColor c = QColorDialog::getColor(init, this, "Unterstreichungsfarbe");
    if (!c.isValid()) return;
    e->underlineColor = c;
    updateColorSwatch(m_ulColorBtn, c);
    emit modified();
}
void FormatBar::onUnderlineStyleChanged(int idx) {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->underlineStyle = idx; emit modified();
}
void FormatBar::onVAlignTop() {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->verticalAlignment = "top";
    m_updating = true;
    m_vAlignTop->setChecked(true); m_vAlignMiddle->setChecked(false); m_vAlignBottom->setChecked(false);
    m_updating = false; emit modified();
}
void FormatBar::onVAlignMiddle() {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->verticalAlignment = "middle";
    m_updating = true;
    m_vAlignTop->setChecked(false); m_vAlignMiddle->setChecked(true); m_vAlignBottom->setChecked(false);
    m_updating = false; emit modified();
}
void FormatBar::onVAlignBottom() {
    if (m_updating) return;
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->verticalAlignment = "bottom";
    m_updating = true;
    m_vAlignTop->setChecked(false); m_vAlignMiddle->setChecked(false); m_vAlignBottom->setChecked(true);
    m_updating = false; emit modified();
}
