#include "EditorArea.h"
#include "SlideEditor2D.h"
#include "SlideEditor3D.h"
#include "TimelinePanel.h"
#include <cstdio>
#include "dialogs/InsertTableDialog.h"
#include "dialogs/InsertChartDialog.h"
#include "dialogs/InsertFormulaDialog.h"
#include "dialogs/InsertIFrameDialog.h"
#include "dialogs/InsertButtonDialog.h"
#include "dialogs/InsertCheckboxDialog.h"
#include "dialogs/InsertSliderDialog.h"
#include "dialogs/InsertWorldObjectDialog.h"
#include "dialogs/ShapePickerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QSplitter>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QDoubleSpinBox>
#include <QSpinBox>
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

    m_btnInteractive = new QToolButton(m_elemToolbar);
    m_btnInteractive->setIcon(QIcon(":/icons/tune.svg"));
    m_btnInteractive->setText("Interactive");
    m_btnInteractive->setIconSize(QSize(16, 16));
    m_btnInteractive->setToolTip("Insert an interactive element: Button, Checkbox or Slider");
    m_btnInteractive->setMinimumWidth(100);
    m_btnInteractive->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnInteractive->setPopupMode(QToolButton::InstantPopup);
    auto* interactiveMenu = new QMenu(m_btnInteractive);
    QAction* actInsertButton   = interactiveMenu->addAction(QIcon(":/icons/smart_button.svg"), "Button");
    QAction* actInsertCheckbox = interactiveMenu->addAction(QIcon(":/icons/check_circle.svg"), "Checkbox");
    QAction* actInsertSlider   = interactiveMenu->addAction(QIcon(":/icons/tune.svg"), "Slider");
    m_btnInteractive->setMenu(interactiveMenu);
    tbRow->addWidget(m_btnInteractive);

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

    // Separator
    auto* sepZoom = new QFrame(m_elemToolbar);
    sepZoom->setFrameShape(QFrame::VLine);
    tbRow->addWidget(sepZoom);

    // Zoom controls — for zooming into complex slide layouts
    auto mkZoomBtn = [&](const QString& text, const QString& tip) {
        auto* b = new QPushButton(text, m_elemToolbar);
        b->setToolTip(tip);
        b->setFixedWidth(24);
        b->setStyleSheet(LAYER_BTN_SS);
        tbRow->addWidget(b);
        return b;
    };
    m_btnZoomOut = mkZoomBtn(QString::fromUtf8("\xE2\x88\x92"), "Zoom out (Ctrl+Scroll / Ctrl+-)");
    m_zoomSpin = new QSpinBox(m_elemToolbar);
    m_zoomSpin->setRange(25, 600);
    m_zoomSpin->setSingleStep(10);
    m_zoomSpin->setValue(100);
    m_zoomSpin->setSuffix("%");
    m_zoomSpin->setFixedWidth(64);
    m_zoomSpin->setToolTip("Zoom level");
    tbRow->addWidget(m_zoomSpin);
    m_btnZoomIn = mkZoomBtn("+", "Zoom in (Ctrl+Scroll / Ctrl++)");
    m_btnZoomReset = new QPushButton("Fit", m_elemToolbar);
    m_btnZoomReset->setToolTip("Reset zoom to fit the slide (Ctrl+0)");
    m_btnZoomReset->setFixedWidth(36);
    m_btnZoomReset->setStyleSheet(LAYER_BTN_SS);
    tbRow->addWidget(m_btnZoomReset);

    m_btnTimelineToggle = new QPushButton("Timeline", m_elemToolbar);
    m_btnTimelineToggle->setCheckable(true);
    m_btnTimelineToggle->setChecked(false);
    m_btnTimelineToggle->setToolTip("Show/hide the per-slide animation Timeline panel");
    m_btnTimelineToggle->setFixedWidth(80);
    tbRow->addWidget(m_btnTimelineToggle);

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
    m_btnGizmoScale  = mkGizmoBtn("tune",        "Scale [R]",  "Scale a 3D object (drag X/Y/Z handles)");
    m_btnGizmoMove->setChecked(true);

    gRow->addSpacing(12);
    m_btnInsertWorldObj = new QPushButton(QIcon(":/icons/hub.svg"), "Insert Object", m_gizmoToolbar);
    m_btnInsertWorldObj->setIconSize(QSize(16, 16));
    m_btnInsertWorldObj->setToolTip("Insert a free-floating 3D model (.gltf/.glb)");
    m_btnInsertWorldObj->setFixedWidth(130);
    gRow->addWidget(m_btnInsertWorldObj);

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

    // ── Stacked editor + Timeline panel (vertical split) ───────────────
    m_stack    = new QStackedWidget(this);
    m_editor2D = new SlideEditor2D(this);
    m_editor3D = new SlideEditor3D(this);
    m_stack->addWidget(m_editor2D);
    m_stack->addWidget(m_editor3D);
    m_stack->setCurrentIndex(0);

    m_timelinePanel = new TimelinePanel(this);
    m_timelinePanel->setVisible(false); // hidden by default; shown via the Timeline toolbar toggle

    auto* vsplit = new QSplitter(Qt::Vertical, this);
    vsplit->addWidget(m_stack);
    vsplit->addWidget(m_timelinePanel);
    vsplit->setStretchFactor(0, 4);
    vsplit->setStretchFactor(1, 1);
    vsplit->setSizes({600, 180});
    mainLayout->addWidget(vsplit, 1);

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
    auto currentSlideId = [this]() -> QString {
        const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
        return s ? s->id : QString();
    };
    connect(actInsertButton, &QAction::triggered, this, [this, currentSlideId]() {
        InsertButtonDialog dlg(this, m_pres, currentSlideId());
        if (dlg.exec() == QDialog::Accepted)
            m_editor2D->addButtonElement(dlg.config());
    });
    connect(actInsertCheckbox, &QAction::triggered, this, [this, currentSlideId]() {
        InsertCheckboxDialog dlg(this, m_pres, currentSlideId());
        if (dlg.exec() == QDialog::Accepted)
            m_editor2D->addCheckboxElement(dlg.config());
    });
    connect(actInsertSlider, &QAction::triggered, this, [this, currentSlideId]() {
        InsertSliderDialog dlg(this, m_pres, currentSlideId());
        if (dlg.exec() == QDialog::Accepted)
            m_editor2D->addSliderElement(dlg.config());
    });
    connect(m_btnDelete,   &QPushButton::clicked, m_editor2D, &SlideEditor2D::deleteSelectedElement);
    connect(m_btnFront,    &QPushButton::clicked, m_editor2D, &SlideEditor2D::bringToFront);
    connect(m_btnForward,  &QPushButton::clicked, m_editor2D, &SlideEditor2D::bringForward);
    connect(m_btnBackward, &QPushButton::clicked, m_editor2D, &SlideEditor2D::sendBackward);
    connect(m_btnBack,     &QPushButton::clicked, m_editor2D, &SlideEditor2D::sendToBack);

    // Zoom controls
    connect(m_btnZoomOut,   &QPushButton::clicked, m_editor2D, &SlideEditor2D::zoomOut);
    connect(m_btnZoomIn,    &QPushButton::clicked, m_editor2D, &SlideEditor2D::zoomIn);
    connect(m_btnZoomReset, &QPushButton::clicked, m_editor2D, &SlideEditor2D::zoomReset);
    connect(m_zoomSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            m_editor2D, &SlideEditor2D::setZoomPercent);
    connect(m_editor2D, &SlideEditor2D::zoomChanged, this, [this](float z) {
        QSignalBlocker blk(m_zoomSpin);
        m_zoomSpin->setValue(qRound(z * 100.f));
    });

    // Gizmo mode buttons
    connect(m_btnGizmoMove, &QPushButton::clicked, this, [this]() {
        m_btnGizmoMove->setChecked(true);
        m_btnGizmoRotate->setChecked(false);
        m_btnGizmoScale->setChecked(false);
        m_editor3D->setGizmoMode(SlideEditor3D::GizmoMode::Move);
    });
    connect(m_btnGizmoRotate, &QPushButton::clicked, this, [this]() {
        m_btnGizmoMove->setChecked(false);
        m_btnGizmoRotate->setChecked(true);
        m_btnGizmoScale->setChecked(false);
        m_editor3D->setGizmoMode(SlideEditor3D::GizmoMode::Rotate);
    });
    connect(m_btnGizmoScale, &QPushButton::clicked, this, [this]() {
        m_btnGizmoMove->setChecked(false);
        m_btnGizmoRotate->setChecked(false);
        m_btnGizmoScale->setChecked(true);
        m_editor3D->setGizmoMode(SlideEditor3D::GizmoMode::Scale);
    });
    connect(m_btnInsertWorldObj, &QPushButton::clicked, this, [this]() {
        InsertWorldObjectDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted && !dlg.modelPath().isEmpty())
            m_editor3D->addWorldObject(dlg.modelPath());
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
    connect(m_editor3D, &SlideEditor3D::worldObjectSelected,
            this, &EditorArea::worldObjectSelected);
    connect(m_editor3D, &SlideEditor3D::distanceChanged, this, [this](float d) {
        QSignalBlocker blk(m_distanceSpin);
        m_distanceSpin->setValue(d);
    });
    connect(m_distanceSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        m_editor3D->setDistance(float(v));
    });

    // ── Timeline panel ──────────────────────────────────────────────────
    connect(m_btnTimelineToggle, &QPushButton::toggled, m_timelinePanel, &QWidget::setVisible);
    connect(m_timelinePanel, &TimelinePanel::timelineModified,
            this, &EditorArea::presentationModified);
    connect(m_timelinePanel, &TimelinePanel::timelineModified,
            m_editor2D, QOverload<>::of(&QWidget::update));
    connect(m_timelinePanel, &TimelinePanel::keyframeEditRequested,
            this, &EditorArea::keyframeEditRequested);
    connect(m_timelinePanel, &TimelinePanel::previewTimeChanged, this, [this](float t) {
        m_editor2D->setPreviewTime(t);
    });
    connect(m_editor2D, &SlideEditor2D::elementSelected, m_timelinePanel, &TimelinePanel::setSelectedElement);
    connect(m_editor2D, &SlideEditor2D::keyframeEditDone, this, &EditorArea::keyframeEditDone);
}

