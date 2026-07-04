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
#include <QInputDialog>
#include <QLineEdit>
#include <QIcon>

static const QString ALIGN_BTN =
    "QPushButton { border:1px solid #d1d5db; padding:2px 5px; min-width:26px; font-size:12px; background:#ffffff; color:#111827; }"
    "QPushButton:hover   { background:#f3f4f6; border-color:#9ca3af; }"
    "QPushButton:checked { background:#eff6ff; color:#2563eb; border-color:#2563eb; }"
    "QPushButton:disabled { color:#9ca3af; }"
    "QToolTip { background:#1f2937; color:#f9fafb; border:none; padding:4px 8px; "
    "           border-radius:4px; font-size:10px; }";

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

    tr->addWidget(lbl("Sz:", m_textGroup));
    m_fontSize = new QSpinBox(m_textGroup);
    m_fontSize->setRange(6, 400);
    m_fontSize->setValue(32);
    m_fontSize->setFixedWidth(58);
    m_fontSize->setSuffix("pt");
    m_fontSize->setToolTip("Font size – mouse wheel = change size live");
    tr->addWidget(m_fontSize);

    tr->addWidget(makeSep(m_textGroup));

    // Color buttons — their style is set dynamically by updateColorSwatch
    m_colorBtn = new QPushButton("A", m_textGroup);
    m_colorBtn->setToolTip("Text color");
    m_colorBtn->setFixedWidth(32);
    tr->addWidget(m_colorBtn);

    m_bgColorBtn = new QPushButton("Bg", m_textGroup);
    m_bgColorBtn->setToolTip("Background color");
    m_bgColorBtn->setFixedWidth(32);
    tr->addWidget(m_bgColorBtn);

    tr->addWidget(makeSep(m_textGroup));

    m_alignLeft   = new QPushButton(QIcon(":/icons/format_align_left.svg"),   "", m_textGroup);
    m_alignCenter = new QPushButton(QIcon(":/icons/format_align_center.svg"), "", m_textGroup);
    m_alignRight  = new QPushButton(QIcon(":/icons/format_align_right.svg"),  "", m_textGroup);
    for (auto* b : {m_alignLeft, m_alignCenter, m_alignRight}) {
        b->setCheckable(true);
        b->setIconSize(QSize(16, 16));
        b->setFixedWidth(30);
        b->setStyleSheet(ALIGN_BTN);
        tr->addWidget(b);
    }
    m_alignLeft->setToolTip("Align Left");
    m_alignCenter->setToolTip("Center");
    m_alignRight->setToolTip("Align Right");

    m_vAlignTop    = new QPushButton(QIcon(":/icons/vertical_align_top.svg"),    "", m_textGroup);
    m_vAlignMiddle = new QPushButton(QIcon(":/icons/vertical_align_center.svg"), "", m_textGroup);
    m_vAlignBottom = new QPushButton(QIcon(":/icons/vertical_align_bottom.svg"), "", m_textGroup);
    for (auto* b : {m_vAlignTop, m_vAlignMiddle, m_vAlignBottom}) {
        b->setCheckable(true);
        b->setIconSize(QSize(16, 16));
        b->setFixedWidth(30);
        b->setStyleSheet(ALIGN_BTN);
        tr->addWidget(b);
    }
    m_vAlignTop->setToolTip("Align Top");
    m_vAlignMiddle->setToolTip("Align Middle");
    m_vAlignBottom->setToolTip("Align Bottom");

    tr->addWidget(makeSep(m_textGroup));

    m_listBullet   = new QPushButton(QIcon(":/icons/format_list_bulleted.svg"), "", m_textGroup);
    m_listNumbered = new QPushButton(QIcon(":/icons/format_list_numbered.svg"), "", m_textGroup);
    for (auto* b : {m_listBullet, m_listNumbered}) {
        b->setCheckable(true);
        b->setIconSize(QSize(16, 16));
        b->setFixedWidth(30);
        b->setStyleSheet(ALIGN_BTN);
        tr->addWidget(b);
    }
    m_listBullet->setToolTip("Bulleted list");
    m_listNumbered->setToolTip("Numbered list");

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
    m_boldBtn->setToolTip("Bold (B)");
    m_italicBtn->setToolTip("Italic (I)");
    m_underlineBtn->setToolTip("Underline (U)");
    m_strikeBtn->setToolTip("Strikethrough");

    tr->addWidget(makeSep(m_textGroup));

    // Underline color + style (only meaningful when underline is on)
    m_ulColorBtn = new QPushButton("U", m_textGroup);
    m_ulColorBtn->setToolTip("Choose underline color");
    m_ulColorBtn->setFixedWidth(28);
    tr->addWidget(m_ulColorBtn);

    m_ulStyleCombo = new QComboBox(m_textGroup);
    m_ulStyleCombo->addItem("─── Solid");
    m_ulStyleCombo->addItem("─ ─ Dashed");
    m_ulStyleCombo->addItem("··· Dotted");
    m_ulStyleCombo->addItem("~~~ Wavy");
    m_ulStyleCombo->setFixedWidth(130);
    m_ulStyleCombo->setToolTip("Underline style");
    tr->addWidget(m_ulStyleCombo);

    tr->addWidget(makeSep(m_textGroup));

    m_linkBtn = new QPushButton(QIcon(":/icons/link.svg"), "Link", m_textGroup);
    m_linkBtn->setIconSize(QSize(14, 14));
    m_linkBtn->setCheckable(true);
    m_linkBtn->setStyleSheet(ALIGN_BTN + "QPushButton { min-width:78px; }");
    m_linkBtn->setToolTip("Set/remove hyperlink for this text");
    tr->addWidget(m_linkBtn);

    tr->addWidget(makeSep(m_textGroup));

    m_fmtPainterBtn = new QPushButton(QIcon(":/icons/format_paint.svg"), "Format", m_textGroup);
    m_fmtPainterBtn->setIconSize(QSize(14, 14));
    m_fmtPainterBtn->setToolTip("Apply format to another element");
    m_fmtPainterBtn->setStyleSheet(ALIGN_BTN + "QPushButton { min-width:92px; }");
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
    m_sizeW = mkDSpin("W:");  m_sizeW->setToolTip("Width (mouse wheel = live)");
    m_sizeH = mkDSpin("H:");  m_sizeH->setToolTip("Height (mouse wheel = live)");

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
    connect(m_linkBtn,      &QPushButton::clicked, this, &FormatBar::onLinkClicked);
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
    m_pres     = pres;
    m_slideIdx = slideIdx;
    m_elemIdx  = elemIdx;
    m_cellRow  = -1;
    m_cellCol  = -1;
    refresh();
}

