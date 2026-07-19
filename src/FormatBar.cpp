#include "FormatBar.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
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

// Wraps a cluster of controls (contentLayout) with a small caption label
// underneath, Office-ribbon-style — turns one long flat row of buttons into
// scannable, named groups (e.g. "Schrift", "Ausrichtung", "Liste").
static QWidget* makeGroup(QWidget* parent, const QString& caption, QLayout* contentLayout) {
    auto* group = new QWidget(parent);
    auto* v = new QVBoxLayout(group);
    v->setContentsMargins(2, 1, 2, 1);
    v->setSpacing(1);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    v->addLayout(contentLayout);
    v->addStretch();
    auto* capLbl = new QLabel(caption, group);
    capLbl->setAlignment(Qt::AlignHCenter);
    capLbl->setStyleSheet("color:#9ca3af; font-size:9px;");
    v->addWidget(capLbl);
    return group;
}

FormatBar::FormatBar(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(4, 2, 4, 2);
    row->setSpacing(4);

    // ── Text formatting group ──────────────────────────────────────────────
    // Organized into small, captioned clusters (Office-ribbon style) instead
    // of one long flat row — each cluster stacks its controls over 1-2 rows
    // so the whole bar stays scannable instead of scrolling off-screen.
    m_textGroup = new QWidget(this);
    auto* tr = new QHBoxLayout(m_textGroup);
    tr->setContentsMargins(0, 0, 0, 0);
    tr->setSpacing(2);

    // Schrift: font name on top, size + text/background color below
    {
        auto* top = new QHBoxLayout();
        m_fontCombo = new QFontComboBox(m_textGroup);
        m_fontCombo->setFixedWidth(150);
        m_fontCombo->setFocusPolicy(Qt::StrongFocus);
        top->addWidget(m_fontCombo);

        auto* bottom = new QHBoxLayout();
        bottom->addWidget(lbl("Sz:", m_textGroup));
        m_fontSize = new QSpinBox(m_textGroup);
        m_fontSize->setRange(6, 400);
        m_fontSize->setValue(32);
        m_fontSize->setFixedWidth(52);
        m_fontSize->setSuffix("pt");
        m_fontSize->setToolTip("Font size – mouse wheel = change size live");
        bottom->addWidget(m_fontSize);

        m_colorBtn = new QPushButton("A", m_textGroup);
        m_colorBtn->setToolTip("Text color");
        m_colorBtn->setFixedWidth(28);
        bottom->addWidget(m_colorBtn);

        m_bgColorBtn = new QPushButton("Bg", m_textGroup);
        m_bgColorBtn->setToolTip("Background color");
        m_bgColorBtn->setFixedWidth(28);
        bottom->addWidget(m_bgColorBtn);

        auto* v = new QVBoxLayout();
        v->setSpacing(2);
        v->addLayout(top);
        v->addLayout(bottom);
        tr->addWidget(makeGroup(m_textGroup, "Text", v));
    }
    tr->addWidget(makeSep(m_textGroup));

    // Schriftschnitt: Bold/Italic/Underline/Strike (2x2), underline color+style below
    {
        m_boldBtn      = new QPushButton("B",  m_textGroup);
        m_italicBtn    = new QPushButton("I",  m_textGroup);
        m_underlineBtn = new QPushButton("U",  m_textGroup);
        m_strikeBtn    = new QPushButton("S̶", m_textGroup);
        for (auto* b : {m_boldBtn, m_italicBtn, m_underlineBtn, m_strikeBtn}) {
            b->setCheckable(true);
            b->setFixedWidth(26);
            b->setStyleSheet(ALIGN_BTN);
        }
        m_boldBtn->setStyleSheet(ALIGN_BTN + "QPushButton { font-weight:bold; }");
        m_italicBtn->setStyleSheet(ALIGN_BTN + "QPushButton { font-style:italic; }");
        m_boldBtn->setToolTip("Bold (B)");
        m_italicBtn->setToolTip("Italic (I)");
        m_underlineBtn->setToolTip("Underline (U)");
        m_strikeBtn->setToolTip("Strikethrough");

        auto* top = new QHBoxLayout();
        top->addWidget(m_boldBtn);
        top->addWidget(m_italicBtn);
        top->addWidget(m_underlineBtn);
        top->addWidget(m_strikeBtn);

        // Underline color + style (only meaningful when underline is on)
        m_ulColorBtn = new QPushButton("U", m_textGroup);
        m_ulColorBtn->setToolTip("Choose underline color");
        m_ulColorBtn->setFixedWidth(26);

        m_ulStyleCombo = new QComboBox(m_textGroup);
        m_ulStyleCombo->addItem("─── Solid");
        m_ulStyleCombo->addItem("─ ─ Dashed");
        m_ulStyleCombo->addItem("··· Dotted");
        m_ulStyleCombo->addItem("~~~ Wavy");
        m_ulStyleCombo->setFixedWidth(90);
        m_ulStyleCombo->setToolTip("Underline style");

        auto* bottom = new QHBoxLayout();
        bottom->addWidget(m_ulColorBtn);
        bottom->addWidget(m_ulStyleCombo);

        auto* v = new QVBoxLayout();
        v->setSpacing(2);
        v->addLayout(top);
        v->addLayout(bottom);
        tr->addWidget(makeGroup(m_textGroup, "Text Format", v));
    }
    tr->addWidget(makeSep(m_textGroup));

    // Ausrichtung: horizontal align row, vertical align row below
    {
        m_alignLeft   = new QPushButton(QIcon(":/icons/format_align_left.svg"),   "", m_textGroup);
        m_alignCenter = new QPushButton(QIcon(":/icons/format_align_center.svg"), "", m_textGroup);
        m_alignRight  = new QPushButton(QIcon(":/icons/format_align_right.svg"),  "", m_textGroup);
        auto* top = new QHBoxLayout();
        for (auto* b : {m_alignLeft, m_alignCenter, m_alignRight}) {
            b->setCheckable(true);
            b->setIconSize(QSize(16, 16));
            b->setFixedWidth(28);
            b->setStyleSheet(ALIGN_BTN);
            top->addWidget(b);
        }
        m_alignLeft->setToolTip("Align Left");
        m_alignCenter->setToolTip("Center");
        m_alignRight->setToolTip("Align Right");

        m_vAlignTop    = new QPushButton(QIcon(":/icons/vertical_align_top.svg"),    "", m_textGroup);
        m_vAlignMiddle = new QPushButton(QIcon(":/icons/vertical_align_center.svg"), "", m_textGroup);
        m_vAlignBottom = new QPushButton(QIcon(":/icons/vertical_align_bottom.svg"), "", m_textGroup);
        auto* bottom = new QHBoxLayout();
        for (auto* b : {m_vAlignTop, m_vAlignMiddle, m_vAlignBottom}) {
            b->setCheckable(true);
            b->setIconSize(QSize(16, 16));
            b->setFixedWidth(28);
            b->setStyleSheet(ALIGN_BTN);
            bottom->addWidget(b);
        }
        m_vAlignTop->setToolTip("Align Top");
        m_vAlignMiddle->setToolTip("Align Middle");
        m_vAlignBottom->setToolTip("Align Bottom");

        auto* v = new QVBoxLayout();
        v->setSpacing(2);
        v->addLayout(top);
        v->addLayout(bottom);
        tr->addWidget(makeGroup(m_textGroup, "Orientation", v));
    }
    tr->addWidget(makeSep(m_textGroup));

    // Liste: bullet + numbered
    {
        m_listBullet   = new QPushButton(QIcon(":/icons/format_list_bulleted.svg"), "", m_textGroup);
        m_listNumbered = new QPushButton(QIcon(":/icons/format_list_numbered.svg"), "", m_textGroup);
        auto* top = new QHBoxLayout();
        for (auto* b : {m_listBullet, m_listNumbered}) {
            b->setCheckable(true);
            b->setIconSize(QSize(16, 16));
            b->setFixedWidth(28);
            b->setStyleSheet(ALIGN_BTN);
            top->addWidget(b);
        }
        m_listBullet->setToolTip("Bulleted list");
        m_listNumbered->setToolTip("Numbered list");

        auto* v = new QVBoxLayout();
        v->addLayout(top);
        tr->addWidget(makeGroup(m_textGroup, "List", v));
    }
    tr->addWidget(makeSep(m_textGroup));

    // Erweitert: link (top), code-language + format painter (bottom)
    {
        m_linkBtn = new QPushButton(QIcon(":/icons/link.svg"), "Link", m_textGroup);
        m_linkBtn->setIconSize(QSize(14, 14));
        m_linkBtn->setCheckable(true);
        m_linkBtn->setStyleSheet(ALIGN_BTN + "QPushButton { min-width:70px; }");
        m_linkBtn->setToolTip("Set/remove hyperlink for this text");

        m_fmtPainterBtn = new QPushButton(QIcon(":/icons/format_paint.svg"), "Format", m_textGroup);
        m_fmtPainterBtn->setIconSize(QSize(14, 14));
        m_fmtPainterBtn->setToolTip("Apply format to another element");
        m_fmtPainterBtn->setStyleSheet(ALIGN_BTN + "QPushButton { min-width:70px; }");

        auto* top = new QHBoxLayout();
        top->addWidget(m_linkBtn);
        top->addWidget(m_fmtPainterBtn);

        // Single compact combo instead of a separate toggle button: first
        // entry turns code formatting off, any language entry turns it on
        // with that language (spans are normally created automatically on
        // paste — see SlideEditor2D::handleTextEditKey/pasteTextAsNewElement
        // — this combo is for overriding/correcting the guessed language, or
        // for the rare manual case where auto-detection didn't trigger).
        m_codeLangCombo = new QComboBox(m_textGroup);
        m_codeLangCombo->addItem("Code: off", QString());
        for (const char* langPair : {"plaintext:Plain code", "javascript:JavaScript", "python:Python",
                                      "cpp:C++", "csharp:C#", "java:Java", "html:HTML", "css:CSS",
                                      "json:JSON", "bash:Bash", "sql:SQL", "php:PHP", "xml:XML"}) {
            QString s = QString::fromLatin1(langPair);
            m_codeLangCombo->addItem(s.section(':', 1), s.section(':', 0, 0));
        }
        m_codeLangCombo->setToolTip("Mark the selected text as a code block / choose its language");
        auto* bottom = new QHBoxLayout();
        bottom->addWidget(m_codeLangCombo);

        auto* v = new QVBoxLayout();
        v->setSpacing(2);
        v->addLayout(top);
        v->addLayout(bottom);
        tr->addWidget(makeGroup(m_textGroup, "Advanced", v));
    }

    row->addWidget(m_textGroup);
    row->addWidget(makeSep(this));

    // ── Geometry group: Position (X/Y) on top, Size (W/H) below ─────────────
    m_geomGroup = new QWidget(this);
    {
        auto* v = new QVBoxLayout(m_geomGroup);
        v->setContentsMargins(2, 1, 2, 1);
        v->setSpacing(1);

        auto mkDSpin = [&](QHBoxLayout* into, const QString& label) -> QDoubleSpinBox* {
            into->addWidget(lbl(label, m_geomGroup));
            auto* sb = new QDoubleSpinBox(m_geomGroup);
            sb->setRange(-99999, 99999);
            sb->setDecimals(0);
            sb->setFixedWidth(62);
            into->addWidget(sb);
            return sb;
        };
        auto* top = new QHBoxLayout();
        m_posX  = mkDSpin(top, "X:");  m_posX->setToolTip("Position X");
        m_posY  = mkDSpin(top, "Y:");  m_posY->setToolTip("Position Y");
        auto* bottom = new QHBoxLayout();
        m_sizeW = mkDSpin(bottom, "W:");  m_sizeW->setToolTip("Width (mouse wheel = live)");
        m_sizeH = mkDSpin(bottom, "H:");  m_sizeH->setToolTip("Height (mouse wheel = live)");

        v->addLayout(top);
        v->addLayout(bottom);
        auto* capLbl = new QLabel("Position && Größe", m_geomGroup);
        capLbl->setAlignment(Qt::AlignHCenter);
        capLbl->setStyleSheet("color:#9ca3af; font-size:9px;");
        v->addWidget(capLbl);
    }

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
    connect(m_codeLangCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &FormatBar::onCodeLangChanged);
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
    bool isAudio  = hasElem && e->type == SlideElement::Audio;
    bool isCell   = (cell != nullptr);
    bool isTable  = hasElem && e->type == SlideElement::Table;

    m_textGroup->setEnabled(isText || isCell || isShape || isAudio);
    m_geomGroup->setEnabled(hasElem);

    // Audio has no font/alignment/list/link concept — only the color swatches
    // (background pill + label text, see SlideEditor2D::drawElement's Audio
    // branch) apply, so those are the only controls this type enables.
    m_fontCombo->setEnabled(isText || isShape || isCell);
    m_fontSize->setEnabled(isText || isShape || isCell);
    m_boldBtn->setEnabled(isText || isShape || isCell);
    m_italicBtn->setEnabled(isText || isShape || isCell);
    m_alignLeft->setEnabled(isText || isCell);
    m_alignCenter->setEnabled(isText || isShape || isCell);
    m_alignRight->setEnabled(isText || isCell);

    // Reset underline/list/vAlign (not supported per cell)
    m_ulStyleCombo->setEnabled(false);
    m_ulColorBtn->setEnabled(false);
    m_codeLangCombo->setEnabled(false);
    m_codeLangCombo->setCurrentIndex(0);
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

        // Code-span state at the current selection/cursor position (only
        // meaningful outside of list-formatted text — see plan notes).
        if (e->listStyle == SlideElement::NoList && m_selStart >= 0) {
            m_codeLangCombo->setEnabled(true);
            const CodeSpan* active = nullptr;
            for (const CodeSpan& sp : e->codeSpans) {
                if (m_selStart >= sp.start && m_selStart <= sp.start + sp.length) { active = &sp; break; }
            }
            if (active) {
                int idx = m_codeLangCombo->findData(active->language.isEmpty() ? "plaintext" : active->language);
                m_codeLangCombo->setCurrentIndex(idx >= 0 ? idx : 0);
            } else {
                m_codeLangCombo->setCurrentIndex(0); // "Code: off"
            }
        }
    } else if (isAudio) {
        // Only the color swatches are meaningful here (see SlideEditor2D::
        // drawElement's Audio branch: background pill + label text color) —
        // font/alignment/list/link have no effect on this element type.
        updateColorSwatch(m_colorBtn,   e->color.isValid() ? e->color : QColor(203, 213, 225));
        updateColorSwatch(m_bgColorBtn, e->backgroundColor.isValid() && e->backgroundColor != Qt::transparent
                                             ? e->backgroundColor : QColor(30, 41, 59));
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
        m_fmtPainterBtn->setEnabled(false);
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
    // Elements default to a transparent background (valid QColor, alpha 0) —
    // treat that the same as "unset" here, else the alpha-channel dialog opens
    // with the slider already at 0 and a color pick that doesn't also drag the
    // slider up silently stays fully transparent (invisible, looks like the
    // change "didn't save").
    QColor init = (e->backgroundColor.isValid() && e->backgroundColor != Qt::transparent)
                      ? e->backgroundColor : Qt::white;
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
void FormatBar::setTextSelection(int cursorPos, int selAnchor) {
    if (cursorPos < 0) {
        m_selStart = -1;
        m_selEnd   = -1;
    } else {
        int anchor = (selAnchor < 0) ? cursorPos : selAnchor;
        m_selStart = qMin(cursorPos, anchor);
        m_selEnd   = qMax(cursorPos, anchor);
    }
    refresh();
}
void FormatBar::onCodeLangChanged(int) {
    if (m_updating) return;
    auto* e = currentElem();
    if (!e || e->type != SlideElement::Text || e->listStyle != SlideElement::NoList || m_selStart < 0) {
        refresh();
        return;
    }
    QString lang = m_codeLangCombo->currentData().toString();
    int hi = qMax(m_selEnd, m_selStart); // collapsed cursor: hi == m_selStart

    CodeSpan* active = nullptr;
    for (CodeSpan& sp : e->codeSpans) {
        if (m_selStart >= sp.start && m_selStart <= sp.start + sp.length) { active = &sp; break; }
    }

    if (lang.isEmpty()) {
        // "Code: off" — remove any span covering the current position/selection.
        if (!active) { refresh(); return; }
        QVector<CodeSpan> kept;
        for (const CodeSpan& sp : e->codeSpans)
            if (sp.start + sp.length <= m_selStart || sp.start >= hi) kept << sp;
        e->codeSpans = kept;
    } else if (active) {
        active->language = lang;
    } else {
        if (m_selStart >= m_selEnd) { refresh(); return; } // need a non-empty selection to arm a new span
        QVector<CodeSpan> kept;
        for (const CodeSpan& sp : e->codeSpans)
            if (sp.start + sp.length <= m_selStart || sp.start >= m_selEnd) kept << sp;
        CodeSpan added;
        added.start    = m_selStart;
        added.length   = m_selEnd - m_selStart;
        added.language = lang;
        kept << added;
        e->codeSpans = kept;
    }
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