void EditorArea::setPresentation(Presentation* pres, int slideIndex) {
    m_pres       = pres;
    m_slideIndex = slideIndex;
    // TimelinePanel must be pointed at the new presentation *before*
    // SlideEditor2D::setSlide() runs: that call synchronously emits
    // elementSelected(-1), which TimelinePanel::setSelectedElement() (wired
    // below) turns into an immediate rebuildRows() — if that happened while
    // m_timelinePanel still referenced the *old* Presentation (which may
    // already have been deleted by the caller, e.g. when opening a different
    // project), rebuildRows() would dereference freed memory.
    m_timelinePanel->setSlide(pres, slideIndex);
    m_editor2D->setSlide(pres, slideIndex);
    m_editor3D->setPresentation(pres, slideIndex);
}

void EditorArea::refresh() {
    m_editor2D->update();
    m_editor3D->update();
    m_timelinePanel->refresh();
}

void EditorArea::activateFormatPainter(const SlideElement& source) {
    m_editor2D->activateFormatPainter(source);
}

void EditorArea::setSelectedElementForTimeline(int elemIndex) {
    m_timelinePanel->setSelectedElement(elemIndex);
}

void EditorArea::setKeyframeEditActive(bool active, const QString& label) {
    m_editor2D->setKeyframeEditActive(active, label);
}

void EditorArea::ensure2DMode() {
    switchTo2D();
}

void EditorArea::selectElement(int index) {
    m_editor2D->selectElement(index);
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