void FormatBar::setTableCell(int row, int col) {
    m_cellRow = row;
    m_cellCol = col;
    refresh();
}

SlideElement* FormatBar::currentElem() {
    if (!m_pres || m_slideIdx < 0 || m_elemIdx < 0) return nullptr;
    if (m_slideIdx >= m_pres->slides.size()) return nullptr;
    auto& elems = m_pres->slides[m_slideIdx].elements;
    if (m_elemIdx >= elems.size()) return nullptr;
    return &elems[m_elemIdx];
}

TableCell* FormatBar::currentCell() {
    auto* e = currentElem();
    if (!e || e->type != SlideElement::Table) return nullptr;
    if (m_cellRow < 0 || m_cellRow >= e->tableRows ||
        m_cellCol < 0 || m_cellCol >= e->tableCols) return nullptr;
    return &e->tableCells[m_cellRow][m_cellCol];
}

void FormatBar::refresh() {
    m_updating = true;
    SlideElement* e    = currentElem();
    TableCell*    cell = currentCell();
    bool hasElem  = (e != nullptr);
    bool isText   = hasElem && e->type == SlideElement::Text;
    bool isShape  = hasElem && e->type == SlideElement::Shape;
    bool isCell   = (cell != nullptr);
    bool isTable  = hasElem && e->type == SlideElement::Table;

    m_textGroup->setEnabled(isText || isCell || isShape);
    m_geomGroup->setEnabled(hasElem);

    // Reset underline/list/vAlign (not supported per cell)
    m_ulStyleCombo->setEnabled(false);
    m_ulColorBtn->setEnabled(false);
    m_listBullet->setChecked(false);
    m_listNumbered->setChecked(false);
    m_listBullet->setEnabled(isText);
    m_listNumbered->setEnabled(isText);
    m_underlineBtn->setEnabled(isText || isShape);
    m_strikeBtn->setEnabled(isText || isShape);
    m_vAlignTop->setEnabled(isText);
    m_vAlignMiddle->setEnabled(isText);
    m_vAlignBottom->setEnabled(isText);
    m_linkBtn->setEnabled(isText);
    m_linkBtn->setChecked(isText && !e->hyperlink.trimmed().isEmpty());

    if (isCell) {
        // Font size shows table-wide font size
        m_fontCombo->setCurrentFont(QFont(e->tableFontFamily));
        m_fontSize->setValue(e->tableFontSize);
        // Colors from cell (fall back to table default)
        QColor tc = cell->textColor.isValid() ? cell->textColor : e->tableDefaultText;
        QColor bg = cell->bgColor.isValid()   ? cell->bgColor   : e->tableDefaultBg;
        updateColorSwatch(m_colorBtn,   tc);
        updateColorSwatch(m_bgColorBtn, bg);
        // Alignment
        m_alignLeft->setChecked(cell->textAlign.isEmpty() || cell->textAlign == "left");
        m_alignCenter->setChecked(cell->textAlign == "center");
        m_alignRight->setChecked(cell->textAlign == "right");
        m_vAlignTop->setChecked(false);
        m_vAlignMiddle->setChecked(false);
        m_vAlignBottom->setChecked(false);
        // Bold / Italic
        m_boldBtn->setChecked(cell->bold);
        m_italicBtn->setChecked(cell->italic);
        m_underlineBtn->setChecked(false);
        m_strikeBtn->setChecked(false);
        updateColorSwatch(m_ulColorBtn, Qt::black);
        m_fmtPainterBtn->setEnabled(false);
    } else if (isShape) {
        m_fontCombo->setCurrentFont(QFont(e->fontFamily));
        m_fontSize->setValue(e->fontSize);
        updateColorSwatch(m_colorBtn,   e->color.isValid() ? e->color : Qt::white);
        updateColorSwatch(m_bgColorBtn, e->backgroundColor);
        m_alignLeft->setChecked(false);
        m_alignCenter->setChecked(true);
        m_alignRight->setChecked(false);
        m_vAlignTop->setChecked(false);
        m_vAlignMiddle->setChecked(false);
        m_vAlignBottom->setChecked(false);
        m_boldBtn->setChecked(e->bold);
        m_italicBtn->setChecked(e->italic);
        m_underlineBtn->setChecked(e->underline);
        m_strikeBtn->setChecked(e->strikethrough);
        updateColorSwatch(m_ulColorBtn, Qt::black);
        m_fmtPainterBtn->setEnabled(true);
    } else if (isText) {
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
        m_boldBtn->setChecked(false);
        m_italicBtn->setChecked(false);
        m_underlineBtn->setChecked(false);
        m_strikeBtn->setChecked(false);
        updateColorSwatch(m_ulColorBtn, Qt::black);
        m_fmtPainterBtn->setEnabled(hasElem && !isTable);
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
                "QPushButton:disabled { opacity:0.4; }"
                "QToolTip { background:#1f2937; color:#f9fafb; border:none; padding:4px 8px; "
                "           border-radius:4px; font-size:10px; }").arg(hex, fg));
}

