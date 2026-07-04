#include "EditorArea.h"
#include "SlideEditor2D.h"
#include "SlideEditor3D.h"
#include "dialogs/InsertTableDialog.h"
#include "dialogs/InsertChartDialog.h"
#include "dialogs/InsertFormulaDialog.h"
#include "dialogs/InsertIFrameDialog.h"
#include "dialogs/InsertButtonDialog.h"
#include "dialogs/ShapePickerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QFrame>
#include <QIcon>

// Any widget with its own stylesheet stops inheriting the app-wide dark
// QToolTip style, so every local stylesheet below re-declares it explicitly
// (otherwise tooltips fall back to an unreadable white-on-white look).
static const char* kToolTipSS =
    "QToolTip { background:#1f2937; color:#f9fafb; border:none; padding:4px 8px; "
    "           border-radius:4px; font-size:10px; }";

EditorArea::EditorArea(QWidget* parent) : QWidget(parent) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Top bar: mode toggle ──────────────────────────────────────────
    auto* topBar = new QWidget(this);
    topBar->setFixedHeight(38);
    topBar->setStyleSheet(QString("background:#ffffff; border-bottom:1px solid #e5e7eb;") + kToolTipSS);
    auto* topRow = new QHBoxLayout(topBar);
    topRow->setContentsMargins(8, 4, 8, 4);

    m_btn2D = new QPushButton("2D Mode", topBar);
    m_btn3D = new QPushButton("3D Mode", topBar);
    for (auto* b : {m_btn2D, m_btn3D}) {
        b->setCheckable(true);
        b->setFixedWidth(90);
    }
    m_btn2D->setChecked(true);

    topRow->addStretch();
    topRow->addWidget(m_btn2D);
    topRow->addWidget(m_btn3D);
    topRow->addStretch();
    mainLayout->addWidget(topBar);

    // ── Element toolbar (2D only) ─────────────────────────────────────
    m_elemToolbar = new QWidget(this);
    m_elemToolbar->setFixedHeight(36);
    m_elemToolbar->setStyleSheet(QString("background:#f9fafb; border-bottom:1px solid #e5e7eb;") + kToolTipSS);
    auto* tbRow = new QHBoxLayout(m_elemToolbar);
    tbRow->setContentsMargins(8, 3, 8, 3);
    tbRow->setSpacing(6);

    auto mkBtn = [&](const QString& icon, const QString& text, const QString& tip) {
        auto* b = new QPushButton(QIcon(":/icons/" + icon + ".svg"), text, m_elemToolbar);
        b->setIconSize(QSize(16, 16));
        b->setToolTip(tip);
        b->setMinimumWidth(80);
        tbRow->addWidget(b);
        return b;
    };
    m_btnText   = mkBtn("title",        "Text",    "Add text box");
    m_btnShape  = mkBtn("shapes",       "Shapes",  "Insert shape");
    m_btnImage  = mkBtn("image",        "Image",   "Import image");
    m_btnTable   = mkBtn("table",        "Table",  "Insert table");
    m_btnChart   = mkBtn("bar_chart",    "Chart",  "Insert chart");
    m_btnFormula = mkBtn("functions",    "Formula", "Insert formula (LaTeX)");
    m_btnIFrame  = mkBtn("language",     "iFrame", "Embed website/link");
    m_btnButton  = mkBtn("smart_button", "Button", "Insert navigation button (jumps to a slide)");

    // Separator
    auto* sep = new QFrame(m_elemToolbar);
    sep->setFrameShape(QFrame::VLine);
    tbRow->addWidget(sep);

    // Layer buttons (smaller)
    static const char* LAYER_BTN_SS =
        "QPushButton { padding:1px 3px; min-height:0; font-size:13px; color:#111827; "
        "              background:#ffffff; border:1px solid #d1d5db; border-radius:4px; }"
        "QPushButton:hover   { background:#f3f4f6; }"
        "QPushButton:pressed { background:#eff6ff; color:#2563eb; }"
        "QToolTip { background:#1f2937; color:#f9fafb; border:none; padding:4px 8px; "
        "           border-radius:4px; font-size:10px; }";

    auto mkLayerBtn = [&](const QString& icon, const QString& tip) {
        auto* b = new QPushButton(QIcon(":/icons/" + icon + ".svg"), QString(), m_elemToolbar);
        b->setIconSize(QSize(16, 16));
        b->setToolTip(tip);
        b->setFixedWidth(32);
        b->setStyleSheet(LAYER_BTN_SS);
        tbRow->addWidget(b);
        return b;
    };
    m_btnFront    = mkLayerBtn("flip_to_front",  "Bring to Front (Topmost)");
    m_btnForward  = mkLayerBtn("arrow_upward",   "Move one layer up");
    m_btnBackward = mkLayerBtn("arrow_downward", "Move one layer down");
    m_btnBack     = mkLayerBtn("flip_to_back",   "Send to Back (Bottommost)");

    tbRow->addStretch();
    m_btnDelete = mkBtn("delete", "Delete", "Delete selected element");
    m_btnDelete->setStyleSheet(
        "QPushButton { background:#fef2f2; color:#dc2626; border:1px solid #fecaca; padding:3px; border-radius:4px; }"
        "QPushButton:hover { background:#fee2e2; border-color:#f87171; }"
        "QToolTip { background:#1f2937; color:#f9fafb; border:none; padding:4px 8px; "
        "           border-radius:4px; font-size:10px; }");
    mainLayout->addWidget(m_elemToolbar);

    // ── 3D Gizmo toolbar (shown only in 3D mode) ──────────────────────
    m_gizmoToolbar = new QWidget(this);
    m_gizmoToolbar->setFixedHeight(36);
    m_gizmoToolbar->setStyleSheet(QString("background:#f9fafb; border-bottom:1px solid #e5e7eb;") + kToolTipSS);
    auto* gRow = new QHBoxLayout(m_gizmoToolbar);
    gRow->setContentsMargins(8, 3, 8, 3);
    gRow->setSpacing(6);

    auto mkGizmoBtn = [&](const QString& icon, const QString& text, const QString& tip) {
        auto* b = new QPushButton(QIcon(":/icons/" + icon + ".svg"), text, m_gizmoToolbar);
        b->setIconSize(QSize(16, 16));
        b->setToolTip(tip);
        b->setCheckable(true);
        b->setFixedWidth(110);
        gRow->addWidget(b);
        return b;
    };
    m_btnGizmoMove   = mkGizmoBtn("open_with",   "Move [W]",   "Move slide in 3D (drag X/Y/Z arrows)");
    m_btnGizmoRotate = mkGizmoBtn("3d_rotation", "Rotate [E]", "Rotate slide in 3D (drag X/Y/Z rings)");
    m_btnGizmoMove->setChecked(true);

    gRow->addStretch();

    // Camera distance control
    auto* distLbl = new QLabel("Distance:", m_gizmoToolbar);
    distLbl->setStyleSheet("color:#9ca3af; font-size:9px;");
    gRow->addWidget(distLbl);
    m_distanceSpin = new QDoubleSpinBox(m_gizmoToolbar);
    m_distanceSpin->setRange(200.0, 100000.0);
    m_distanceSpin->setValue(6000.0);
    m_distanceSpin->setSingleStep(500.0);
    m_distanceSpin->setDecimals(0);
    m_distanceSpin->setFixedWidth(90);
    m_distanceSpin->setToolTip("Camera distance (mouse wheel also changes the distance)");
    gRow->addWidget(m_distanceSpin);

    gRow->addSpacing(12);
    auto* hint = new QLabel("L-Drag: Orbit  |  R-Drag: Pan  |  Scroll: Zoom", m_gizmoToolbar);
    hint->setStyleSheet("color:#9ca3af; font-size:9px;");
    gRow->addWidget(hint);

    m_gizmoToolbar->setVisible(false); // hidden until 3D mode
    mainLayout->addWidget(m_gizmoToolbar);

    // ── Stacked editor ────────────────────────────────────────────────
    m_stack    = new QStackedWidget(this);
    m_editor2D = new SlideEditor2D(this);
    m_editor3D = new SlideEditor3D(this);
    m_stack->addWidget(m_editor2D);
    m_stack->addWidget(m_editor3D);
    m_stack->setCurrentIndex(0);
    mainLayout->addWidget(m_stack, 1);

    // ── Connections ───────────────────────────────────────────────────
    connect(m_btn2D, &QPushButton::clicked, this, &EditorArea::switchTo2D);
    connect(m_btn3D, &QPushButton::clicked, this, &EditorArea::switchTo3D);

    connect(m_btnText,  &QPushButton::clicked, m_editor2D, &SlideEditor2D::addTextElement);
    connect(m_btnShape, &QPushButton::clicked, this, [this]() {
        ShapePickerDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted && !dlg.selectedShape().isEmpty())
            m_editor2D->addShapeElement(dlg.selectedShape());
    });
    connect(m_btnImage, &QPushButton::clicked, m_editor2D, &SlideEditor2D::addImageElement);
    connect(m_btnTable,    &QPushButton::clicked, this, [this]() {
        InsertTableDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted)
            m_editor2D->addTableElement(dlg.rows(), dlg.cols());
    });
    connect(m_btnChart,    &QPushButton::clicked, this, [this]() {
        InsertChartDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted)
            m_editor2D->addChartElement(dlg.selectedType());
    });
    connect(m_btnFormula,  &QPushButton::clicked, this, [this]() {
        InsertFormulaDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted && !dlg.latex().isEmpty())
            m_editor2D->addFormulaElement(dlg.latex());
    });
    connect(m_btnIFrame,   &QPushButton::clicked, this, [this]() {
        InsertIFrameDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted && !dlg.url().isEmpty())
            m_editor2D->addIFrameElement(dlg.url());
    });
    connect(m_btnButton,   &QPushButton::clicked, this, [this]() {
        QVector<QPair<QString, QString>> slides;
        if (m_pres) {
            for (const Slide& s : m_pres->slides)
                slides.append({s.id, s.name.isEmpty() ? QString("Slide %1").arg(slides.size() + 1) : s.name});
        }
        InsertButtonDialog dlg(this, slides);
        if (dlg.exec() == QDialog::Accepted)
            m_editor2D->addButtonElement(dlg.label(), dlg.targetSlideId());
    });
    connect(m_btnDelete,   &QPushButton::clicked, m_editor2D, &SlideEditor2D::deleteSelectedElement);
    connect(m_btnFront,    &QPushButton::clicked, m_editor2D, &SlideEditor2D::bringToFront);
    connect(m_btnForward,  &QPushButton::clicked, m_editor2D, &SlideEditor2D::bringForward);
    connect(m_btnBackward, &QPushButton::clicked, m_editor2D, &SlideEditor2D::sendBackward);
    connect(m_btnBack,     &QPushButton::clicked, m_editor2D, &SlideEditor2D::sendToBack);

    // Gizmo mode buttons
    connect(m_btnGizmoMove, &QPushButton::clicked, this, [this]() {
        m_btnGizmoMove->setChecked(true);
        m_btnGizmoRotate->setChecked(false);
        m_editor3D->setGizmoMode(SlideEditor3D::GizmoMode::Move);
    });
    connect(m_btnGizmoRotate, &QPushButton::clicked, this, [this]() {
        m_btnGizmoMove->setChecked(false);
        m_btnGizmoRotate->setChecked(true);
        m_editor3D->setGizmoMode(SlideEditor3D::GizmoMode::Rotate);
    });

    connect(m_editor2D, &SlideEditor2D::presentationModified,
            this, &EditorArea::presentationModified);
    connect(m_editor2D, &SlideEditor2D::presentationModified,
            m_editor3D, &SlideEditor3D::markAllDirty);
    connect(m_editor2D, &SlideEditor2D::elementSelected,
            this, &EditorArea::elementSelected);
    connect(m_editor2D, &SlideEditor2D::tableCellSelected,
            this, &EditorArea::tableCellSelected);

    connect(m_editor3D, &SlideEditor3D::presentationModified,
            this, &EditorArea::presentationModified);
    connect(m_editor3D, &SlideEditor3D::slideSelected, this, [this](int /*idx*/) {
        // Properties panel refresh is handled via presentationModified
    });
    connect(m_editor3D, &SlideEditor3D::distanceChanged, this, [this](float d) {
        QSignalBlocker blk(m_distanceSpin);
        m_distanceSpin->setValue(d);
    });
    connect(m_distanceSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_editor3D->setDistance(float(v));
    });
}

void EditorArea::setPresentation(Presentation* pres, int slideIndex) {
    m_pres       = pres;
    m_slideIndex = slideIndex;
    m_editor2D->setSlide(pres, slideIndex);
    m_editor3D->setPresentation(pres, slideIndex);
}

void EditorArea::refresh() {
    m_editor2D->update();
    m_editor3D->update();
}

void EditorArea::activateFormatPainter(const SlideElement& source) {
    m_editor2D->activateFormatPainter(source);
}

void EditorArea::switchTo2D() {
    m_btn2D->setChecked(true);
    m_btn3D->setChecked(false);
    m_stack->setCurrentIndex(0);
    m_elemToolbar->setVisible(true);
    m_gizmoToolbar->setVisible(false);
}

void EditorArea::switchTo3D() {
    m_btn2D->setChecked(false);
    m_btn3D->setChecked(true);
    m_stack->setCurrentIndex(1);
    m_elemToolbar->setVisible(false);
    m_gizmoToolbar->setVisible(true);
    m_editor3D->setFocus();
    m_editor3D->update();
}