void FormatBar::onFontChanged(const QFont& f) {
    if (m_updating) return;
    if (auto* e = currentElem()) {
        if (e->type == SlideElement::Table) { e->tableFontFamily = f.family(); emit modified(); return; }
        if (e->type == SlideElement::Text || e->type == SlideElement::Shape)
            { e->fontFamily = f.family(); emit modified(); }
    }
}
void FormatBar::onFontSizeChanged(int v) {
    if (m_updating) return;
    if (auto* e = currentElem()) {
        if (e->type == SlideElement::Table) { e->tableFontSize = v; emit modified(); return; }
        if (e->type == SlideElement::Text || e->type == SlideElement::Shape)
            { e->fontSize = v; emit modified(); }
    }
}
void FormatBar::onColorClicked() {
    if (auto* cell = currentCell()) {
        QColor init = cell->textColor.isValid() ? cell->textColor : Qt::black;
        QColor c = QColorDialog::getColor(init, this, "Cell Text Color");
        if (!c.isValid()) return;
        cell->textColor = c; updateColorSwatch(m_colorBtn, c); emit modified(); return;
    }
    auto* e = currentElem(); if (!e) return;
    QColor c = QColorDialog::getColor(e->color, this, "Text Color");
    if (!c.isValid()) return;
    e->color = c; updateColorSwatch(m_colorBtn, c); emit modified();
}
void FormatBar::onBgColorClicked() {
    if (auto* cell = currentCell()) {
        auto* e = currentElem();
        QColor init = cell->bgColor.isValid() ? cell->bgColor : (e ? e->tableDefaultBg : Qt::white);
        QColor c = QColorDialog::getColor(init, this, "Cell Background");
        if (!c.isValid()) return;
        cell->bgColor = c; updateColorSwatch(m_bgColorBtn, c); emit modified(); return;
    }
    auto* e = currentElem(); if (!e) return;
    QColor init = e->backgroundColor.isValid() ? e->backgroundColor : Qt::white;
    QColor c = QColorDialog::getColor(init, this, "Background Color",
                                      QColorDialog::ShowAlphaChannel);
    if (!c.isValid()) return;
    e->backgroundColor = c; updateColorSwatch(m_bgColorBtn, c); emit modified();
}
void FormatBar::onAlignLeft() {
    if (m_updating) return;
    if (auto* cell = currentCell()) {
        cell->textAlign = "left";
        m_updating = true;
        m_alignLeft->setChecked(true); m_alignCenter->setChecked(false); m_alignRight->setChecked(false);
        m_updating = false; emit modified(); return;
    }
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->textAlignment = "left";
    m_updating = true;
    m_alignLeft->setChecked(true); m_alignCenter->setChecked(false); m_alignRight->setChecked(false);
    m_updating = false; emit modified();
}
void FormatBar::onAlignCenter() {
    if (m_updating) return;
    if (auto* cell = currentCell()) {
        cell->textAlign = "center";
        m_updating = true;
        m_alignLeft->setChecked(false); m_alignCenter->setChecked(true); m_alignRight->setChecked(false);
        m_updating = false; emit modified(); return;
    }
    auto* e = currentElem(); if (!e || e->type != SlideElement::Text) return;
    e->textAlignment = "center";
    m_updating = true;
    m_alignLeft->setChecked(false); m_alignCenter->setChecked(true); m_alignRight->setChecked(false);
    m_updating = false; emit modified();
}
void FormatBar::onAlignRight() {
    if (m_updating) return;
    if (auto* cell = currentCell()) {
        cell->textAlign = "right";
        m_updating = true;
        m_alignLeft->setChecked(false); m_alignCenter->setChecked(false); m_alignRight->setChecked(true);
        m_updating = false; emit modified(); return;
    }
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
    if (auto* cell = currentCell()) { cell->bold = m_boldBtn->isChecked(); emit modified(); return; }
    auto* e = currentElem();
    if (!e || (e->type != SlideElement::Text && e->type != SlideElement::Shape)) return;
    e->bold = m_boldBtn->isChecked(); emit modified();
}
void FormatBar::onItalic() {
    if (m_updating) return;
    if (auto* cell = currentCell()) { cell->italic = m_italicBtn->isChecked(); emit modified(); return; }
    auto* e = currentElem();
    if (!e || (e->type != SlideElement::Text && e->type != SlideElement::Shape)) return;
    e->italic = m_italicBtn->isChecked(); emit modified();
}
void FormatBar::onUnderline() {
    if (m_updating) return;
    auto* e = currentElem();
    if (!e || (e->type != SlideElement::Text && e->type != SlideElement::Shape)) return;
    e->underline = m_underlineBtn->isChecked();
    m_ulStyleCombo->setEnabled(e->underline && e->type == SlideElement::Text);
    m_ulColorBtn->setEnabled(e->underline && e->type == SlideElement::Text);
    emit modified();
}
void FormatBar::onStrikethrough() {
    if (m_updating) return;
    auto* e = currentElem();
    if (!e || (e->type != SlideElement::Text && e->type != SlideElement::Shape)) return;
    e->strikethrough = m_strikeBtn->isChecked(); emit modified();
}
void FormatBar::onLinkClicked() {
    auto* e = currentElem();
    if (!e || e->type != SlideElement::Text) { refresh(); return; }
    bool ok = false;
    QString url = QInputDialog::getText(this, "Hyperlink",
        "Target URL (leave empty to remove):", QLineEdit::Normal, e->hyperlink, &ok);
    if (!ok) { refresh(); return; }
    e->hyperlink = url.trimmed();
    refresh();
    emit modified();
}
void FormatBar::onUnderlineColorClicked() {
    auto* e = currentElem(); if (!e) return;
    QColor init = e->underlineColor.isValid() ? e->underlineColor : e->color;
    QColor c = QColorDialog::getColor(init, this, "Underline Color");
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
