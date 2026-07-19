#include "SlideEditor2D.h"
#include "ShapeUtils.h"
#include "MeshGradientRenderer.h"
#include "IconUtils.h"
#include "rendering/ChartRenderer.h"
#include "rendering/LatexRenderer.h"
#include "rendering/CodeHighlighter.h"
#include "dialogs/ChartEditorDialog.h"
#include "dialogs/InsertFormulaDialog.h"
#include "dialogs/InsertIFrameDialog.h"
#include "dialogs/InsertButtonDialog.h"
#include "models/VariableEngine.h"
#include "models/TimelineEngine.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QMenu>
#include <QTextLayout>
#include <QTransform>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextFormat>
#include <QFocusEvent>
#include <QTimer>
#include <QUuid>
#include <QImageWriter>
#include <QComboBox>
#include <algorithm>

QVector<SlideElement> SlideEditor2D::s_clipboard;
bool                  SlideEditor2D::s_hasClipboard = false;

// Forward decls (defined further below, near the inline text-edit code) —
// also used by drawElement()'s code-span rendering path.
static void buildLayout(QTextLayout& layout, const SlideElement& e,
                        const QFont& font, float width,
                        const QString& alignOverride = {});
static QFont elemFont(const SlideElement& e, float scaleY);
static float textVOff(const QTextLayout& layout, float elemH, const QString& vAlign);

// Defaults — overridden by m_pres->slideWidth/slideHeight when available
static constexpr float SLIDE_W_DEFAULT = 1920.f;
static constexpr float SLIDE_H_DEFAULT = 1080.f;
static constexpr float HANDLE_R  = 9.f;   // half-size of hit area for handles
static constexpr float HANDLE_V  = 7.f;   // visual half-size of handle squares
static constexpr float MIN_SIZE  = 20.f;  // minimum element size
static constexpr float SNAP_PX   = 10.f;  // snap threshold in screen pixels
static constexpr float MIN_ZOOM  = 0.25f; // 25%
static constexpr float MAX_ZOOM  = 6.0f;  // 600%
static constexpr float ZOOM_STEP = 1.15f; // per wheel notch / toolbar click
static constexpr float RULER_THICKNESS = 20.f; // matches slideRect()'s fit-margin, so the ruler bars exactly cover it
static const QColor GUIDE_COLOR(41, 121, 255);  // persistent guides / circle / measure overlays

// ── Handle index layout ───────────────────────────────────────────────────────
// 0=TL  4=TC  1=TR
// 6=ML        7=MR
// 2=BL  5=BC  3=BR

// Rotate pt around center by angleDeg degrees (clockwise)
static QPointF rotatePt(const QPointF& pt, const QPointF& center, float angleDeg) {
    float rad = float(qDegreesToRadians(double(angleDeg)));
    float c = qCos(rad), s = qSin(rad);
    float x = float(pt.x()) - float(center.x());
    float y = float(pt.y()) - float(center.y());
    return center + QPointF(x * c - y * s, x * s + y * c);
}
// Un-rotate pt around center (inverse rotation)
static QPointF unrotatePt(const QPointF& pt, const QPointF& center, float angleDeg) {
    return rotatePt(pt, center, -angleDeg);
}

static QVector<QPointF> handlePoints(const QRectF& r) {
    return {
        r.topLeft(),
        r.topRight(),
        r.bottomLeft(),
        r.bottomRight(),
        {r.center().x(), r.top()},
        {r.center().x(), r.bottom()},
        {r.left(),  r.center().y()},
        {r.right(), r.center().y()},
    };
}

// Same curated language list as FormatBar's per-selection combo (kept in
// sync manually — see FormatBar.cpp's m_codeLangCombo setup).
static void populateCodeLangItems(QComboBox* combo) {
    combo->addItem("Code: off", QString());
    for (const char* langPair : {"plaintext:Plain code", "javascript:JavaScript", "python:Python",
                                  "cpp:C++", "csharp:C#", "java:Java", "html:HTML", "css:CSS",
                                  "json:JSON", "bash:Bash", "sql:SQL", "php:PHP", "xml:XML"}) {
        QString s = QString::fromLatin1(langPair);
        combo->addItem(s.section(':', 1), s.section(':', 0, 0));
    }
}

// ── Constructor ───────────────────────────────────────────────────────────────

SlideEditor2D::SlideEditor2D(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setBackgroundRole(QPalette::Dark);
    setAutoFillBackground(true);
    setMouseTracking(true);

    setAcceptDrops(true);

    m_cursorBlink = new QTimer(this);
    m_cursorBlink->setInterval(530);
    connect(m_cursorBlink, &QTimer::timeout, this, [this]() {
        m_cursorVisible = !m_cursorVisible;
        if (m_editingElem >= 0) update();
    });

    m_codeLangCombo = new QComboBox(this);
    populateCodeLangItems(m_codeLangCombo);
    m_codeLangCombo->setFixedWidth(110);
    m_codeLangCombo->setToolTip("This code block's language");
    m_codeLangCombo->setStyleSheet(
        "QComboBox { background:#ffffff; color:#111827; border:1px solid #9ca3af; "
        "            padding:1px 4px; font-size:11px; }");
    m_codeLangCombo->hide();
    connect(m_codeLangCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SlideEditor2D::onCodeLangOverlayChanged);
}

// ── Public API ────────────────────────────────────────────────────────────────

void SlideEditor2D::setSlide(Presentation* pres, int slideIndex) {
    finishTextEdit();
    exitTableEditMode();
    if (pres != m_pres) m_pixmapCache.clear();
    m_pres           = pres;
    m_slideIndex     = slideIndex;
    m_selectedElem   = -1;
    m_selectedElems.clear();
    m_resizingHandle = -1;
    m_dragDivider    = {};
    update();
    emit elementSelected(-1);
    emit elementsSelected(m_selectedElems);
}

void SlideEditor2D::setPreviewTime(float tSeconds) {
    m_previewTime = tSeconds;
    update();
}

void SlideEditor2D::setKeyframeEditActive(bool active, const QString& label) {
    m_keyframeEditActive = active;
    m_keyframeEditLabel  = label;
    update();
}

void SlideEditor2D::selectElement(int index) {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || index < -1 || index >= s->elements.size()) return;
    selectWithGroupExpansion(index);
}

// ── Multi-selection / grouping helpers ───────────────────────────────────────

QVector<int> SlideEditor2D::groupMembers(int elemIndex) const {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    QVector<int> result;
    if (!s || elemIndex < 0 || elemIndex >= s->elements.size()) return result;
    const QString& gid = s->elements[elemIndex].groupId;
    if (gid.isEmpty()) { result.append(elemIndex); return result; }
    for (int i = 0; i < s->elements.size(); ++i)
        if (s->elements[i].groupId == gid) result.append(i);
    return result;
}

void SlideEditor2D::setSingleSelection(int index) {
    m_selectedElem = index;
    m_selectedElems.clear();
    if (index >= 0) m_selectedElems.append(index);
}

void SlideEditor2D::selectWithGroupExpansion(int hitIndex) {
    if (hitIndex < 0) {
        m_selectedElem = -1;
        m_selectedElems.clear();
    } else {
        m_selectedElem  = hitIndex;
        m_selectedElems = groupMembers(hitIndex);
    }
    update();
    emit elementSelected(m_selectedElems.size() == 1 ? m_selectedElem : -1);
    emit elementsSelected(m_selectedElems);
}

QRectF SlideEditor2D::selectionWidgetRect() const {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    QRectF result;
    if (!s) return result;
    for (int idx : m_selectedElems) {
        if (idx < 0 || idx >= s->elements.size()) continue;
        QRectF wr = elemToWidget(s->elements[idx]);
        result = result.isNull() ? wr : result.united(wr);
    }
    return result;
}

QRectF SlideEditor2D::selectionSlideRect() const {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    QRectF result;
    if (!s) return result;
    for (int idx : m_selectedElems) {
        if (idx < 0 || idx >= s->elements.size()) continue;
        const SlideElement& e = s->elements[idx];
        QRectF r(e.x, e.y, e.width, e.height);
        result = result.isNull() ? r : result.united(r);
    }
    return result;
}

// ── Coordinate helpers ────────────────────────────────────────────────────────

QRectF SlideEditor2D::slideRect() const {
    const float margin = 20.f;
    const float aw = width()  - 2 * margin;
    const float ah = height() - 2 * margin;
    const float scale = qMin(aw / SLIDE_W_DEFAULT, ah / SLIDE_H_DEFAULT) * m_zoom;
    const float sw = SLIDE_W_DEFAULT * scale;
    const float sh = SLIDE_H_DEFAULT * scale;
    return QRectF((width()  - sw) / 2.f + m_panOffset.x(),
                  (height() - sh) / 2.f + m_panOffset.y(), sw, sh);
}

// ── Zoom ──────────────────────────────────────────────────────────────────────

void SlideEditor2D::setZoom(float newZoom, const QPointF& anchorWidgetPos) {
    newZoom = qBound(MIN_ZOOM, newZoom, MAX_ZOOM);
    if (qFuzzyCompare(newZoom, m_zoom)) return;

    // Keep the slide point currently under anchorWidgetPos fixed on screen.
    QPointF slidePt = widgetToSlide(anchorWidgetPos);
    m_zoom = newZoom;
    QRectF sr = slideRect();
    float sx = sr.width()  / SLIDE_W_DEFAULT;
    float sy = sr.height() / SLIDE_H_DEFAULT;
    QPointF newWidgetPos(sr.x() + slidePt.x() * sx, sr.y() + slidePt.y() * sy);
    m_panOffset += anchorWidgetPos - newWidgetPos;

    emit zoomChanged(m_zoom);
    update();
}

void SlideEditor2D::zoomIn() {
    setZoom(m_zoom * ZOOM_STEP, QPointF(width() / 2.0, height() / 2.0));
}

void SlideEditor2D::zoomOut() {
    setZoom(m_zoom / ZOOM_STEP, QPointF(width() / 2.0, height() / 2.0));
}

void SlideEditor2D::zoomReset() {
    m_zoom      = 1.0f;
    m_panOffset = {0, 0};
    emit zoomChanged(m_zoom);
    update();
}

void SlideEditor2D::setZoomPercent(int percent) {
    setZoom(float(percent) / 100.f, QPointF(width() / 2.0, height() / 2.0));
}

QRectF SlideEditor2D::elemToWidget(const SlideElement& e) const {
    QRectF sr = slideRect();
    const float sx = sr.width()  / SLIDE_W_DEFAULT;
    const float sy = sr.height() / SLIDE_H_DEFAULT;
    return QRectF(sr.x() + e.x * sx, sr.y() + e.y * sy,
                  e.width * sx, e.height * sy);
}

QPointF SlideEditor2D::widgetToSlide(const QPointF& wp) const {
    QRectF sr = slideRect();
    return QPointF((wp.x() - sr.x()) * SLIDE_W_DEFAULT / sr.width(),
                   (wp.y() - sr.y()) * SLIDE_H_DEFAULT / sr.height());
}

int SlideEditor2D::hitTest(const QPointF& wpos) const {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return -1;
    for (int i = s->elements.size() - 1; i >= 0; --i) {
        const SlideElement& elem = s->elements[i];
        QRectF wr = elemToWidget(elem);
        // Un-rotate the click point into the element's local coordinate space
        QPointF testPos = (elem.rotation != 0.f)
            ? unrotatePt(wpos, wr.center(), elem.rotation)
            : wpos;
        if (wr.contains(testPos)) return i;
    }
    return -1;
}

int SlideEditor2D::hitHandle(const QPointF& wpos) const {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElems.isEmpty()) return -1;

    QRectF wr;
    float  rot = 0.f;

    if (m_selectedElems.size() == 1) {
        int idx = m_selectedElems.first();
        if (idx < 0 || idx >= s->elements.size()) return -1;
        const SlideElement& elem = s->elements[idx];
        wr  = elemToWidget(elem);
        rot = elem.rotation;

        // Check rotation handle first: circle 30px above TC in local space, then rotated.
        // Only offered for a single selected element — group rotation isn't supported.
        QPointF localRotHandle(wr.center().x(), wr.top() - 30);
        QPointF rotHandleW = (rot != 0.f) ? rotatePt(localRotHandle, wr.center(), rot)
                                           : localRotHandle;
        QRectF rhr(rotHandleW.x() - HANDLE_R, rotHandleW.y() - HANDLE_R,
                   HANDLE_R * 2, HANDLE_R * 2);
        if (rhr.contains(wpos)) return 8;
    } else {
        wr = selectionWidgetRect();
        if (wr.isNull()) return -1;
    }

    // Resize handles: un-rotate click into local space first (rot==0 for multi-selection)
    QPointF testPos = (rot != 0.f) ? unrotatePt(wpos, wr.center(), rot) : wpos;
    const auto pts = handlePoints(wr);
    for (int i = 0; i < pts.size(); ++i) {
        QRectF hr(pts[i].x() - HANDLE_R, pts[i].y() - HANDLE_R,
                  HANDLE_R * 2, HANDLE_R * 2);
        if (hr.contains(testPos)) return i;
    }
    return -1;
}

void SlideEditor2D::applyResize(SlideElement& e, int handle,
                                 const QPointF& cur,
                                 float ox, float oy, float ow, float oh,
                                 bool constrain) const {
    float cx = float(cur.x()), cy = float(cur.y());
    float right  = ox + ow;
    float bottom = oy + oh;

    // Store draft values
    float nx = ox, ny = oy, nw = ow, nh = oh;

    switch (handle) {
        case 0: nx = cx; nw = right  - cx; ny = cy; nh = bottom - cy; break; // TL
        case 1:          nw = cx     - ox; ny = cy; nh = bottom - cy; break; // TR
        case 2: nx = cx; nw = right  - cx;          nh = cy    - oy; break; // BL
        case 3:          nw = cx     - ox;           nh = cy    - oy; break; // BR
        case 4:                             ny = cy; nh = bottom - cy; break; // TC
        case 5:                                      nh = cy    - oy; break; // BC
        case 6: nx = cx; nw = right  - cx;                            break; // ML
        case 7:          nw = cx     - ox;                            break; // MR
    }

    // Proportional resize (Ctrl held) — only for corner handles
    if (constrain && ow > 0 && oh > 0 && handle <= 3) {
        float aspect = ow / oh;
        float sw = nw / ow, sh = nh / oh;
        float scale = (qAbs(sw - 1.f) >= qAbs(sh - 1.f)) ? sw : sh;
        if (scale < MIN_SIZE / ow) scale = MIN_SIZE / ow;
        nw = ow * scale;
        nh = oh * scale;
        // re-anchor opposite corner
        if (handle == 0) { nx = ox + ow - nw; ny = oy + oh - nh; } // TL → anchor BR
        if (handle == 1) {                     ny = oy + oh - nh; } // TR → anchor BL
        if (handle == 2) { nx = ox + ow - nw;                    } // BL → anchor TR
        // BR (3): anchor TL, nx/ny unchanged
        Q_UNUSED(aspect);
    }

    // Clamp to minimum size
    if (nw < MIN_SIZE) {
        if (handle == 0 || handle == 2 || handle == 6) nx = ox + ow - MIN_SIZE;
        nw = MIN_SIZE;
    }
    if (nh < MIN_SIZE) {
        if (handle == 0 || handle == 1 || handle == 4) ny = oy + oh - MIN_SIZE;
        nh = MIN_SIZE;
    }

    e.x = nx; e.y = ny; e.width = nw; e.height = nh;
}

// ── Focus out: finish text edit ───────────────────────────────────────────────

void SlideEditor2D::focusOutEvent(QFocusEvent*) {
    if (m_editingElem >= 0) finishTextEdit();
}

// ── Snap / alignment guides ───────────────────────────────────────────────────

void SlideEditor2D::applySnapAndGuides(SlideElement& e) {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;

    float sw = (m_pres && m_pres->slideWidth  > 0) ? m_pres->slideWidth  : SLIDE_W_DEFAULT;
    float sh = (m_pres && m_pres->slideHeight > 0) ? m_pres->slideHeight : SLIDE_H_DEFAULT;

    QRectF sr = slideRect();
    float threshX = SNAP_PX * SLIDE_W_DEFAULT / float(sr.width());
    float threshY = SNAP_PX * SLIDE_H_DEFAULT / float(sr.height());

    m_snapGuides.clear();

    // Build candidate snap lines from slide bounds + other elements
    QVector<float> xCands = {0.f, sw * 0.5f, sw};
    QVector<float> yCands = {0.f, sh * 0.5f, sh};
    for (int i = 0; i < s->elements.size(); ++i) {
        if (m_selectedElems.contains(i)) continue;
        const SlideElement& o = s->elements[i];
        xCands << o.x << (o.x + o.width  * 0.5f) << (o.x + o.width);
        yCands << o.y << (o.y + o.height * 0.5f) << (o.y + o.height);
    }
    // Persistent ruler guides are snap targets too, same as slide bounds/elements.
    for (const GuideLine& g : s->guides) {
        if (g.vertical) xCands << g.pos; else yCands << g.pos;
    }

    // Find the globally closest anchor×candidate pair for X
    struct XSnap { float pt; float offset; };
    const XSnap axs[3] = {
        {e.x,                  0.f          },  // left edge
        {e.x + e.width * .5f, -e.width*.5f  },  // center
        {e.x + e.width,       -e.width      },  // right edge
    };
    bool  snapX = false;
    float bestXDist = threshX, bestXNewX = e.x, bestXGuide = 0;
    for (const XSnap& a : axs) {
        for (float c : xCands) {
            float d = qAbs(a.pt - c);
            if (d < bestXDist) {
                bestXDist = d; bestXNewX = c + a.offset; bestXGuide = c; snapX = true;
            }
        }
    }
    if (snapX) { e.x = bestXNewX; m_snapGuides.append({true, bestXGuide}); }

    // Same for Y — globally closest anchor×candidate pair
    struct YSnap { float pt; float offset; };
    const YSnap ays[3] = {
        {e.y,                   0.f           },
        {e.y + e.height * .5f, -e.height*.5f  },
        {e.y + e.height,       -e.height      },
    };
    bool  snapY = false;
    float bestYDist = threshY, bestYNewY = e.y, bestYGuide = 0;
    for (const YSnap& a : ays) {
        for (float c : yCands) {
            float d = qAbs(a.pt - c);
            if (d < bestYDist) {
                bestYDist = d; bestYNewY = c + a.offset; bestYGuide = c; snapY = true;
            }
        }
    }
    if (snapY) { e.y = bestYNewY; m_snapGuides.append({false, bestYGuide}); }
}

float SlideEditor2D::snapGuidePos(bool vertical, float pos) const {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return pos;

    QRectF sr = slideRect();
    float thresh = vertical ? SNAP_PX * SLIDE_W_DEFAULT / float(sr.width())
                             : SNAP_PX * SLIDE_H_DEFAULT / float(sr.height());
    float sw = (m_pres->slideWidth  > 0) ? m_pres->slideWidth  : SLIDE_W_DEFAULT;
    float sh = (m_pres->slideHeight > 0) ? m_pres->slideHeight : SLIDE_H_DEFAULT;

    QVector<float> cands = vertical ? QVector<float>{0.f, sw * 0.5f, sw}
                                     : QVector<float>{0.f, sh * 0.5f, sh};
    for (const SlideElement& o : s->elements) {
        if (vertical) cands << o.x << (o.x + o.width  * 0.5f) << (o.x + o.width);
        else          cands << o.y << (o.y + o.height * 0.5f) << (o.y + o.height);
    }

    float best = pos, bestD = thresh;
    for (float c : cands) {
        float d = qAbs(c - pos);
        if (d < bestD) { bestD = d; best = c; }
    }
    return best;
}

// ── Rulers & persistent guides ────────────────────────────────────────────────

void SlideEditor2D::setRulersVisible(bool visible) {
    m_showRulers = visible;
    update();
}

bool SlideEditor2D::inTopRulerBand(const QPointF& wp) const {
    return m_showRulers && wp.y() >= 0 && wp.y() < RULER_THICKNESS && wp.x() >= 0 && wp.x() < width();
}

bool SlideEditor2D::inLeftRulerBand(const QPointF& wp) const {
    return m_showRulers && wp.x() >= 0 && wp.x() < RULER_THICKNESS && wp.y() >= 0 && wp.y() < height();
}

int SlideEditor2D::hitGuide(const QPointF& wp, bool& vertical) const {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || s->guides.isEmpty()) return -1;

    QRectF sr = slideRect();
    const float thresh = 5.f;
    for (int i = 0; i < s->guides.size(); ++i) {
        const GuideLine& g = s->guides[i];
        if (g.vertical) {
            float wx = float(sr.x() + g.pos * sr.width() / SLIDE_W_DEFAULT);
            if (qAbs(wp.x() - wx) <= thresh && wp.y() >= sr.top() - 2 && wp.y() <= sr.bottom() + 2) {
                vertical = true;
                return i;
            }
        } else {
            float wy = float(sr.y() + g.pos * sr.height() / SLIDE_H_DEFAULT);
            if (qAbs(wp.y() - wy) <= thresh && wp.x() >= sr.left() - 2 && wp.x() <= sr.right() + 2) {
                vertical = false;
                return i;
            }
        }
    }
    return -1;
}

void SlideEditor2D::finalizeGuideDrag() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    m_draggingGuide = false;
    if (!s) return;

    // Dropped back onto the ruler band it came from → delete (or, for a
    // brand-new drag that never left the band, simply never create it).
    bool droppedOnRuler = m_guideVertical ? inLeftRulerBand(m_guideDragPos)
                                           : inTopRulerBand(m_guideDragPos);
    if (droppedOnRuler) {
        if (!m_newGuide && m_guideIndex >= 0 && m_guideIndex < s->guides.size())
            s->guides.remove(m_guideIndex);
        return;
    }

    QPointF slidePt = widgetToSlide(m_guideDragPos);
    float newPos = m_guideVertical ? float(slidePt.x()) : float(slidePt.y());
    if (m_snapNewGuideToObjects)
        newPos = snapGuidePos(m_guideVertical, newPos);

    if (m_newGuide) {
        GuideLine g; g.vertical = m_guideVertical; g.pos = newPos;
        s->guides.append(g);
    } else if (m_guideIndex >= 0 && m_guideIndex < s->guides.size()) {
        s->guides[m_guideIndex].pos = newPos;
    }
}

// Picks a "nice" tick spacing (in slide-space units) so labels stay >= ~60px apart on screen.
static float pickNiceRulerStep(float pxPerUnit) {
    static const float steps[] = {5.f, 10.f, 20.f, 25.f, 50.f, 100.f, 200.f, 250.f, 500.f, 1000.f, 2000.f};
    for (float s : steps)
        if (s * pxPerUnit >= 60.f) return s;
    return 2000.f;
}

void SlideEditor2D::drawRulers(QPainter& p) const {
    if (!m_showRulers) return;

    QRectF sr = slideRect();
    const float T = RULER_THICKNESS;

    p.fillRect(QRectF(0, 0, width(), T), QColor(250, 250, 250));
    p.fillRect(QRectF(0, 0, T, height()), QColor(250, 250, 250));
    p.fillRect(QRectF(0, 0, T, T), QColor(235, 235, 235));

    p.setPen(QPen(QColor(150, 150, 150), 1));
    p.drawLine(QPointF(0, T), QPointF(width(), T));
    p.drawLine(QPointF(T, 0), QPointF(T, height()));

    QFont f = p.font();
    f.setPointSize(7);
    p.setFont(f);
    p.setPen(QColor(70, 70, 70));

    float scaleX = float(sr.width())  / SLIDE_W_DEFAULT;
    float scaleY = float(sr.height()) / SLIDE_H_DEFAULT;

    // Horizontal (top) ruler — slide-space X ticks
    if (scaleX > 0.0001f) {
        float step = pickNiceRulerStep(scaleX);
        float firstX = std::floor((T - sr.x()) / scaleX / step) * step;
        for (float sx = firstX; ; sx += step) {
            float wx = float(sr.x() + sx * scaleX);
            if (wx > width()) break;
            if (wx < T) continue;
            p.drawLine(QPointF(wx, T - 6), QPointF(wx, T));
            p.drawText(QPointF(wx + 2, T - 8), QString::number(int(sx)));
        }
    }

    // Vertical (left) ruler — slide-space Y ticks
    if (scaleY > 0.0001f) {
        float step = pickNiceRulerStep(scaleY);
        float firstY = std::floor((T - sr.y()) / scaleY / step) * step;
        for (float sy = firstY; ; sy += step) {
            float wy = float(sr.y() + sy * scaleY);
            if (wy > height()) break;
            if (wy < T) continue;
            p.drawLine(QPointF(T - 6, wy), QPointF(T, wy));
            p.drawText(QRectF(0, wy - 7, T - 7, 14), Qt::AlignRight | Qt::AlignVCenter, QString::number(int(sy)));
        }
    }
}

// ── Painting ─────────────────────────────────────────────────────────────────

static void drawCheckerboard(QPainter& p, const QRectF& r) {
    const int cell = 16;
    for (int row = 0; row * cell < int(r.height()) + cell; ++row) {
        for (int col = 0; col * cell < int(r.width()) + cell; ++col) {
            QColor c = ((row + col) % 2 == 0) ? QColor(200, 200, 200) : Qt::white;
            QRectF cr(r.x() + col * cell, r.y() + row * cell, cell, cell);
            p.fillRect(cr.intersected(r), c);
        }
    }
}

void SlideEditor2D::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Scene background (behind slide)
    QColor sceneBg = (m_pres && m_pres->sceneBackground.isValid())
                     ? m_pres->sceneBackground : QColor(60, 60, 60);
    p.fillRect(rect(), sceneBg);

    const Slide* slide = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!slide) return;

    QRectF sr = slideRect();
    float scaleX = float(sr.width())  / SLIDE_W_DEFAULT;
    float scaleY = float(sr.height()) / SLIDE_H_DEFAULT;

    // Drop shadow
    p.fillRect(sr.adjusted(4, 4, 4, 4), QColor(0, 0, 0, 100));

    // Slide background
    bool isTransparent = !slide->backgroundColor.isValid()
                         || slide->backgroundColor == Qt::transparent
                         || slide->backgroundColor.alpha() == 0;
    if (isTransparent)
        drawCheckerboard(p, sr);
    else
        p.fillRect(sr, slide->backgroundColor);

    p.setPen(QPen(QColor(180, 180, 180), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(sr);

    // Elements (clipped to slide)
    p.setClipRect(sr);
    for (int i = 0; i < slide->elements.size(); ++i) {
        bool showSel = m_selectedElems.contains(i) && (i != m_editingElem);
        const SlideElement& baseElem = slide->elements[i];
        SlideElement previewElem;
        const SlideElement* renderElem = &baseElem;
        if (m_previewTime >= 0.f && !baseElem.timeline.isDefault()) {
            previewElem = TimelineEngine::previewAt(baseElem, baseElem.timeline, m_previewTime);
            renderElem = &previewElem;
        }
        p.setOpacity(double(renderElem->opacity));
        drawElement(p, *renderElem, showSel, i == m_editingElem, slide->id);
        p.setOpacity(1.0);
        if (i == m_editingElem)
            drawTextCursor(p, slide->elements[i]);
    }
    p.setClipping(false);

    // Snap guides (drawn over the slide, no clipping)
    if (!m_snapGuides.isEmpty()) {
        p.setPen(QPen(QColor(0, 200, 255, 210), 1, Qt::DashLine));
        for (const SnapGuide& g : m_snapGuides) {
            if (g.vertical) {
                float wx = float(sr.x() + g.pos * sr.width() / SLIDE_W_DEFAULT);
                p.drawLine(QPointF(wx, sr.top()), QPointF(wx, sr.bottom()));
            } else {
                float wy = float(sr.y() + g.pos * sr.height() / SLIDE_H_DEFAULT);
                p.drawLine(QPointF(sr.left(), wy), QPointF(sr.right(), wy));
            }
        }
    }

    // Persistent ruler guides (solid — distinct from the dashed temporary snap guides above)
    if (!slide->guides.isEmpty() || m_draggingGuide) {
        p.setPen(QPen(GUIDE_COLOR, 1, Qt::SolidLine));
        for (int i = 0; i < slide->guides.size(); ++i) {
            if (m_draggingGuide && !m_newGuide && i == m_guideIndex) continue; // drawn live below instead
            const GuideLine& g = slide->guides[i];
            if (g.vertical) {
                float wx = float(sr.x() + g.pos * sr.width() / SLIDE_W_DEFAULT);
                p.drawLine(QPointF(wx, sr.top()), QPointF(wx, sr.bottom()));
            } else {
                float wy = float(sr.y() + g.pos * sr.height() / SLIDE_H_DEFAULT);
                p.drawLine(QPointF(sr.left(), wy), QPointF(sr.right(), wy));
            }
        }
        if (m_draggingGuide) {
            // Red = will be deleted on release (dragged back onto the ruler it came from)
            bool willDelete = m_guideVertical ? inLeftRulerBand(m_guideDragPos) : inTopRulerBand(m_guideDragPos);
            p.setPen(QPen(willDelete ? QColor(220, 50, 50) : GUIDE_COLOR, 1, Qt::SolidLine));
            if (m_guideVertical)
                p.drawLine(QPointF(m_guideDragPos.x(), sr.top()), QPointF(m_guideDragPos.x(), sr.bottom()));
            else
                p.drawLine(QPointF(sr.left(), m_guideDragPos.y()), QPointF(sr.right(), m_guideDragPos.y()));
        }
    }

    // Circle guides ("Kreis/Zirkel" tool)
    if (!slide->guideCircles.isEmpty() || m_drawingCircle) {
        p.setPen(QPen(GUIDE_COLOR, 1, Qt::SolidLine));
        p.setBrush(Qt::NoBrush);
        for (const GuideCircle& c : slide->guideCircles) {
            QPointF centerW(sr.x() + c.cx * scaleX, sr.y() + c.cy * scaleY);
            p.drawEllipse(centerW, c.radius * scaleX, c.radius * scaleY);
        }
        if (m_drawingCircle) {
            p.setPen(QPen(GUIDE_COLOR, 1, Qt::DashLine));
            p.drawEllipse(m_circleCenterWidget, m_circleRadiusWidget, m_circleRadiusWidget);
            QString label = QString("r = %1").arg(int(m_circleRadiusWidget / qMax(0.0001f, scaleX)));
            p.setPen(GUIDE_COLOR);
            p.drawText(m_circleCenterWidget + QPointF(6, -6), label);
        }
    }

    // Measure tool ("Messgerät") — ad-hoc two-point distance/angle readout
    if (m_measuring || m_hasMeasureResult) {
        QPointF a = m_measureStartWidget, b = m_measureEndWidget;
        p.setPen(QPen(QColor(255, 140, 0), 2));
        p.drawLine(a, b);
        QPointF sa = widgetToSlide(a), sb = widgetToSlide(b);
        float dx = float(sb.x() - sa.x()), dy = float(sb.y() - sa.y());
        float dist = std::sqrt(dx * dx + dy * dy);
        float angle = float(qRadiansToDegrees(std::atan2(dy, dx)));
        QString label = QString("%1 px, %2°").arg(int(dist)).arg(int(angle));
        QPointF mid = (a + b) / 2.0;
        QFontMetrics fm(p.font());
        QRectF labelRect(mid.x() + 6, mid.y() - fm.height() - 4, fm.horizontalAdvance(label) + 8, fm.height() + 4);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 160));
        p.drawRoundedRect(labelRect, 3, 3);
        p.setPen(Qt::white);
        p.drawText(labelRect, Qt::AlignCenter, label);
    }

    // Selection handles (drawn outside clip so they extend beyond slide edge)
    if (m_selectedElems.size() == 1) {
        int idx = m_selectedElems.first();
        if (idx >= 0 && idx < slide->elements.size())
            drawHandles(p, elemToWidget(slide->elements[idx]), slide->elements[idx].rotation);
    } else if (m_selectedElems.size() > 1) {
        drawGroupHandles(p, selectionWidgetRect());
    }

    // Marquee (rubber-band) selection rectangle
    if (m_marqueeActive) {
        p.setPen(QPen(QColor(0, 120, 215), 1, Qt::DashLine));
        p.setBrush(QColor(0, 120, 215, 40));
        p.drawRect(m_marqueeRect);
    }

    // Zoom indicator (only shown once the user has actually zoomed)
    if (!qFuzzyCompare(m_zoom, 1.0f)) {
        QString label = QString("%1%").arg(qRound(m_zoom * 100.f));
        QFont f = p.font();
        f.setPointSize(9);
        p.setFont(f);
        QFontMetrics fm(f);
        QRectF badge(width() - fm.horizontalAdvance(label) - 18, height() - 28, fm.horizontalAdvance(label) + 10, 20);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 140));
        p.drawRoundedRect(badge, 4, 4);
        p.setPen(Qt::white);
        p.drawText(badge, Qt::AlignCenter, label);
    }

    drawRulers(p);

    if (m_keyframeEditActive) {
        QRectF banner(0, 0, width(), 30);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(230, 140, 20, 235));
        p.drawRect(banner);
        QFont f = p.font();
        f.setPointSize(10);
        f.setBold(true);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(banner.adjusted(10, 0, -100, 0), Qt::AlignVCenter | Qt::AlignLeft, m_keyframeEditLabel);
        m_keyframeDoneRect = QRectF(width() - 90, 4, 80, 22);
        p.setBrush(QColor(0, 0, 0, 120));
        p.drawRoundedRect(m_keyframeDoneRect, 4, 4);
        p.drawText(m_keyframeDoneRect, Qt::AlignCenter, "Done (Esc)");
    }

    updateCodeLangOverlay();
}

int SlideEditor2D::slideNumberFor(const QString& currentSlideId) const {
    if (!m_pres) return 0;
    for (int i = 0; i < m_pres->slides.size(); ++i)
        if (m_pres->slides[i].id == currentSlideId) return i + 1;
    return 0;
}

QString SlideEditor2D::substituteVars(const QString& raw, const QString& currentSlideId) const {
    if (!m_pres) return raw;
    return VariableEngine::substitute(raw, m_pres->variables, currentSlideId,
                                       slideNumberFor(currentSlideId), m_pres->slides.size());
}

void SlideEditor2D::drawElement(QPainter& p, const SlideElement& e, bool selected, bool isBeingEdited,
                                 const QString& currentSlideId) const {
    QRectF wr = elemToWidget(e);
    QRectF sr = slideRect();
    float scaleY = sr.height() / SLIDE_H_DEFAULT;

    if (e.type == SlideElement::Text) {
        if (e.backgroundColor.isValid() && e.backgroundColor != Qt::transparent)
            p.fillRect(wr, e.backgroundColor);

        if (wr.width() < 1.0 || wr.height() < 1.0) return;

        bool hasCode = !e.codeSpans.isEmpty() && e.listStyle == SlideElement::NoList;
        QString displayText = (isBeingEdited || hasCode) ? e.content : substituteVars(e.content, currentSlideId);
        if (e.listStyle != SlideElement::NoList) {
            QStringList lines = displayText.split('\n');
            QStringList fmt;
            for (int ln = 0; ln < lines.size(); ++ln)
                fmt << (e.listStyle == SlideElement::Bullets
                        ? QString("• ") + lines[ln]
                        : QString::number(ln + 1) + ". " + lines[ln]);
            displayText = fmt.join('\n');
        }

        QFont font(e.fontFamily, qMax(6, int(e.fontSize * scaleY)));
        font.setBold(e.bold);
        font.setItalic(e.italic);
        font.setUnderline(e.underline || !e.hyperlink.trimmed().isEmpty());
        font.setStrikeOut(e.strikethrough);

        p.save();
        p.setFont(font);
        p.setPen(e.color.isValid() ? e.color : Qt::black);
        p.setClipRect(wr, Qt::IntersectClip);
        if (hasCode) {
            // Render code spans with a monospace font + subtle background tint,
            // plus real per-token colors from CodeHighlighter — via
            // QTextLayout::FormatRange. Overlapping ranges are merged by Qt
            // (later entries only override the properties they set), so the
            // span-wide range below supplies font/background and the narrower
            // token ranges on top just add a foreground color.
            QTextLayout layout(displayText, font);
            QVector<QTextLayout::FormatRange> ranges;
            for (const CodeSpan& sp : e.codeSpans) {
                int start = qBound(0, sp.start, displayText.length());
                int len   = qBound(0, sp.length, displayText.length() - start);
                if (len <= 0) continue;
                QTextCharFormat fmt;
                fmt.setFontFamilies({"Consolas", "monospace"});
                fmt.setBackground(QColor(128, 128, 128, 60));
                ranges << QTextLayout::FormatRange{start, len, fmt};

                for (const auto& tok : CodeHighlighter::tokenize(displayText.mid(start, len), sp.language)) {
                    QColor col = CodeHighlighter::colorForToken(tok.kind);
                    if (!col.isValid()) continue;
                    int tStart = qBound(0, start + tok.start, displayText.length());
                    int tLen   = qBound(0, tok.length, displayText.length() - tStart);
                    if (tLen <= 0) continue;
                    QTextCharFormat tfmt;
                    tfmt.setForeground(col);
                    ranges << QTextLayout::FormatRange{tStart, tLen, tfmt};
                }
            }
            // Formats must be set before beginLayout()/createLine() (done inside
            // buildLayout) — setting them afterward invalidates the already-built
            // line layout, leaving lineCount() at 0 and drawing nothing.
            layout.setFormats(ranges);
            buildLayout(layout, e, font, float(wr.width()), e.textAlignment);
            float yOff = textVOff(layout, float(wr.height()), e.verticalAlignment);
            layout.draw(&p, wr.topLeft() + QPointF(0, yOff));
        } else {
            // QTextLayout (not QPainter::drawText) so line breaks/positions are
            // pixel-identical to textPositionAt()/drawTextCursor()'s hit-testing
            // layout — drawText's own wrapping could disagree by a line or two
            // on long multi-line text, making clicks land on the wrong line.
            QTextLayout layout(displayText, font);
            buildLayout(layout, e, font, float(wr.width()));
            float vOff = textVOff(layout, float(wr.height()), e.verticalAlignment);
            layout.draw(&p, wr.topLeft() + QPointF(0, vOff));
        }
        p.restore();

    } else if (e.type == SlideElement::Shape) {
        bool hasRot = (e.rotation != 0.f);
        if (hasRot) {
            p.save();
            p.translate(wr.center());
            p.rotate(double(e.rotation));
            p.translate(-wr.center());
        }

        p.setPen(e.borderWidth > 0
                 ? QPen(e.borderColor.isValid() ? e.borderColor : Qt::darkGray, e.borderWidth)
                 : Qt::NoPen);
        {
            QRectF sr2 = slideRect();
            float rx = e.cornerRadius * sr2.width()  / SLIDE_W_DEFAULT;
            float ry = e.cornerRadius * sr2.height() / SLIDE_H_DEFAULT;

            if (e.useMeshGradient && e.meshGradient.isUsable() && e.content != "line") {
                QImage meshImg = MeshGradientRenderer::renderMeshGradient(
                    e.content, wr.size().toSize(), e.meshGradient, QSizeF(rx, ry));
                p.drawImage(wr.topLeft(), meshImg);
                p.setBrush(Qt::NoBrush);
            } else {
                p.setBrush(e.backgroundColor == Qt::transparent ? Qt::NoBrush : QBrush(e.backgroundColor));
            }

            if (e.content == "line") {
                p.drawLine(wr.topLeft(), wr.bottomRight());
            } else if (e.content == "rect") {
                if (rx > 0 || ry > 0) p.drawRoundedRect(wr, rx, ry);
                else                   p.drawRect(wr);
            } else {
                p.drawPath(ShapeUtils::shapeToPath(e.content, wr));
            }
        }

        // Draw text overlaid inside the shape
        if (!e.shapeText.isEmpty()) {
            QString shapeDisplayText = isBeingEdited ? e.shapeText : substituteVars(e.shapeText, currentSlideId);
            QFont font(e.fontFamily, qMax(6, int(e.fontSize * scaleY)));
            font.setBold(e.bold);
            font.setItalic(e.italic);
            p.save();
            p.setFont(font);
            p.setPen(e.color.isValid() ? e.color : Qt::white);
            p.setClipRect(wr, Qt::IntersectClip);
            p.drawText(wr, Qt::AlignCenter | Qt::TextWordWrap, shapeDisplayText);
            p.restore();
        }

        // Selection outline and restore rotation transform
        if (selected) {
            p.setPen(QPen(QColor(0, 120, 215), 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(wr);
        }
        if (hasRot) p.restore();
        return;

    } else if (e.type == SlideElement::Icon) {
        bool hasRot = (e.rotation != 0.f);
        if (hasRot) {
            p.save();
            p.translate(wr.center());
            p.rotate(double(e.rotation));
            p.translate(-wr.center());
        }
        p.setPen(Qt::NoPen);
        p.setBrush(e.color.isValid() ? e.color : Qt::black);
        p.drawPath(IconUtils::iconToPath(e.content, wr));

        if (selected) {
            p.setPen(QPen(QColor(0, 120, 215), 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(wr);
        }
        if (hasRot) p.restore();
        return;

    } else if (e.type == SlideElement::Table) {
        drawTableElement(p, e, selected, currentSlideId);
        return; // table draws its own selection outline

    } else if (e.type == SlideElement::Image) {
        bool drawn = false;
        if (!e.content.isEmpty()) {
            if (!m_pixmapCache.contains(e.content))
                m_pixmapCache[e.content] = QPixmap(e.content);
            const QPixmap& px = m_pixmapCache[e.content];
            if (!px.isNull()) { p.drawPixmap(wr.toRect(), px); drawn = true; }
        }
        if (!drawn) {
            p.fillRect(wr, QColor(180, 180, 200));
            p.setPen(Qt::darkGray);
            p.drawText(wr, Qt::AlignCenter,
                       e.content.isEmpty() ? "[Image]" : QFileInfo(e.content).fileName());
        }

    } else if (e.type == SlideElement::Chart) {
        p.fillRect(wr, Qt::white);
        ChartRenderer::paint(p, wr, e.chartData, m_pres ? &m_pres->variables : nullptr, currentSlideId,
                             slideNumberFor(currentSlideId), m_pres ? m_pres->slides.size() : 0);
        p.setPen(QPen(QColor(200, 200, 200), 0.5));
        p.setBrush(Qt::NoBrush);
        p.drawRect(wr);
        if (selected) {
            p.save();
            p.setPen(QColor(37, 99, 235, 200));
            p.setFont(QFont("Arial", qMax(6, int(9 * sr.height() / SLIDE_H_DEFAULT))));
            p.drawText(QRectF(wr.x(), wr.bottom() - 18 * sr.height()/SLIDE_H_DEFAULT,
                              wr.width(), 18 * sr.height()/SLIDE_H_DEFAULT),
                       Qt::AlignCenter, "Double-click to edit");
            p.restore();
        }

    } else if (e.type == SlideElement::Formula) {
        if (e.backgroundColor.isValid() && e.backgroundColor != Qt::transparent)
            p.fillRect(wr, e.backgroundColor);

        if (e.content.trimmed().isEmpty()) {
            p.save();
            p.setPen(QColor(150, 150, 150));
            p.setFont(QFont(e.fontFamily, qMax(6, int(9 * scaleY))));
            p.drawText(wr, Qt::AlignCenter, "∑ Formula");
            p.restore();
        } else {
            LatexRenderer::paint(p, wr, e.content, e.fontFamily,
                                 qMax(6, int(e.fontSize * scaleY)),
                                 e.color.isValid() ? e.color : Qt::black);
        }

        if (selected) {
            p.save();
            p.setPen(QColor(37, 99, 235, 200));
            p.setFont(QFont("Arial", qMax(6, int(9 * scaleY))));
            p.drawText(QRectF(wr.x(), wr.bottom() - 18 * scaleY, wr.width(), 18 * scaleY),
                       Qt::AlignCenter, "Double-click to edit");
            p.restore();
        }

    } else if (e.type == SlideElement::IFrame) {
        p.fillRect(wr, QColor(235, 240, 250));
        p.setPen(QPen(QColor(150, 160, 190), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(wr);

        p.save();
        p.setPen(QColor(90, 100, 140));
        QFont iconFont("Arial", qMax(10, int(28 * scaleY)));
        p.setFont(iconFont);
        QRectF iconRect(wr.x(), wr.y() + 4 * scaleY, wr.width(), wr.height() * 0.55);
        p.drawText(iconRect, Qt::AlignCenter, QStringLiteral("\U0001F310"));

        QFont labelFont("Arial", qMax(6, int(9 * scaleY)));
        p.setFont(labelFont);
        QRectF labelRect(wr.x() + 4, wr.y() + wr.height() * 0.6, wr.width() - 8, wr.height() * 0.35);
        p.drawText(labelRect, Qt::AlignCenter | Qt::TextWordWrap,
                   e.content.isEmpty() ? "iFrame – double-click for link" : e.content);
        p.restore();

    } else if (e.type == SlideElement::Video) {
        bool hasRot = (e.rotation != 0.f);
        if (hasRot) {
            p.save();
            p.translate(wr.center());
            p.rotate(double(e.rotation));
            p.translate(-wr.center());
        }
        p.fillRect(wr, QColor(20, 20, 24));
        p.setPen(QColor(70, 70, 78));
        p.setBrush(Qt::NoBrush);
        p.drawRect(wr);

        QRectF playRect(wr.center().x() - wr.height() * 0.14, wr.center().y() - wr.height() * 0.18,
                        wr.height() * 0.28, wr.height() * 0.36);
        QPolygonF tri;
        tri << playRect.topLeft() << QPointF(playRect.right(), playRect.center().y()) << playRect.bottomLeft();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(230, 230, 235));
        p.drawPolygon(tri);

        p.setPen(QColor(200, 200, 210));
        QFont labelFont("Arial", qMax(6, int(9 * scaleY)));
        p.setFont(labelFont);
        QRectF labelRect(wr.x() + 4, wr.bottom() - 20 * scaleY, wr.width() - 8, 18 * scaleY);
        p.drawText(labelRect, Qt::AlignCenter,
                   e.content.isEmpty() ? "[Video]" : QFileInfo(e.content).fileName());
        if (hasRot) p.restore();

    } else if (e.type == SlideElement::Audio) {
        bool hasRot = (e.rotation != 0.f);
        if (hasRot) {
            p.save();
            p.translate(wr.center());
            p.rotate(double(e.rotation));
            p.translate(-wr.center());
        }
        float rr = qMin(wr.height() * 0.5f, 18.f * scaleY);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(30, 41, 59));
        p.drawRoundedRect(wr, rr, rr);

        QRectF iconRect(wr.x() + wr.width() * 0.04, wr.y(), wr.height(), wr.height());
        QPolygonF tri;
        tri << QPointF(iconRect.center().x() - iconRect.width() * 0.12, iconRect.center().y() - iconRect.height() * 0.18)
            << QPointF(iconRect.center().x() + iconRect.width() * 0.16, iconRect.center().y())
            << QPointF(iconRect.center().x() - iconRect.width() * 0.12, iconRect.center().y() + iconRect.height() * 0.18);
        p.setBrush(QColor(230, 230, 235));
        p.drawPolygon(tri);

        QRectF barsRect(iconRect.right(), wr.y() + wr.height() * 0.22,
                       wr.width() - iconRect.width() - wr.width() * 0.12, wr.height() * 0.56);
        if (e.audioShowWaveform) {
            // Deterministic pseudo-waveform (no decoded amplitude data in the
            // editor) purely so the two display modes look visually distinct
            // while editing — the real per-sample waveform is drawn client-side
            // at export time (Web Audio API), see HtmlExporter's audio JS.
            int bars = qMax(4, int(barsRect.width() / (4 * scaleY)));
            uint seed = qHash(e.content);
            for (int i = 0; i < bars; ++i) {
                seed = seed * 1103515245u + 12345u;
                float f = float((seed >> 8) & 0xFF) / 255.f;
                float bh = barsRect.height() * (0.25f + 0.75f * f);
                float bx = barsRect.x() + i * (barsRect.width() / bars);
                p.setBrush(QColor(100, 116, 139));
                p.drawRect(QRectF(bx, barsRect.center().y() - bh / 2, qMax(1.0, barsRect.width() / bars - 2), bh));
            }
        } else {
            p.setPen(QColor(203, 213, 225));
            QFont labelFont("Arial", qMax(6, int(9 * scaleY)));
            p.setFont(labelFont);
            p.drawText(barsRect, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
                       e.content.isEmpty() ? "[Audio]" : QFileInfo(e.content).fileName());
        }
        if (hasRot) p.restore();

    } else if (e.type == SlideElement::Button) {
        QRectF sr2 = slideRect();
        float rx = e.cornerRadius * sr2.width()  / SLIDE_W_DEFAULT;
        float ry = e.cornerRadius * sr2.height() / SLIDE_H_DEFAULT;

        p.setPen(e.borderWidth > 0
                 ? QPen(e.borderColor.isValid() ? e.borderColor : Qt::darkGray, e.borderWidth)
                 : Qt::NoPen);
        p.setBrush(e.backgroundColor.isValid() && e.backgroundColor != Qt::transparent
                   ? QBrush(e.backgroundColor) : QBrush(QColor(37, 99, 235)));
        if (rx > 0 || ry > 0) p.drawRoundedRect(wr, rx, ry);
        else                   p.drawRect(wr);

        QFont font(e.fontFamily, qMax(6, int(e.fontSize * scaleY)));
        font.setBold(e.bold);
        font.setItalic(e.italic);
        p.save();
        p.setFont(font);
        p.setPen(e.color.isValid() ? e.color : Qt::white);
        p.setClipRect(wr, Qt::IntersectClip);
        QString btnLabel = e.content.isEmpty() ? "Button" : substituteVars(e.content, currentSlideId);
        p.drawText(wr, Qt::AlignCenter | Qt::TextWordWrap, btnLabel);
        p.restore();

        if (selected) {
            p.save();
            p.setPen(QColor(37, 99, 235, 220));
            p.setFont(QFont("Arial", qMax(6, int(9 * scaleY))));
            p.drawText(QRectF(wr.x(), wr.bottom() - 16 * scaleY, wr.width(), 16 * scaleY),
                       Qt::AlignCenter, "Double-click to edit");
            p.restore();
        }

    } else if (e.type == SlideElement::Checkbox) {
        const Variable* v = m_pres ? m_pres->variables.findById(e.boundVariableId) : nullptr;
        bool checked = v && v->type == Variable::Boolean && v->boolValue;

        float box = qMin(wr.height(), 32.f * scaleY);
        QRectF boxRect(wr.x(), wr.y() + (wr.height() - box) / 2.0, box, box);
        p.setPen(QPen(e.borderColor.isValid() ? e.borderColor : Qt::darkGray, qMax(1.f, e.borderWidth)));
        p.setBrush(checked ? (e.backgroundColor.isValid() && e.backgroundColor != Qt::transparent
                              ? e.backgroundColor : QColor(37, 99, 235))
                            : QColor(255, 255, 255));
        p.drawRoundedRect(boxRect, 3, 3);
        if (checked) {
            p.setPen(QPen(Qt::white, qMax(2.0, 2.5 * scaleY)));
            QPointF a(boxRect.x() + box * 0.22, boxRect.y() + box * 0.55);
            QPointF b(boxRect.x() + box * 0.42, boxRect.y() + box * 0.75);
            QPointF c(boxRect.x() + box * 0.80, boxRect.y() + box * 0.25);
            p.drawLine(a, b);
            p.drawLine(b, c);
        }

        QRectF labelRect(boxRect.right() + 8 * scaleY, wr.y(), wr.width() - box - 8 * scaleY, wr.height());
        QFont font(e.fontFamily, qMax(6, int(e.fontSize * scaleY)));
        p.setFont(font);
        p.setPen(e.color.isValid() ? e.color : Qt::black);
        p.drawText(labelRect, Qt::AlignVCenter | Qt::TextWordWrap, substituteVars(e.content, currentSlideId));

        if (!v && selected) {
            p.setPen(QColor(200, 60, 60));
            p.drawText(QRectF(wr.x(), wr.bottom() - 16 * scaleY, wr.width(), 16 * scaleY),
                       Qt::AlignCenter, "No variable bound – double-click to fix");
        }

    } else if (e.type == SlideElement::Slider) {
        const Variable* v = m_pres ? m_pres->variables.findById(e.boundVariableId) : nullptr;
        double value = v && v->type == Variable::Number ? v->numberValue : e.sliderMin;
        double span  = (e.sliderMax - e.sliderMin);
        double frac  = span != 0.0 ? qBound(0.0, (value - e.sliderMin) / span, 1.0) : 0.0;

        QFont labelFont(e.fontFamily, qMax(6, int(e.fontSize * scaleY)));
        p.setFont(labelFont);
        p.setPen(e.color.isValid() ? e.color : Qt::black);
        QRectF labelRect(wr.x(), wr.y(), wr.width(), wr.height() * 0.4);
        QString label = e.content.isEmpty() ? QString() : substituteVars(e.content, currentSlideId) + "  ";
        p.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter,
                   label + QString::number(value, 'f', (value == int(value)) ? 0 : 2));

        float trackY = float(wr.y() + wr.height() * 0.72);
        float trackH = qMax(3.f, 6.f * scaleY);
        QRectF track(wr.x(), trackY - trackH / 2, wr.width(), trackH);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(220, 224, 230));
        p.drawRoundedRect(track, trackH / 2, trackH / 2);

        QRectF filled(wr.x(), trackY - trackH / 2, float(wr.width() * frac), trackH);
        p.setBrush(e.backgroundColor.isValid() && e.backgroundColor != Qt::transparent
                   ? e.backgroundColor : QColor(37, 99, 235));
        p.drawRoundedRect(filled, trackH / 2, trackH / 2);

        float thumbR = qMax(6.f, 9.f * scaleY);
        QPointF thumb(wr.x() + wr.width() * frac, trackY);
        p.setBrush(Qt::white);
        p.setPen(QPen(QColor(37, 99, 235), qMax(1.f, 2.f * scaleY)));
        p.drawEllipse(thumb, thumbR, thumbR);

        if (!v && selected) {
            p.setPen(QColor(200, 60, 60));
            p.drawText(QRectF(wr.x(), wr.bottom() - 16 * scaleY, wr.width(), 16 * scaleY),
                       Qt::AlignCenter, "No variable bound – double-click to fix");
        }
    }

    // Generic dashed selection outline (Text, Image, Chart)
    if (selected) {
        p.setPen(QPen(QColor(0, 120, 215), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(wr);
    }
}

// ── Table helpers ─────────────────────────────────────────────────────────────

QRectF SlideEditor2D::cellRect(const SlideElement& e, int row, int col) const {
    QRectF wr = elemToWidget(e);
    float cx = float(wr.x());
    for (int c = 0; c < col; ++c) cx += e.tableColFracs[c] * float(wr.width());
    float cy = float(wr.y());
    for (int r = 0; r < row; ++r) cy += e.tableRowFracs[r] * float(wr.height());

    // Account for colspan/rowspan
    int cs = 1, rs = 1;
    if (row < e.tableCells.size() && col < e.tableCells[row].size()) {
        cs = qMax(1, qMin(e.tableCells[row][col].colspan, e.tableCols - col));
        rs = qMax(1, qMin(e.tableCells[row][col].rowspan, e.tableRows - row));
    }
    float cw = 0, rh = 0;
    for (int c = col; c < col + cs && c < e.tableCols; ++c)
        cw += e.tableColFracs[c] * float(wr.width());
    for (int r = row; r < row + rs && r < e.tableRows; ++r)
        rh += e.tableRowFracs[r] * float(wr.height());
    return QRectF(cx, cy, cw, rh);
}

SlideEditor2D::CellPos SlideEditor2D::hitTableCell(int elemIdx, QPointF wpos) const {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || elemIdx < 0 || elemIdx >= s->elements.size()) return {};
    const SlideElement& e = s->elements[elemIdx];
    if (e.type != SlideElement::Table) return {};
    QRectF wr = elemToWidget(e);
    if (!wr.contains(wpos)) return {};

    float rx = float(wpos.x() - wr.x());
    float ry = float(wpos.y() - wr.y());

    int col = e.tableCols - 1;
    float cx = 0;
    for (int c = 0; c < e.tableCols; ++c) {
        cx += e.tableColFracs[c] * float(wr.width());
        if (rx < cx) { col = c; break; }
    }
    int row = e.tableRows - 1;
    float cy = 0;
    for (int r = 0; r < e.tableRows; ++r) {
        cy += e.tableRowFracs[r] * float(wr.height());
        if (ry < cy) { row = r; break; }
    }

    // If this cell is covered by a span, find the spanning cell
    if (row >= 0 && row < e.tableCells.size() &&
        col >= 0 && col < e.tableCells[row].size() &&
        e.tableCells[row][col].merged) {
        for (int r = 0; r <= row; ++r) {
            for (int c = 0; c <= col; ++c) {
                if (r < e.tableCells.size() && c < e.tableCells[r].size()) {
                    const TableCell& tc = e.tableCells[r][c];
                    if (!tc.merged && r + tc.rowspan > row && c + tc.colspan > col)
                        return {r, c};
                }
            }
        }
    }
    return {row, col};
}

SlideEditor2D::DividerHit SlideEditor2D::hitTableDivider(int elemIdx, QPointF wpos) const {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || elemIdx < 0 || elemIdx >= s->elements.size()) return {};
    const SlideElement& e = s->elements[elemIdx];
    if (e.type != SlideElement::Table) return {};
    QRectF wr = elemToWidget(e);
    if (!wr.adjusted(-6, -6, 6, 6).contains(wpos)) return {};

    const float THRESH = 5.f;

    // Column dividers
    float cx = float(wr.x());
    for (int c = 0; c < e.tableCols - 1; ++c) {
        cx += e.tableColFracs[c] * float(wr.width());
        if (qAbs(float(wpos.x()) - cx) <= THRESH) {
            float ry = float(wpos.y());
            if (ry >= wr.y() && ry <= wr.bottom())
                return {true, true, c};
        }
    }
    // Row dividers
    float ry = float(wr.y());
    for (int r = 0; r < e.tableRows - 1; ++r) {
        ry += e.tableRowFracs[r] * float(wr.height());
        if (qAbs(float(wpos.y()) - ry) <= THRESH) {
            float rx = float(wpos.x());
            if (rx >= wr.x() && rx <= wr.right())
                return {true, false, r};
        }
    }
    return {};
}

void SlideEditor2D::drawTableElement(QPainter& p, const SlideElement& e, bool selected,
                                     const QString& currentSlideId) const {
    QRectF wr = elemToWidget(e);
    QRectF sr = slideRect();
    float  sy = sr.height() / SLIDE_H_DEFAULT;
    int baseFontPx = qMax(6, int(e.tableFontSize * sy));

    // Multi-cell selection bounds
    int selR1 = -1, selC1 = -1, selR2 = -1, selC2 = -1;
    bool hasMultiSel = m_tableEditMode && selected && m_cellSelAnchorRow >= 0 && m_selTableRow >= 0
        && (m_cellSelAnchorRow != m_selTableRow || m_cellSelAnchorCol != m_selTableCol);
    if (hasMultiSel || (m_tableEditMode && selected && m_cellSelAnchorRow >= 0)) {
        selR1 = qMin(m_cellSelAnchorRow, m_selTableRow);
        selC1 = qMin(m_cellSelAnchorCol, m_selTableCol);
        selR2 = qMax(m_cellSelAnchorRow, m_selTableRow);
        selC2 = qMax(m_cellSelAnchorCol, m_selTableCol);
    }

    // Fill outer rect (catches rounding gaps)
    p.fillRect(wr, e.tableDefaultBg);

    // Draw cells
    float rowY = float(wr.y());
    for (int r = 0; r < e.tableRows; ++r) {
        float rowH = e.tableRowFracs[r] * float(wr.height());
        float colX = float(wr.x());
        for (int c = 0; c < e.tableCols; ++c) {
            float colW = e.tableColFracs[c] * float(wr.width());

            const TableCell& cell = e.tableCells[r][c];

            if (cell.merged) {
                // This cell is covered by a span — skip drawing
                colX += colW;
                continue;
            }

            // Compute spanning rect
            QRectF cr = cellRect(e, r, c);
            bool isHeader = r == 0 && e.tableHasHeader;

            // Background
            QColor bg = isHeader ? e.tableHeaderBg
                      : (cell.bgColor.isValid() ? cell.bgColor : e.tableDefaultBg);
            p.fillRect(cr, bg);

            // Text color
            QColor tc = isHeader ? e.tableHeaderText
                       : (cell.textColor.isValid() ? cell.textColor : e.tableDefaultText);

            // Font
            QFont font(e.tableFontFamily, baseFontPx);
            font.setBold(cell.bold || isHeader);
            font.setItalic(cell.italic);

            Qt::Alignment align = Qt::AlignLeft;
            if (cell.textAlign == "center") align = Qt::AlignHCenter;
            else if (cell.textAlign == "right") align = Qt::AlignRight;

            QRectF tr = cr.adjusted(4, 2, -4, -2);
            p.save();
            p.setFont(font);
            p.setPen(tc);
            p.setClipRect(cr, Qt::IntersectClip);

            bool isCurCell = m_tableEditMode && selected
                          && r == m_selTableRow && c == m_selTableCol;

            if (isCurCell && m_tableCellEditing) {
                // Text selection highlight
                const QString& txt = cell.text;
                if (m_tableSelAnchor >= 0 && m_tableSelAnchor != m_tableCursorPos) {
                    int lo = qMin(m_tableCursorPos, m_tableSelAnchor);
                    int hi = qMax(m_tableCursorPos, m_tableSelAnchor);
                    QFontMetrics fm(font);
                    float x1 = float(tr.x()) + fm.horizontalAdvance(txt.left(lo));
                    float sw = fm.horizontalAdvance(txt.mid(lo, hi - lo));
                    p.fillRect(QRectF(x1, tr.y() + 1, sw, tr.height() - 2),
                               QColor(37, 99, 235, 80));
                }
                p.drawText(tr, int(Qt::TextWordWrap | Qt::AlignVCenter | align), cell.text);
                if (m_cursorVisible) {
                    QFontMetrics fm(font);
                    int pos = qBound(0, m_tableCursorPos, cell.text.length());
                    float cx2 = float(tr.x()) + fm.horizontalAdvance(cell.text.left(pos));
                    float cy2 = float(tr.y()) + 1;
                    float ch  = float(tr.height()) - 2;
                    p.setPen(QPen(tc, 2));
                    p.drawLine(QPointF(cx2, cy2), QPointF(cx2, cy2 + ch));
                }
            } else {
                p.drawText(tr, int(Qt::TextWordWrap | Qt::AlignVCenter | align),
                           substituteVars(cell.text, currentSlideId));
            }
            p.restore();

            // Cell border
            if (e.tableBorderWidth > 0) {
                p.setPen(QPen(e.tableBorderColor, e.tableBorderWidth));
                p.setBrush(Qt::NoBrush);
                p.drawRect(cr);
            }

            // Single-cell selection highlight
            if (isCurCell && !hasMultiSel) {
                p.setPen(QPen(QColor(37, 99, 235), 2.5));
                p.setBrush(Qt::NoBrush);
                p.drawRect(cr.adjusted(1, 1, -1, -1));
                p.fillRect(cr.adjusted(2, 2, -2, -2), QColor(37, 99, 235, 30));
            }

            colX += colW;
        }
        rowY += rowH;
    }

    // Multi-cell selection overlay
    if (hasMultiSel && selR1 >= 0) {
        // Top-left of (selR1, selC1) to bottom-right of (selR2, selC2)
        QRectF tl = cellRect(e, selR1, selC1);
        QRectF br = cellRect(e, selR2, selC2);
        QRectF selRect(tl.topLeft(), br.bottomRight());
        p.fillRect(selRect, QColor(37, 99, 235, 45));
        p.setPen(QPen(QColor(37, 99, 235), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(selRect.adjusted(1, 1, -1, -1));
    }

    // Outer border
    if (e.tableBorderWidth > 0) {
        p.setPen(QPen(e.tableBorderColor, e.tableBorderWidth + 0.5f));
        p.setBrush(Qt::NoBrush);
        p.drawRect(wr);
    }

    if (selected) {
        p.setPen(QPen(QColor(0, 120, 215), 2, Qt::SolidLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(wr.adjusted(-1, -1, 1, 1));
    }
}

void SlideEditor2D::updateCodeLangOverlay() {
    const Slide* slide = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    int idx = (m_editingElem >= 0) ? m_editingElem : m_selectedElem;
    const SlideElement* e = (slide && idx >= 0 && idx < slide->elements.size())
                            ? &slide->elements[idx] : nullptr;
    bool hasCode = e && e->type == SlideElement::Text
                && e->listStyle == SlideElement::NoList
                && !e->codeSpans.isEmpty();
    if (!hasCode) {
        m_codeLangCombo->hide();
        return;
    }

    QString lang = e->codeSpans.first().language.isEmpty() ? "plaintext" : e->codeSpans.first().language;
    int di = m_codeLangCombo->findData(lang);
    m_codeLangCombo->blockSignals(true);
    m_codeLangCombo->setCurrentIndex(di >= 0 ? di : 0);
    m_codeLangCombo->blockSignals(false);

    QRectF wr = elemToWidget(*e);
    int comboH = m_codeLangCombo->sizeHint().height();
    int x = int(wr.left());
    int y = int(wr.top()) - comboH - 4;
    if (y < 0) y = int(wr.top()) + 4; // not enough room above — sit just inside the top edge
    m_codeLangCombo->move(x, y);
    m_codeLangCombo->show();
    m_codeLangCombo->raise();
}

void SlideEditor2D::onCodeLangOverlayChanged(int) {
    Slide* slide = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    int idx = (m_editingElem >= 0) ? m_editingElem : m_selectedElem;
    if (!slide || idx < 0 || idx >= slide->elements.size()) return;
    SlideElement& e = slide->elements[idx];
    if (e.codeSpans.isEmpty()) return;

    QString lang = m_codeLangCombo->currentData().toString();
    if (lang.isEmpty()) e.codeSpans.clear();       // "Code: off" — back to plain text
    else for (CodeSpan& sp : e.codeSpans) sp.language = lang;

    update();
    emit presentationModified();
}

void SlideEditor2D::drawHandles(QPainter& p, const QRectF& r, float rotation) const {
    p.save();
    if (rotation != 0.f) {
        p.translate(r.center());
        p.rotate(double(rotation));
        p.translate(-r.center());
    }

    // Blue selection border
    p.setPen(QPen(QColor(0, 120, 215), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r);

    // 8 resize handles (white squares with blue border)
    const auto pts = handlePoints(r);
    for (const auto& pt : pts) {
        p.setPen(QPen(QColor(0, 120, 215), 1.5));
        p.setBrush(Qt::white);
        p.drawRect(QRectF(pt.x() - HANDLE_V, pt.y() - HANDLE_V,
                          HANDLE_V * 2,       HANDLE_V * 2));
    }

    // Rotation handle: stem + circle above TC
    QPointF tc(r.center().x(), r.top());
    QPointF rotHandle(tc.x(), tc.y() - 30);
    p.setPen(QPen(QColor(0, 120, 215), 1.5));
    p.drawLine(tc, rotHandle);
    p.setBrush(Qt::white);
    p.drawEllipse(rotHandle, HANDLE_V, HANDLE_V);

    p.restore();
}

// Bounding-box chrome for a multi/group selection: a rounded dashed outline
// (the "abrunden" group-styling ask, read as the group's own visual framing)
// plus resize handles on its corners/edges. No rotation handle — group
// rotation-as-one-unit isn't supported (see FEATURES_TODO.md scope note).
void SlideEditor2D::drawGroupHandles(QPainter& p, const QRectF& r) const {
    if (r.isNull()) return;
    p.save();

    QPainterPath path;
    path.addRoundedRect(r.adjusted(-6, -6, 6, 6), 8, 8);
    p.setPen(QPen(QColor(0, 120, 215), 2, Qt::DashLine));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    const auto pts = handlePoints(r);
    for (const auto& pt : pts) {
        p.setPen(QPen(QColor(0, 120, 215), 1.5));
        p.setBrush(Qt::white);
        p.drawRect(QRectF(pt.x() - HANDLE_V, pt.y() - HANDLE_V,
                          HANDLE_V * 2,       HANDLE_V * 2));
    }

    p.restore();
}

// ── Format painter ────────────────────────────────────────────────────────────

static void applyFormat(SlideElement& dst, const SlideElement& src) {
    dst.color             = src.color;
    dst.backgroundColor   = src.backgroundColor;
    dst.fontFamily        = src.fontFamily;
    dst.fontSize          = src.fontSize;
    dst.textAlignment     = src.textAlignment;
    dst.verticalAlignment = src.verticalAlignment;
    dst.bold           = src.bold;
    dst.italic         = src.italic;
    dst.underline      = src.underline;
    dst.strikethrough  = src.strikethrough;
    dst.underlineColor = src.underlineColor;
    dst.underlineStyle = src.underlineStyle;
    dst.listStyle      = src.listStyle;
    dst.borderColor    = src.borderColor;
    dst.borderWidth    = src.borderWidth;
    dst.cornerRadius   = src.cornerRadius;
}

void SlideEditor2D::activateFormatPainter(const SlideElement& source) {
    m_formatTemplate   = source;
    m_formatPainterMode = true;
    setCursor(Qt::CrossCursor);
}

// ── Mouse events ──────────────────────────────────────────────────────────────

void SlideEditor2D::mousePressEvent(QMouseEvent* e) {
    if (m_keyframeEditActive && e->button() == Qt::LeftButton
        && m_keyframeDoneRect.contains(e->position())) {
        emit keyframeEditDone();
        return;
    }
    if (e->button() == Qt::MiddleButton) {
        m_panning        = true;
        m_panStartMouse  = e->position();
        m_panStartOffset = m_panOffset;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (e->button() != Qt::LeftButton) return;

    // Rulers & guides — always take priority over content underneath, same as in real apps.
    if (m_showRulers) {
        bool vert;
        int gi = hitGuide(e->position(), vert);
        if (gi >= 0) {
            m_draggingGuide = true;
            m_newGuide      = false;
            m_guideVertical = vert;
            m_guideIndex    = gi;
            m_guideDragPos  = e->position();
            update();
            return;
        }
        bool inTop  = inTopRulerBand(e->position());
        bool inLeft = inLeftRulerBand(e->position());
        if (inTop || inLeft) {
            if (m_rulerTool == RulerTool::Standard) {
                m_draggingGuide = true;
                m_newGuide      = true;
                m_guideVertical = inLeft; // dragging out of the left ruler → vertical guide; top → horizontal
                m_guideDragPos  = e->position();
                update();
            }
            return; // ruler-band clicks never fall through to element/marquee handling
        }
    }

    // Format painter mode
    if (m_formatPainterMode) {
        int hit = hitTest(e->position());
        if (hit >= 0 && m_pres) {
            Slide* s = m_pres->slideAt(m_slideIndex);
            applyFormat(s->elements[hit], m_formatTemplate);
            setSingleSelection(hit);
            emit elementSelected(hit);
            emit elementsSelected(m_selectedElems);
            emit presentationModified();
        }
        m_formatPainterMode = false;
        setCursor(Qt::ArrowCursor);
        update();
        return;
    }

    // Text editing: click inside same element → reposition cursor; elsewhere → finish
    if (m_editingElem >= 0) {
        Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
        if (s && hitTest(e->position()) == m_editingElem) {
            m_cursorPos     = textPositionAt(s->elements[m_editingElem], e->position());
            m_selAnchor     = -1;
            m_textSelecting = true;
            m_cursorVisible = true;
            m_cursorBlink->start();
            update();
            return;
        }
        finishTextEdit();
    }

    // Table edit mode
    if (m_tableEditMode && m_selectedElem >= 0) {
        Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
        if (!s) return;
        SlideElement& elem = s->elements[m_selectedElem];

        // Check column/row divider for resize
        DividerHit dh = hitTableDivider(m_selectedElem, e->position());
        if (dh.valid) {
            m_dragDivider  = dh;
            m_divDragStart = dh.isCol ? float(e->position().x()) : float(e->position().y());
            m_divFracA     = dh.isCol ? elem.tableColFracs[dh.idx]     : elem.tableRowFracs[dh.idx];
            m_divFracB     = dh.isCol ? elem.tableColFracs[dh.idx + 1] : elem.tableRowFracs[dh.idx + 1];
            return;
        }

        // Click inside table: select cell
        CellPos cp = hitTableCell(m_selectedElem, e->position());
        if (cp.valid()) {
            bool sameCell = cp.row == m_selTableRow && cp.col == m_selTableCol;
            m_selTableRow = cp.row;
            m_selTableCol = cp.col;
            // Anchor for multi-cell selection
            m_cellSelAnchorRow = cp.row;
            m_cellSelAnchorCol = cp.col;
            m_isDraggingCellSel = true;
            if (sameCell && m_tableCellEditing) {
                // Reposition cursor within cell text
                QRectF cr   = cellRect(elem, cp.row, cp.col);
                float sy    = slideRect().height() / SLIDE_H_DEFAULT;
                QFont font(elem.tableFontFamily, qMax(6, int(elem.tableFontSize * sy)));
                font.setBold(elem.tableCells[cp.row][cp.col].bold || (cp.row == 0 && elem.tableHasHeader));
                QFontMetrics fm(font);
                QRectF tr = cr.adjusted(4, 2, -4, -2);
                float relX = float(e->position().x()) - float(tr.x());
                m_tableCursorPos = 0;
                float accum = 0;
                const QString& txt = elem.tableCells[cp.row][cp.col].text;
                for (int i = 0; i < txt.length(); ++i) {
                    float hw = fm.horizontalAdvance(txt.left(i + 1));
                    if (relX < (accum + hw) * 0.5f + accum * 0.5f) { m_tableCursorPos = i; break; }
                    m_tableCursorPos = i + 1;
                    accum = hw;
                }
                m_tableSelAnchor = -1;
            } else {
                m_tableCellEditing = false;
            }
            emit tableCellSelected(cp.row, cp.col);
            update();
            return;
        }

        // Click outside table: exit table edit mode
        exitTableEditMode();
        // Fall through to regular hit test
    }

    // Column/row divider resize (table element must be selected but not in edit mode)
    if (m_selectedElem >= 0 && m_pres) {
        Slide* s = m_pres->slideAt(m_slideIndex);
        if (s && m_selectedElem < s->elements.size() &&
            s->elements[m_selectedElem].type == SlideElement::Table) {
            DividerHit dh = hitTableDivider(m_selectedElem, e->position());
            if (dh.valid) {
                SlideElement& elem = s->elements[m_selectedElem];
                m_dragDivider  = dh;
                m_divDragStart = dh.isCol ? float(e->position().x()) : float(e->position().y());
                m_divFracA     = dh.isCol ? elem.tableColFracs[dh.idx]     : elem.tableRowFracs[dh.idx];
                m_divFracB     = dh.isCol ? elem.tableColFracs[dh.idx + 1] : elem.tableRowFracs[dh.idx + 1];
                return;
            }
        }
    }

    // Handles of selected element (resize or rotation)
    int handle = hitHandle(e->position());
    if (handle == 8) {
        // Rotation handle
        Slide* s = m_pres->slideAt(m_slideIndex);
        const SlideElement& elem = s->elements[m_selectedElem];
        QRectF wr = elemToWidget(elem);
        m_rotatingHandle   = true;
        m_rotateOrigAngle  = elem.rotation;
        m_rotateStartAngle = float(qRadiansToDegrees(
            qAtan2(e->position().y() - wr.center().y(),
                   e->position().x() - wr.center().x())));
        return;
    }
    if (handle >= 0) {
        Slide* s = m_pres->slideAt(m_slideIndex);
        m_resizingHandle = handle;
        QRectF bbox = selectionSlideRect();
        m_resizeOrigX = float(bbox.x());     m_resizeOrigY = float(bbox.y());
        m_resizeOrigW = float(bbox.width()); m_resizeOrigH = float(bbox.height());
        m_resizeOrigMembers.clear();
        for (int idx : m_selectedElems) {
            const SlideElement& el = s->elements[idx];
            m_resizeOrigMembers.append(QRectF(el.x, el.y, el.width, el.height));
        }
        m_dragStartSlide = widgetToSlide(e->position());
        return;
    }

    // Hit-test elements for selection / move
    int hit = hitTest(e->position());
    if (hit != m_selectedElem && m_tableEditMode) {
        exitTableEditMode();
    }

    const bool toggleMod = (e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier));

    if (hit < 0) {
        if (m_rulerTool == RulerTool::Circle) {
            m_drawingCircle      = true;
            m_circleCenterWidget = e->position();
            m_circleRadiusWidget = 0.f;
            update();
            return;
        }
        if (m_rulerTool == RulerTool::Measure) {
            m_measuring         = true;
            m_hasMeasureResult  = false;
            m_measureStartWidget = e->position();
            m_measureEndWidget   = e->position();
            update();
            return;
        }
        if (!toggleMod) {
            // Empty space: start a rubber-band marquee drag. A plain click with
            // no movement clears the selection on release (mouseReleaseEvent).
            m_marqueeActive = true;
            m_marqueeStart  = e->position();
            m_marqueeRect   = QRectF(m_marqueeStart, QSizeF(0, 0));
            update();
            return;
        }
    } else {
        QVector<int> members = groupMembers(hit);
        if (toggleMod) {
            // Ctrl/Shift+click: toggle this element's whole group into/out of
            // the existing selection, without disturbing the rest of it.
            if (m_selectedElems.contains(hit)) {
                for (int m : members) m_selectedElems.removeAll(m);
            } else {
                for (int m : members)
                    if (!m_selectedElems.contains(m)) m_selectedElems.append(m);
            }
            m_selectedElem = m_selectedElems.isEmpty() ? -1 : hit;
        } else if (!m_selectedElems.contains(hit)) {
            // Plain click outside the current selection: replace it with just
            // this element's group. Clicking a member already selected keeps
            // the whole selection intact so the following drag moves everyone.
            m_selectedElems = members;
            m_selectedElem  = hit;
        } else {
            // Clicking a member already in the selection: keep the whole
            // selection so the drag that follows moves every member.
            m_selectedElem = hit;
        }
    }

    if (hit >= 0 && m_pres) {
        m_dragging       = true;
        m_dragStartSlide = widgetToSlide(e->position());
        Slide* s         = m_pres->slideAt(m_slideIndex);
        m_dragOrigin     = QPointF(s->elements[hit].x, s->elements[hit].y);
        m_dragOrigins.clear();
        for (int idx : m_selectedElems)
            m_dragOrigins.append(QPointF(s->elements[idx].x, s->elements[idx].y));
    }

    emit elementSelected(m_selectedElems.size() == 1 ? m_selectedElem : -1);
    emit elementsSelected(m_selectedElems);
    update();
}

void SlideEditor2D::mouseMoveEvent(QMouseEvent* e) {
    // Canvas panning (middle-mouse-button drag)
    if (m_panning) {
        m_panOffset = m_panStartOffset + (e->position() - m_panStartMouse);
        update();
        return;
    }

    // Ruler guide being created/repositioned
    if (m_draggingGuide) {
        m_guideDragPos = e->position();
        update();
        return;
    }

    // Circle tool ("Kreis/Zirkel") drag
    if (m_drawingCircle) {
        QPointF d = e->position() - m_circleCenterWidget;
        m_circleRadiusWidget = float(std::sqrt(d.x() * d.x() + d.y() * d.y()));
        update();
        return;
    }

    // Measure tool ("Messgerät") drag
    if (m_measuring) {
        m_measureEndWidget = e->position();
        update();
        return;
    }

    // Rubber-band marquee selection drag
    if (m_marqueeActive) {
        m_marqueeRect = QRectF(m_marqueeStart, e->position()).normalized();
        update();
        return;
    }

    // Table divider drag-resize
    if (m_dragDivider.valid) {
        Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
        if (s && m_selectedElem >= 0 && m_selectedElem < s->elements.size()) {
            SlideElement& elem = s->elements[m_selectedElem];
            QRectF wr = elemToWidget(elem);
            float totalPx = m_dragDivider.isCol ? float(wr.width()) : float(wr.height());
            float delta   = (m_dragDivider.isCol ? float(e->position().x()) : float(e->position().y()))
                            - m_divDragStart;
            float dFrac = (totalPx > 0) ? delta / totalPx : 0.f;
            float minFrac = 20.f / totalPx; // minimum ~20px
            float newA = qMax(minFrac, m_divFracA + dFrac);
            float newB = qMax(minFrac, m_divFracA + m_divFracB - newA);
            if (m_dragDivider.isCol) {
                elem.tableColFracs[m_dragDivider.idx]     = newA;
                elem.tableColFracs[m_dragDivider.idx + 1] = newB;
            } else {
                elem.tableRowFracs[m_dragDivider.idx]     = newA;
                elem.tableRowFracs[m_dragDivider.idx + 1] = newB;
            }
            update();
            emit presentationModified();
        }
        return;
    }

    // Multi-cell drag selection (table edit mode)
    if (m_isDraggingCellSel && m_tableEditMode && m_selectedElem >= 0) {
        CellPos cp = hitTableCell(m_selectedElem, e->position());
        if (cp.valid() && (cp.row != m_selTableRow || cp.col != m_selTableCol)) {
            m_selTableRow = cp.row;
            m_selTableCol = cp.col;
            m_tableCellEditing = false; // exit text editing when dragging across cells
            update();
        }
        return;
    }

    // Text drag-selection
    if (m_editingElem >= 0 && m_textSelecting) {
        Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
        if (s) {
            if (m_selAnchor < 0) m_selAnchor = m_cursorPos;
            m_cursorPos = textPositionAt(s->elements[m_editingElem], e->position());
            if (m_selAnchor == m_cursorPos) m_selAnchor = -1;
            m_cursorVisible = true;
            update();
            emit textSelectionChanged(m_cursorPos, m_selAnchor);
        }
        return;
    }

    QPointF curSlide = widgetToSlide(e->position());

    // Rotation drag
    if (m_rotatingHandle) {
        Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
        if (s && m_selectedElem >= 0 && m_selectedElem < s->elements.size()) {
            SlideElement& elem = s->elements[m_selectedElem];
            QRectF wr = elemToWidget(elem);
            float angle = float(qRadiansToDegrees(
                qAtan2(e->position().y() - wr.center().y(),
                       e->position().x() - wr.center().x())));
            float newRot = m_rotateOrigAngle + (angle - m_rotateStartAngle);
            if (e->modifiers() & Qt::ControlModifier)
                newRot = qRound(newRot / 45.f) * 45.f;
            elem.rotation = newRot;
            update();
            emit presentationModified();
        }
        return;
    }

    // Resize mode — origin is the bounding box of the whole selection; for a
    // single selected element that box is just its own rect, so this collapses
    // to the previous single-element behavior exactly (scale=1, offset=0).
    if (m_resizingHandle >= 0) {
        Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
        if (s && !m_selectedElems.isEmpty() && m_selectedElems.size() == m_resizeOrigMembers.size()) {
            bool constrain = (e->modifiers() & Qt::ControlModifier) != 0;
            SlideElement bboxElem; // pseudo-element standing in for the selection's bbox
            bboxElem.x = m_resizeOrigX; bboxElem.y = m_resizeOrigY;
            bboxElem.width = m_resizeOrigW; bboxElem.height = m_resizeOrigH;
            applyResize(bboxElem, m_resizingHandle, curSlide,
                        m_resizeOrigX, m_resizeOrigY,
                        m_resizeOrigW, m_resizeOrigH, constrain);
            float scaleX = (m_resizeOrigW > 0.0001f) ? bboxElem.width  / m_resizeOrigW : 1.f;
            float scaleY = (m_resizeOrigH > 0.0001f) ? bboxElem.height / m_resizeOrigH : 1.f;
            for (int k = 0; k < m_selectedElems.size(); ++k) {
                int idx = m_selectedElems[k];
                if (idx < 0 || idx >= s->elements.size()) continue;
                const QRectF& orig = m_resizeOrigMembers[k];
                SlideElement& el = s->elements[idx];
                el.x      = float(bboxElem.x + (orig.x() - m_resizeOrigX) * scaleX);
                el.y      = float(bboxElem.y + (orig.y() - m_resizeOrigY) * scaleY);
                el.width  = float(orig.width()  * scaleX);
                el.height = float(orig.height() * scaleY);
            }
            update();
            emit presentationModified();
        }
        return;
    }

    // Move mode — apply the same delta to every selected element; snap/guides
    // are computed against the primary element only, then the corrective nudge
    // that snapping introduced is applied to the rest of the selection too.
    if (m_dragging && !m_selectedElems.isEmpty() && m_selectedElems.size() == m_dragOrigins.size()) {
        Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
        if (!s) return;
        QPointF delta = curSlide - m_dragStartSlide;
        int primaryPos = m_selectedElems.indexOf(m_selectedElem);
        QPointF corrective(0, 0);
        if (primaryPos >= 0 && m_selectedElem < s->elements.size()) {
            SlideElement& primary = s->elements[m_selectedElem];
            QPointF preSnap = m_dragOrigins[primaryPos] + delta;
            primary.x = float(preSnap.x());
            primary.y = float(preSnap.y());
            applySnapAndGuides(primary);
            corrective = QPointF(primary.x, primary.y) - preSnap;
        }
        for (int k = 0; k < m_selectedElems.size(); ++k) {
            int idx = m_selectedElems[k];
            if (idx == m_selectedElem || idx < 0 || idx >= s->elements.size()) continue;
            SlideElement& el = s->elements[idx];
            QPointF np = m_dragOrigins[k] + delta + corrective;
            el.x = float(np.x());
            el.y = float(np.y());
        }
        update();
        emit presentationModified();
        return;
    }

    // Cursor hints when hovering
    if (m_showRulers) {
        bool vert;
        if (hitGuide(e->position(), vert) >= 0) {
            setCursor(vert ? Qt::SplitHCursor : Qt::SplitVCursor);
            return;
        }
        if (inTopRulerBand(e->position()) || inLeftRulerBand(e->position())) {
            if (m_rulerTool == RulerTool::Standard)
                setCursor(inLeftRulerBand(e->position()) ? Qt::SplitHCursor : Qt::SplitVCursor);
            else
                setCursor(Qt::CrossCursor);
            return;
        }
    }
    if (m_selectedElem >= 0) {
        DividerHit dh = hitTableDivider(m_selectedElem, e->position());
        if (dh.valid) {
            setCursor(dh.isCol ? Qt::SplitHCursor : Qt::SplitVCursor);
            return;
        }
    }
    int handle = hitHandle(e->position());
    if (handle == 8) {
        setCursor(Qt::CrossCursor);
    } else if (handle >= 0) {
        static const Qt::CursorShape cursors[8] = {
            Qt::SizeFDiagCursor, Qt::SizeBDiagCursor,
            Qt::SizeBDiagCursor, Qt::SizeFDiagCursor,
            Qt::SizeVerCursor,   Qt::SizeVerCursor,
            Qt::SizeHorCursor,   Qt::SizeHorCursor,
        };
        setCursor(cursors[handle]);
    } else if (hitTest(e->position()) >= 0) {
        setCursor(Qt::SizeAllCursor);
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void SlideEditor2D::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::MiddleButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        return;
    }
    if (e->button() == Qt::LeftButton) {
        if (m_draggingGuide) {
            m_guideDragPos = e->position();
            finalizeGuideDrag();
            update();
            emit presentationModified();
            return;
        }
        if (m_drawingCircle) {
            m_drawingCircle = false;
            Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
            if (s && m_circleRadiusWidget > 3.f) {
                QRectF sr = slideRect();
                float scaleX = float(sr.width())  / SLIDE_W_DEFAULT;
                float scaleY = float(sr.height()) / SLIDE_H_DEFAULT;
                QPointF centerSlide = widgetToSlide(m_circleCenterWidget);
                GuideCircle c;
                c.cx     = float(centerSlide.x());
                c.cy     = float(centerSlide.y());
                c.radius = m_circleRadiusWidget / qMax(0.0001f, (scaleX + scaleY) * 0.5f);
                s->guideCircles.append(c);
                emit presentationModified();
            }
            update();
            return;
        }
        if (m_measuring) {
            m_measuring        = false;
            m_hasMeasureResult = true;
            update();
            return;
        }
        if (m_marqueeActive) {
            m_marqueeActive = false;
            // Only treat it as a real marquee if the user actually dragged a
            // visible distance; a plain click-release with no movement clears
            // the selection instead (matches clicking empty space elsewhere).
            if (m_marqueeRect.width() > 3 || m_marqueeRect.height() > 3) {
                QVector<int> newSel;
                const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
                if (s) {
                    for (int i = 0; i < s->elements.size(); ++i) {
                        if (m_marqueeRect.intersects(elemToWidget(s->elements[i]))) {
                            for (int m : groupMembers(i))
                                if (!newSel.contains(m)) newSel.append(m);
                        }
                    }
                }
                m_selectedElems = newSel;
                m_selectedElem  = newSel.isEmpty() ? -1 : newSel.first();
            } else {
                m_selectedElems.clear();
                m_selectedElem = -1;
            }
            m_marqueeRect = QRectF();
            emit elementSelected(m_selectedElems.size() == 1 ? m_selectedElem : -1);
            emit elementsSelected(m_selectedElems);
        }
        m_textSelecting     = false;
        m_dragging          = false;
        m_resizingHandle    = -1;
        m_rotatingHandle    = false;
        m_dragDivider       = {};
        m_isDraggingCellSel = false;
        m_snapGuides.clear();
        update();
    }
}

void SlideEditor2D::wheelEvent(QWheelEvent* e) {
    // Ctrl + wheel: zoom in/out, keeping the point under the cursor fixed.
    if (e->modifiers() & Qt::ControlModifier) {
        float factor = (e->angleDelta().y() > 0) ? ZOOM_STEP : (1.f / ZOOM_STEP);
        setZoom(m_zoom * factor, e->position());
        e->accept();
        return;
    }
    // Plain wheel: pan vertically; Shift+wheel: pan horizontally (useful once zoomed in).
    QPointF delta = (e->modifiers() & Qt::ShiftModifier)
                   ? QPointF(e->angleDelta().y() / 8.0, 0)
                   : QPointF(e->angleDelta().x() / 8.0, e->angleDelta().y() / 8.0);
    if (!delta.isNull()) {
        m_panOffset += delta;
        update();
        e->accept();
        return;
    }
    QWidget::wheelEvent(e);
}

void SlideEditor2D::mouseDoubleClickEvent(QMouseEvent* e) {
    int hit = hitTest(e->position());
    if (hit < 0 || !m_pres) return;
    Slide* s = m_pres->slideAt(m_slideIndex);
    if (!s) return;
    const SlideElement& elem = s->elements[hit];

    if (elem.type == SlideElement::Chart) {
        setSingleSelection(hit);
        openChartEditor();
        return;
    } else if (elem.type == SlideElement::Formula) {
        setSingleSelection(hit);
        openFormulaEditor();
        return;
    } else if (elem.type == SlideElement::IFrame) {
        setSingleSelection(hit);
        openIFrameEditor();
        return;
    } else if (elem.type == SlideElement::Button) {
        setSingleSelection(hit);
        openButtonEditor();
        return;
    } else if (elem.type == SlideElement::Checkbox) {
        setSingleSelection(hit);
        openCheckboxEditor();
        return;
    } else if (elem.type == SlideElement::Slider) {
        setSingleSelection(hit);
        openSliderEditor();
        return;
    } else if (elem.type == SlideElement::Text) {
        startTextEdit(hit, e->position());
    } else if (elem.type == SlideElement::Shape) {
        setSingleSelection(hit);
        startTextEdit(hit, e->position());
    } else if (elem.type == SlideElement::Table) {
        setSingleSelection(hit);
        m_tableEditMode = true;
        CellPos cp = hitTableCell(hit, e->position());
        if (!cp.valid()) { m_selTableRow = 0; m_selTableCol = 0; }
        else             { m_selTableRow = cp.row; m_selTableCol = cp.col; }
        m_tableCellEditing = true;
        m_tableCursorPos   = s->elements[hit].tableCells[m_selTableRow][m_selTableCol].text.length();
        m_tableSelAnchor   = -1;
        m_cursorBlink->start();
        m_cursorVisible = true;
        emit elementSelected(hit);
        emit elementsSelected(m_selectedElems);
        emit tableCellSelected(m_selTableRow, m_selTableCol);
        update();
    }
}

void SlideEditor2D::keyPressEvent(QKeyEvent* e) {
    if (m_keyframeEditActive) {
        // MainWindow's keyframe-edit session tracks the edited element by
        // index into Slide::elements (see MainWindow::onKeyframeEditRequested).
        // Delete/cut/paste/duplicate would shift that index out from under the
        // session and corrupt an unrelated element when the session ends (its
        // baseline would get written into whatever shifted into the old
        // slot) — so keyboard input is restricted to Escape (end the session)
        // while one is active. Dragging and PropertiesPanel edits still work
        // normally, since neither goes through this handler.
        if (e->key() == Qt::Key_Escape) emit keyframeEditDone();
        return;
    }
    if (m_editingElem >= 0) { handleTextEditKey(e); return; }
    if (m_tableEditMode)    { handleTableKey(e);     return; }

    const bool ctrl  = e->modifiers() & Qt::ControlModifier;
    const bool shift = e->modifiers() & Qt::ShiftModifier;

    if (ctrl && shift && e->key() == Qt::Key_G) {
        ungroupSelectedElements();
    } else if (ctrl && e->key() == Qt::Key_G) {
        groupSelectedElements();
    } else if (ctrl && e->key() == Qt::Key_C) {
        copySelectedElement();
    } else if (ctrl && e->key() == Qt::Key_X) {
        copySelectedElement(); deleteSelectedElement();
    } else if (ctrl && e->key() == Qt::Key_V) {
        // Table selected: ALWAYS prefer text paste over image paste
        if (m_selectedElem >= 0 && m_pres) {
            Slide* s = m_pres->slideAt(m_slideIndex);
            if (s && m_selectedElem < s->elements.size() &&
                s->elements[m_selectedElem].type == SlideElement::Table) {
                QString txt = QApplication::clipboard()->text();
                if (!txt.isEmpty()) {
                    pasteExcelIntoTable(s->elements[m_selectedElem]);
                    return;
                }
            }
        }
        // Non-table: check for image in system clipboard first
        const QMimeData* md = QApplication::clipboard()->mimeData();
        if (md && md->hasImage()) {
            QImage img = qvariant_cast<QImage>(md->imageData());
            if (!img.isNull()) {
                QString path = QDir::tempPath() + "/impress_paste_"
                               + QUuid::createUuid().toString(QUuid::Id128) + ".png";
                img.save(path, "PNG");
                addImageFromPath(path);
                return;
            }
        }
        // No image, not a table: plain text on the clipboard becomes a new text box
        QString txt = QApplication::clipboard()->text();
        if (!txt.isEmpty()) {
            pasteTextAsNewElement(txt);
            return;
        }
        pasteElement();
    } else if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
        deleteSelectedElement();
    } else if (e->key() == Qt::Key_Escape && (m_measuring || m_hasMeasureResult || m_drawingCircle)) {
        m_measuring        = false;
        m_hasMeasureResult = false;
        m_drawingCircle    = false;
        update();
    } else if (e->key() == Qt::Key_Escape) {
        m_selectedElem = -1;
        m_selectedElems.clear();
        emit elementSelected(-1);
        emit elementsSelected(m_selectedElems);
        update();
    } else if (ctrl && (e->key() == Qt::Key_Plus || e->key() == Qt::Key_Equal)) {
        zoomIn();
    } else if (ctrl && e->key() == Qt::Key_Minus) {
        zoomOut();
    } else if (ctrl && e->key() == Qt::Key_0) {
        zoomReset();
    } else {
        QWidget::keyPressEvent(e);
    }
}

void SlideEditor2D::copySelectedElement() {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElems.isEmpty()) return;
    s_clipboard.clear();
    for (int idx : m_selectedElems)
        if (idx >= 0 && idx < s->elements.size()) s_clipboard.append(s->elements[idx]);
    s_hasClipboard = !s_clipboard.isEmpty();
}

void SlideEditor2D::pasteElement() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || !s_hasClipboard || s_clipboard.isEmpty()) return;

    // If the copied set shared a group, give the pasted copies their own new
    // shared group id — keeps the duplicate cohesive without merging it into
    // the original group (which may still be on the slide).
    QString newGroupId;
    if (s_clipboard.size() > 1 && !s_clipboard.first().groupId.isEmpty())
        newGroupId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    m_selectedElems.clear();
    for (const SlideElement& src : s_clipboard) {
        SlideElement newElem = src;
        newElem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        newElem.x += 20; newElem.y += 20;
        if (!newGroupId.isEmpty()) newElem.groupId = newGroupId;
        else if (s_clipboard.size() == 1) newElem.groupId.clear(); // never silently rejoin a stale group
        s->elements.append(newElem);
        m_selectedElems.append(s->elements.size() - 1);
    }
    m_selectedElem = m_selectedElems.isEmpty() ? -1 : m_selectedElems.last();
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElems.size() == 1 ? m_selectedElem : -1);
    emit elementsSelected(m_selectedElems);
}

void SlideEditor2D::resizeEvent(QResizeEvent*) { update(); }

void SlideEditor2D::dragEnterEvent(QDragEnterEvent* e) {
    const QMimeData* md = e->mimeData();
    if (md->hasUrls() || md->hasImage())
        e->acceptProposedAction();
}

void SlideEditor2D::dropEvent(QDropEvent* e) {
    const QMimeData* md = e->mimeData();
    QPointF dropPos = e->position();

    if (md->hasUrls()) {
        static const QStringList imgExts = {"png","jpg","jpeg","bmp","gif","webp","svg"};
        for (const QUrl& url : md->urls()) {
            if (!url.isLocalFile()) continue;
            QString path = url.toLocalFile();
            QString suffix = QFileInfo(path).suffix().toLower();
            if (imgExts.contains(suffix)) {
                addImageFromPath(path, dropPos);
                e->acceptProposedAction();
                return;
            }
            if (suffix == "mp4") {
                addMediaFromPath(path, SlideElement::Video, dropPos);
                e->acceptProposedAction();
                return;
            }
            if (suffix == "mp3") {
                addMediaFromPath(path, SlideElement::Audio, dropPos);
                e->acceptProposedAction();
                return;
            }
        }
    }
    if (md->hasImage()) {
        QImage img = qvariant_cast<QImage>(md->imageData());
        if (!img.isNull()) {
            QString path = QDir::tempPath() + "/impress_drop_"
                           + QUuid::createUuid().toString(QUuid::Id128) + ".png";
            img.save(path, "PNG");
            addImageFromPath(path, dropPos);
            e->acceptProposedAction();
        }
    }
}

void SlideEditor2D::contextMenuEvent(QContextMenuEvent* e) {
    // Right-click on an existing guide: quick delete
    if (m_showRulers) {
        bool vert;
        int gi = hitGuide(e->pos(), vert);
        if (gi >= 0) {
            Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
            QMenu menu(this);
            QAction* aDel = menu.addAction("Führungslinie löschen");
            if (menu.exec(e->globalPos()) == aDel && s && gi < s->guides.size()) {
                s->guides.remove(gi);
                emit presentationModified();
                update();
            }
            return;
        }
        // Right-click on the ruler band itself: switch tool / toggle guide-to-object snapping
        if (inTopRulerBand(e->pos()) || inLeftRulerBand(e->pos())) {
            QMenu menu(this);
            QAction* aStd     = menu.addAction("Standard-Lineal (Führungslinien)");
            QAction* aCircle  = menu.addAction("Kreis / Zirkel");
            QAction* aMeasure = menu.addAction(QString::fromUtf8("Messger\xC3\xA4t (Abstand && Winkel)"));
            for (QAction* a : {aStd, aCircle, aMeasure}) a->setCheckable(true);
            aStd->setChecked(m_rulerTool == RulerTool::Standard);
            aCircle->setChecked(m_rulerTool == RulerTool::Circle);
            aMeasure->setChecked(m_rulerTool == RulerTool::Measure);
            menu.addSeparator();
            QAction* aSnap = menu.addAction("Neue Führungslinie an Objekte einrasten");
            aSnap->setCheckable(true);
            aSnap->setChecked(m_snapNewGuideToObjects);
            menu.addSeparator();
            QAction* aClear = menu.addAction("Alle Führungslinien/Kreise entfernen");

            QAction* chosen = menu.exec(e->globalPos());
            if (chosen == aStd)          m_rulerTool = RulerTool::Standard;
            else if (chosen == aCircle)  m_rulerTool = RulerTool::Circle;
            else if (chosen == aMeasure) { m_rulerTool = RulerTool::Measure; m_hasMeasureResult = false; }
            else if (chosen == aSnap)    m_snapNewGuideToObjects = aSnap->isChecked();
            else if (chosen == aClear) {
                Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
                if (s) { s->guides.clear(); s->guideCircles.clear(); emit presentationModified(); }
            }
            update();
            return;
        }
    }

    // Table edit mode: show table-specific context menu
    if (m_tableEditMode && m_selectedElem >= 0 && m_pres) {
        const Slide* s = m_pres->slideAt(m_slideIndex);
        if (s && m_selectedElem < s->elements.size()) {
            const SlideElement& tbl = s->elements[m_selectedElem];
            QMenu menu(this);

            bool multiSel = m_cellSelAnchorRow >= 0 && m_selTableRow >= 0
                && (m_cellSelAnchorRow != m_selTableRow || m_cellSelAnchorCol != m_selTableCol);

            bool isSpanning = false;
            if (m_selTableRow >= 0 && m_selTableRow < tbl.tableRows &&
                m_selTableCol >= 0 && m_selTableCol < tbl.tableCols) {
                const TableCell& cell = tbl.tableCells[m_selTableRow][m_selTableCol];
                isSpanning = !cell.merged && (cell.colspan > 1 || cell.rowspan > 1);
            }

            if (multiSel) {
                menu.addAction("Merge Cells", this, &SlideEditor2D::mergeCells);
            }
            if (isSpanning) {
                menu.addAction("Split Cells", this, &SlideEditor2D::unmergeCells);
            }
            if (multiSel || isSpanning) menu.addSeparator();
            menu.addAction("Exit Table", this, &SlideEditor2D::exitTableEditMode);
            menu.exec(e->globalPos());
            return;
        }
    }

    QMenu menu(this);
    QAction* aAutoplay    = nullptr;
    QAction* aWaveformOn  = nullptr;
    QAction* aWaveformOff = nullptr;
    bool isMediaSingle = false;
    if (!m_selectedElems.isEmpty()) {
        const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
        bool isGrouped = s && m_selectedElem >= 0 && m_selectedElem < s->elements.size()
                         && !s->elements[m_selectedElem].groupId.isEmpty();

        // Video/Audio playback options — only shown for a single selected media element.
        if (m_selectedElems.size() == 1 && s && m_selectedElem >= 0 && m_selectedElem < s->elements.size()) {
            const SlideElement& me = s->elements[m_selectedElem];
            if (me.type == SlideElement::Video || me.type == SlideElement::Audio) {
                isMediaSingle = true;
                aAutoplay = menu.addAction("Autostart (beim Erreichen der Folie)");
                aAutoplay->setCheckable(true);
                aAutoplay->setChecked(me.mediaAutoplay);
                if (me.type == SlideElement::Audio) {
                    aWaveformOn = menu.addAction("Anzeige: Wellenform");
                    aWaveformOn->setCheckable(true);
                    aWaveformOn->setChecked(me.audioShowWaveform);
                    aWaveformOff = menu.addAction("Anzeige: Kompakt (Symbol + Dauer)");
                    aWaveformOff->setCheckable(true);
                    aWaveformOff->setChecked(!me.audioShowWaveform);
                }
                menu.addSeparator();
            }
        }

        menu.addAction("Copy  (Ctrl+C)", this, &SlideEditor2D::copySelectedElement);
        menu.addAction("Cut (Ctrl+X)", this, [this]() {
            copySelectedElement(); deleteSelectedElement();
        });
        menu.addSeparator();
        if (m_selectedElems.size() >= 2 && !isGrouped)
            menu.addAction("Gruppieren  (Ctrl+G)", this, &SlideEditor2D::groupSelectedElements);
        if (isGrouped)
            menu.addAction("Gruppierung aufheben  (Ctrl+Shift+G)", this, &SlideEditor2D::ungroupSelectedElements);
        if ((m_selectedElems.size() >= 2 && !isGrouped) || isGrouped)
            menu.addSeparator();
        menu.addAction("Bring to Front",       this, &SlideEditor2D::bringToFront);
        menu.addAction("Move One Layer Up",    this, &SlideEditor2D::bringForward);
        menu.addAction("Move One Layer Down",  this, &SlideEditor2D::sendBackward);
        menu.addAction("Send to Back",         this, &SlideEditor2D::sendToBack);
        menu.addSeparator();
        menu.addAction(isGrouped || m_selectedElems.size() > 1 ? "Delete Group" : "Delete Element",
                       this, &SlideEditor2D::deleteSelectedElement);
        menu.addSeparator();
    }
    auto* pasteAct = menu.addAction("Paste (Ctrl+V)", this, &SlideEditor2D::pasteElement);
    pasteAct->setEnabled(s_hasClipboard);
    QAction* chosen = menu.exec(e->globalPos());

    if (isMediaSingle) {
        Slide* ms = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
        if (ms && m_selectedElem >= 0 && m_selectedElem < ms->elements.size()) {
            SlideElement& me = ms->elements[m_selectedElem];
            if (chosen == aAutoplay) {
                me.mediaAutoplay = aAutoplay->isChecked();
                emit presentationModified();
                update();
            } else if (chosen == aWaveformOn) {
                me.audioShowWaveform = true;
                emit presentationModified();
                update();
            } else if (chosen == aWaveformOff) {
                me.audioShowWaveform = false;
                emit presentationModified();
                update();
            }
        }
    }
}

// ── Element operations ────────────────────────────────────────────────────────

void SlideEditor2D::addTextElement() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.type = SlideElement::Text; e.content = "Enter text";
    e.x = 200; e.y = 200; e.width = 700; e.height = 90; e.fontSize = 36;
    s->elements.append(e);
    setSingleSelection(s->elements.size() - 1);
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::pasteTextAsNewElement(const QString& text) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.type = SlideElement::Text; e.content = text;
    e.x = 200; e.y = 200; e.width = 700; e.height = 90; e.fontSize = 36;
    if (CodeHighlighter::looksLikeCode(text)) {
        CodeSpan sp;
        sp.start    = 0;
        sp.length   = text.length();
        sp.language = CodeHighlighter::guessLanguage(text);
        e.codeSpans << sp;
        e.fontFamily = "Consolas";
    }
    s->elements.append(e);
    setSingleSelection(s->elements.size() - 1);
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::addShapeElement(const QString& shapeType) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.type = SlideElement::Shape; e.content = shapeType;
    e.x = 300; e.y = 250; e.width = 500; e.height = 350;
    e.backgroundColor = QColor(100, 149, 237);
    e.borderWidth     = 0.f;
    s->elements.append(e);
    setSingleSelection(s->elements.size() - 1);
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::addIconElement(const QString& iconId) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || iconId.isEmpty()) return;
    SlideElement e;
    e.type = SlideElement::Icon; e.content = iconId;
    e.x = 400; e.y = 400; e.width = 160; e.height = 160;
    e.color = QColor(55, 65, 81);
    s->elements.append(e);
    setSingleSelection(s->elements.size() - 1);
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::addImageFromPath(const QString& path, QPointF widgetPos) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.type    = SlideElement::Image;
    e.content = path;
    // Determine size from the image itself, scale to reasonable default
    QSize natSize = QPixmap(path).size();
    float aspect  = (natSize.height() > 0) ? float(natSize.width()) / natSize.height() : 1.f;
    e.height = 400.f;
    e.width  = e.height * aspect;
    // Place at drop position, or centered if no position given
    if (widgetPos.x() >= 0) {
        QPointF sp = widgetToSlide(widgetPos);
        e.x = float(sp.x()) - e.width  * 0.5f;
        e.y = float(sp.y()) - e.height * 0.5f;
    } else {
        e.x = 200.f; e.y = 150.f;
    }
    s->elements.append(e);
    setSingleSelection(s->elements.size() - 1);
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::addMediaFromPath(const QString& path, SlideElement::Type type, QPointF widgetPos) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.type    = type;
    e.content = path;
    if (type == SlideElement::Video) { e.width = 640.f; e.height = 360.f; }
    else                             { e.width = 400.f; e.height = 100.f; }
    if (widgetPos.x() >= 0) {
        QPointF sp = widgetToSlide(widgetPos);
        e.x = float(sp.x()) - e.width  * 0.5f;
        e.y = float(sp.y()) - e.height * 0.5f;
    } else {
        e.x = 200.f; e.y = 150.f;
    }
    s->elements.append(e);
    setSingleSelection(s->elements.size() - 1);
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::addVideoElement() {
    QString path = QFileDialog::getOpenFileName(nullptr, "Open Video", {},
        "Videos (*.mp4);;All Files (*)");
    if (path.isEmpty()) return;
    addMediaFromPath(path, SlideElement::Video);
}

void SlideEditor2D::addAudioElement() {
    QString path = QFileDialog::getOpenFileName(nullptr, "Open Audio", {},
        "Audio (*.mp3);;All Files (*)");
    if (path.isEmpty()) return;
    addMediaFromPath(path, SlideElement::Audio);
}

void SlideEditor2D::addTableElement(int rows, int cols) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.initTable(rows, cols);
    e.x = 260.f; e.y = 340.f;
    e.width = 1400.f; e.height = float(rows) * 60.f;
    s->elements.append(e);
    setSingleSelection(s->elements.size() - 1);
    m_tableEditMode = false;
    m_selTableRow = m_selTableCol = -1;
    m_tableCellEditing = false;
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::addChartElement(const QString& chartType) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.type = SlideElement::Chart;
    e.chartData = ChartData::createDefault(chartType);
    e.x = 160.f; e.y = 160.f;
    e.width = 900.f; e.height = 560.f;
    s->elements.append(e);
    setSingleSelection(s->elements.size() - 1);
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::openChartEditor() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return;
    SlideElement& e = s->elements[m_selectedElem];
    if (e.type != SlideElement::Chart) return;

    ChartEditorDialog dlg(e.chartData, this);
    if (dlg.exec() == QDialog::Accepted) {
        e.chartData = dlg.chartData();
        update();
        emit presentationModified();
    }
}

void SlideEditor2D::addFormulaElement(const QString& latex) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.type    = SlideElement::Formula;
    e.content = latex;
    e.x = 300.f; e.y = 300.f; e.width = 500.f; e.height = 120.f; e.fontSize = 40;
    s->elements.append(e);
    setSingleSelection(s->elements.size() - 1);
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::openFormulaEditor() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return;
    SlideElement& e = s->elements[m_selectedElem];
    if (e.type != SlideElement::Formula) return;

    InsertFormulaDialog dlg(this, e.content);
    if (dlg.exec() == QDialog::Accepted) {
        e.content = dlg.latex();
        update();
        emit presentationModified();
    }
}

void SlideEditor2D::addIFrameElement(const QString& url) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.type    = SlideElement::IFrame;
    e.content = url;
    e.x = 300.f; e.y = 300.f; e.width = 800.f; e.height = 450.f;
    s->elements.append(e);
    setSingleSelection(s->elements.size() - 1);
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::openIFrameEditor() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return;
    SlideElement& e = s->elements[m_selectedElem];
    if (e.type != SlideElement::IFrame) return;

    InsertIFrameDialog dlg(this, e.content);
    if (dlg.exec() == QDialog::Accepted) {
        e.content = dlg.url();
        update();
        emit presentationModified();
    }
}

void SlideEditor2D::addButtonElement(const ButtonConfig& cfg) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.type             = SlideElement::Button;
    e.content          = cfg.label.isEmpty() ? "Next" : cfg.label;
    e.targetSlideId    = cfg.targetSlideId;
    e.buttonAction      = cfg.action;
    e.boundVariableId   = cfg.boundVariableId;
    e.varOp             = cfg.varOp;
    e.varOpNumber        = cfg.varOpNumber;
    e.varOpText          = cfg.varOpText;
    e.varOpBool          = cfg.varOpBool;
    e.x = 300.f; e.y = 300.f; e.width = 240.f; e.height = 70.f;
    e.fontSize        = 24;
    e.color           = Qt::white;
    e.backgroundColor = QColor(37, 99, 235);
    e.cornerRadius    = 8.f;
    e.bold            = true;
    s->elements.append(e);
    setSingleSelection(s->elements.size() - 1);
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::openButtonEditor() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return;
    SlideElement& e = s->elements[m_selectedElem];
    if (e.type != SlideElement::Button) return;

    ButtonConfig initial;
    initial.label           = e.content;
    initial.action          = e.buttonAction;
    initial.targetSlideId   = e.targetSlideId;
    initial.boundVariableId = e.boundVariableId;
    initial.varOp           = e.varOp;
    initial.varOpNumber     = e.varOpNumber;
    initial.varOpText       = e.varOpText;
    initial.varOpBool       = e.varOpBool;

    InsertButtonDialog dlg(this, m_pres, s->id, initial);
    if (dlg.exec() == QDialog::Accepted) {
        ButtonConfig cfg = dlg.config();
        e.content          = cfg.label.isEmpty() ? "Next" : cfg.label;
        e.targetSlideId    = cfg.targetSlideId;
        e.buttonAction      = cfg.action;
        e.boundVariableId   = cfg.boundVariableId;
        e.varOp             = cfg.varOp;
        e.varOpNumber        = cfg.varOpNumber;
        e.varOpText          = cfg.varOpText;
        e.varOpBool          = cfg.varOpBool;
        update();
        emit presentationModified();
    }
}

void SlideEditor2D::addCheckboxElement(const CheckboxConfig& cfg) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.type             = SlideElement::Checkbox;
    e.content          = cfg.label;
    e.boundVariableId  = cfg.boundVariableId;
    e.x = 300.f; e.y = 300.f; e.width = 320.f; e.height = 44.f;
    e.fontSize = 24;
    e.color    = Qt::black;
    s->elements.append(e);
    setSingleSelection(s->elements.size() - 1);
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::openCheckboxEditor() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return;
    SlideElement& e = s->elements[m_selectedElem];
    if (e.type != SlideElement::Checkbox) return;

    CheckboxConfig initial;
    initial.label           = e.content;
    initial.boundVariableId = e.boundVariableId;

    InsertCheckboxDialog dlg(this, m_pres, s->id, initial);
    if (dlg.exec() == QDialog::Accepted) {
        CheckboxConfig cfg = dlg.config();
        e.content         = cfg.label;
        e.boundVariableId = cfg.boundVariableId;
        update();
        emit presentationModified();
    }
}

void SlideEditor2D::addSliderElement(const SliderConfig& cfg) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.type             = SlideElement::Slider;
    e.boundVariableId  = cfg.boundVariableId;
    e.sliderMin        = cfg.min;
    e.sliderMax        = cfg.max;
    e.sliderStep       = cfg.step;
    e.x = 300.f; e.y = 300.f; e.width = 400.f; e.height = 70.f;
    e.fontSize = 20;
    e.color    = Qt::black;
    s->elements.append(e);
    setSingleSelection(s->elements.size() - 1);
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::openSliderEditor() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return;
    SlideElement& e = s->elements[m_selectedElem];
    if (e.type != SlideElement::Slider) return;

    SliderConfig initial;
    initial.boundVariableId = e.boundVariableId;
    initial.min  = e.sliderMin;
    initial.max  = e.sliderMax;
    initial.step = e.sliderStep;

    InsertSliderDialog dlg(this, m_pres, s->id, initial);
    if (dlg.exec() == QDialog::Accepted) {
        SliderConfig cfg = dlg.config();
        e.boundVariableId = cfg.boundVariableId;
        e.sliderMin  = cfg.min;
        e.sliderMax  = cfg.max;
        e.sliderStep = cfg.step;
        update();
        emit presentationModified();
    }
}

void SlideEditor2D::addImageElement() {
    QString path = QFileDialog::getOpenFileName(nullptr, "Open Image", {},
        "Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp);;All Files (*)");
    if (path.isEmpty()) return;
    addImageFromPath(path);
}

void SlideEditor2D::deleteSelectedElement() {
    // MainWindow's keyframe-edit session tracks its target by index into
    // Slide::elements; removing/reordering any element (this function or the
    // layer-order ones below) would shift that index and, when the session
    // later ends, write its data into whatever element shifted into the old
    // slot. Closing any active session first — as if "Done" were clicked —
    // commits it against the still-consistent pre-edit array.
    if (m_keyframeEditActive) emit keyframeEditDone();
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElems.isEmpty()) return;
    QVector<int> sorted = m_selectedElems;
    std::sort(sorted.begin(), sorted.end());
    for (int k = sorted.size() - 1; k >= 0; --k) {
        int idx = sorted[k];
        if (idx >= 0 && idx < s->elements.size()) s->elements.removeAt(idx);
    }
    m_selectedElem = -1;
    m_selectedElems.clear();
    update();
    emit presentationModified();
    emit elementSelected(-1);
    emit elementsSelected(m_selectedElems);
}

// ── Grouping ──────────────────────────────────────────────────────────────────

void SlideEditor2D::groupSelectedElements() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElems.size() < 2) return;
    QString gid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    for (int idx : m_selectedElems)
        if (idx >= 0 && idx < s->elements.size()) s->elements[idx].groupId = gid;
    update();
    emit presentationModified();
}

void SlideEditor2D::ungroupSelectedElements() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return;
    const QString gid = s->elements[m_selectedElem].groupId;
    if (gid.isEmpty()) return;
    for (SlideElement& el : s->elements)
        if (el.groupId == gid) el.groupId.clear();
    update();
    emit presentationModified();
}

// ── Layer / z-order ───────────────────────────────────────────────────────────

void SlideEditor2D::bringToFront() {
    // See the comment in deleteSelectedElement(): reordering shifts indices
    // out from under an active keyframe-edit session, so close it first.
    if (m_keyframeEditActive) emit keyframeEditDone();
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size() - 1) return;
    s->elements.move(m_selectedElem, s->elements.size() - 1);
    setSingleSelection(s->elements.size() - 1);
    update(); emit presentationModified(); emit elementSelected(m_selectedElem);
}

void SlideEditor2D::bringForward() {
    if (m_keyframeEditActive) emit keyframeEditDone();
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size() - 1) return;
    s->elements.swapItemsAt(m_selectedElem, m_selectedElem + 1);
    setSingleSelection(m_selectedElem + 1);
    update(); emit presentationModified(); emit elementSelected(m_selectedElem);
}

void SlideEditor2D::sendBackward() {
    if (m_keyframeEditActive) emit keyframeEditDone();
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem <= 0) return;
    s->elements.swapItemsAt(m_selectedElem, m_selectedElem - 1);
    setSingleSelection(m_selectedElem - 1);
    update(); emit presentationModified(); emit elementSelected(m_selectedElem);
}

void SlideEditor2D::sendToBack() {
    if (m_keyframeEditActive) emit keyframeEditDone();
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem <= 0) return;
    s->elements.move(m_selectedElem, 0);
    setSingleSelection(0);
    update(); emit presentationModified(); emit elementSelected(m_selectedElem);
}

// ── Inline text edit (WYSIWYG — direct canvas cursor, no overlay widget) ─────

// Build a QTextLayout for a text element at widget scale and do line layout.
// The layout origin is at (0,0); add elemToWidget(e).topLeft() to get widget coords.
static void buildLayout(QTextLayout& layout, const SlideElement& e,
                        const QFont& font, float width,
                        const QString& alignOverride) {
    layout.setFont(font);
    QTextOption opt;
    const QString& align = alignOverride.isEmpty() ? e.textAlignment : alignOverride;
    if      (align == "center") opt.setAlignment(Qt::AlignHCenter);
    else if (align == "right")  opt.setAlignment(Qt::AlignRight);
    else                        opt.setAlignment(Qt::AlignLeft);
    opt.setWrapMode(QTextOption::WordWrap);
    layout.setTextOption(opt);
    layout.beginLayout();
    float y = 0;
    for (;;) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(width);
        line.setPosition(QPointF(0, y));
        y += line.height();
    }
    layout.endLayout();
}

static QFont elemFont(const SlideElement& e, float scaleY) {
    QFont f(e.fontFamily, qMax(6, int(e.fontSize * scaleY)));
    f.setBold(e.bold); f.setItalic(e.italic);
    return f;
}

// Keep CodeSpan offsets valid across inline text edits (insert/remove at a
// cursor position). Called alongside every text.insert()/text.remove() in
// handleTextEditKey() below.
static int mapOffsetAfterRemoval(int offset, int pos, int len) {
    if (offset <= pos)       return offset;
    if (offset <= pos + len) return pos;
    return offset - len;
}

static void adjustSpansForRemoval(QVector<CodeSpan>& spans, int pos, int len) {
    if (len <= 0) return;
    QVector<CodeSpan> result;
    for (CodeSpan sp : spans) {
        int end      = sp.start + sp.length;
        int newStart = mapOffsetAfterRemoval(sp.start, pos, len);
        int newEnd   = mapOffsetAfterRemoval(end, pos, len);
        sp.start  = newStart;
        sp.length = newEnd - newStart;
        if (sp.length > 0) result << sp;
    }
    spans = result;
}

// Insertion at `pos`: offsets at/after pos shift by `len`, except when pos
// falls strictly inside a span (or exactly at its end), where the newly
// typed text is treated as a continuation of that code span.
static void adjustSpansForInsertion(QVector<CodeSpan>& spans, int pos, int len) {
    if (len <= 0) return;
    for (CodeSpan& sp : spans) {
        int end = sp.start + sp.length;
        if (pos <= sp.start)      sp.start += len;
        else if (pos <= end)      sp.length += len;
    }
}

void SlideEditor2D::startTextEdit(int idx, QPointF clickPos) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || idx < 0 || idx >= s->elements.size()) return;
    SlideElement& elem = s->elements[idx];
    if (elem.type != SlideElement::Text && elem.type != SlideElement::Shape) return;
    finishTextEdit();
    m_editingElem      = idx;
    m_editingShapeText = (elem.type == SlideElement::Shape);

    const QString& text = m_editingShapeText ? elem.shapeText : elem.content;
    if (clickPos.x() >= 0)
        m_cursorPos = textPositionAt(elem, clickPos);
    else
        m_cursorPos = text.length();

    m_selAnchor     = -1;
    m_cursorVisible = true;
    m_cursorBlink->start();
    setFocus();
    update();
    emit textSelectionChanged(m_cursorPos, m_selAnchor);
}

const QString& SlideEditor2D::getEditText(const SlideElement& e) const {
    return m_editingShapeText ? e.shapeText : e.content;
}

void SlideEditor2D::exitTableEditMode() {
    m_tableEditMode       = false;
    m_selTableRow         = -1;
    m_selTableCol         = -1;
    m_tableCellEditing    = false;
    m_tableCursorPos      = 0;
    m_tableSelAnchor      = -1;
    m_cellSelAnchorRow    = -1;
    m_cellSelAnchorCol    = -1;
    m_isDraggingCellSel   = false;
    m_cursorBlink->stop();
    m_cursorVisible = false;
    update();
    emit tableCellSelected(-1, -1);
}

void SlideEditor2D::mergeCells() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return;
    SlideElement& e = s->elements[m_selectedElem];
    if (e.type != SlideElement::Table) return;
    if (m_cellSelAnchorRow < 0 || m_selTableRow < 0) return;

    int r1 = qMin(m_cellSelAnchorRow, m_selTableRow);
    int c1 = qMin(m_cellSelAnchorCol, m_selTableCol);
    int r2 = qMax(m_cellSelAnchorRow, m_selTableRow);
    int c2 = qMax(m_cellSelAnchorCol, m_selTableCol);
    if (r1 == r2 && c1 == c2) return;

    // Collect all non-empty texts
    QStringList parts;
    for (int r = r1; r <= r2; ++r)
        for (int c = c1; c <= c2; ++c)
            if (r < e.tableCells.size() && c < e.tableCells[r].size())
                if (!e.tableCells[r][c].text.isEmpty())
                    parts << e.tableCells[r][c].text;

    // Set top-left as spanning cell
    TableCell& anchor = e.tableCells[r1][c1];
    anchor.colspan = c2 - c1 + 1;
    anchor.rowspan = r2 - r1 + 1;
    anchor.merged  = false;
    anchor.text    = parts.join(" ");

    // Mark all other cells in range as merged
    for (int r = r1; r <= r2; ++r) {
        for (int c = c1; c <= c2; ++c) {
            if (r == r1 && c == c1) continue;
            if (r < e.tableCells.size() && c < e.tableCells[r].size()) {
                TableCell& tc = e.tableCells[r][c];
                tc.merged  = true;
                tc.text    = "";
                tc.colspan = 1;
                tc.rowspan = 1;
            }
        }
    }

    // Collapse selection to the spanning cell
    m_selTableRow = r1; m_selTableCol = c1;
    m_cellSelAnchorRow = r1; m_cellSelAnchorCol = c1;
    m_tableCellEditing = false;
    emit presentationModified();
    update();
}

void SlideEditor2D::unmergeCells() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return;
    SlideElement& e = s->elements[m_selectedElem];
    if (e.type != SlideElement::Table) return;
    if (m_selTableRow < 0 || m_selTableCol < 0) return;

    TableCell& cell = e.tableCells[m_selTableRow][m_selTableCol];
    if (cell.merged || (cell.colspan <= 1 && cell.rowspan <= 1)) return;

    int r1 = m_selTableRow, c1 = m_selTableCol;
    int r2 = qMin(r1 + cell.rowspan - 1, e.tableRows - 1);
    int c2 = qMin(c1 + cell.colspan - 1, e.tableCols - 1);

    cell.colspan = 1;
    cell.rowspan = 1;

    for (int r = r1; r <= r2; ++r) {
        for (int c = c1; c <= c2; ++c) {
            if (r == r1 && c == c1) continue;
            if (r < e.tableCells.size() && c < e.tableCells[r].size()) {
                TableCell& tc = e.tableCells[r][c];
                tc.merged  = false;
                tc.colspan = 1;
                tc.rowspan = 1;
            }
        }
    }
    emit presentationModified();
    update();
}

void SlideEditor2D::finishTextEdit() {
    if (m_editingElem < 0) return;
    m_cursorBlink->stop();
    m_editingElem      = -1;
    m_editingShapeText = false;
    m_cursorPos        = 0;
    m_selAnchor        = -1;
    m_textSelecting    = false;
    m_cursorVisible    = false;
    update();
    emit textSelectionChanged(-1, -1);
}

void SlideEditor2D::handleTextEditKey(QKeyEvent* ke) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_editingElem < 0 || m_editingElem >= s->elements.size()) return;

    // Emits textSelectionChanged with the final cursor/selection on every
    // return path below, so FormatBar's Code button/language combo stay in
    // sync without instrumenting each of this function's many early returns.
    struct SelectionNotifier {
        SlideEditor2D* self;
        ~SelectionNotifier() { emit self->textSelectionChanged(self->m_cursorPos, self->m_selAnchor); }
    } notifyOnExit{this};

    SlideElement& elem = s->elements[m_editingElem];
    QString& text = m_editingShapeText ? elem.shapeText : elem.content;

    m_cursorVisible = true;
    m_cursorBlink->start();

    const bool alt      = ke->modifiers() & Qt::AltModifier;
    const bool ctrl     = ke->modifiers() & Qt::ControlModifier;
    const bool shift    = ke->modifiers() & Qt::ShiftModifier;
    const bool ctrlOnly = ctrl && !alt; // true Ctrl, NOT AltGr (which is Ctrl+Alt on Windows)

    // Helper: delete the current selection and reset anchor
    auto deleteSelection = [&]() {
        if (m_selAnchor < 0 || m_selAnchor == m_cursorPos) return;
        int lo = qMin(m_cursorPos, m_selAnchor);
        int hi = qMax(m_cursorPos, m_selAnchor);
        text.remove(lo, hi - lo);
        if (!m_editingShapeText) adjustSpansForRemoval(elem.codeSpans, lo, hi - lo);
        m_cursorPos = lo;
        m_selAnchor = -1;
    };

    // Helper: build layout for Up/Down movement
    auto makeLayout = [&]() -> std::pair<QTextLayout*, float> {
        QRectF wr = elemToWidget(elem);
        float sy = slideRect().height() / SLIDE_H_DEFAULT;
        QFont font = elemFont(elem, sy);
        auto* layout = new QTextLayout(text, font);
        buildLayout(*layout, elem, font, float(wr.width()));
        return {layout, float(wr.height())};
    };

    if (ke->key() == Qt::Key_Escape) { finishTextEdit(); return; }

    // ── Ctrl+A ───────────────────────────────────────────────────────────
    if (ctrlOnly && ke->key() == Qt::Key_A) {
        m_selAnchor = 0; m_cursorPos = text.length(); update(); return;
    }

    // ── Ctrl+C / Ctrl+X: copy (or cut) the selected text to the clipboard ──
    if (ctrlOnly && (ke->key() == Qt::Key_C || ke->key() == Qt::Key_X)) {
        if (m_selAnchor >= 0 && m_selAnchor != m_cursorPos) {
            int lo = qMin(m_cursorPos, m_selAnchor);
            int hi = qMax(m_cursorPos, m_selAnchor);
            QApplication::clipboard()->setText(text.mid(lo, hi - lo));
            if (ke->key() == Qt::Key_X) {
                deleteSelection();
                emit presentationModified();
            }
            update();
        }
        return;
    }

    // ── Ctrl+V: paste clipboard text into this field at the cursor ─────────
    if (ctrlOnly && ke->key() == Qt::Key_V) {
        QString clip = QApplication::clipboard()->text();
        if (!clip.isEmpty()) {
            if (m_selAnchor >= 0) deleteSelection();
            int insertPos = m_cursorPos;
            text.insert(insertPos, clip);
            if (!m_editingShapeText) {
                adjustSpansForInsertion(elem.codeSpans, insertPos, clip.length());
                // Pasted text that looks like source code auto-becomes a code
                // block: replace/trim any spans it overlaps and mark the whole
                // pasted range with a guessed language (same "replace" rule
                // FormatBar::onCodeToggled uses for manual toggling).
                if (elem.type == SlideElement::Text && elem.listStyle == SlideElement::NoList
                    && CodeHighlighter::looksLikeCode(clip)) {
                    QVector<CodeSpan> kept;
                    for (const CodeSpan& sp : elem.codeSpans)
                        if (sp.start + sp.length <= insertPos || sp.start >= insertPos + clip.length())
                            kept << sp;
                    CodeSpan added;
                    added.start    = insertPos;
                    added.length   = clip.length();
                    added.language = CodeHighlighter::guessLanguage(clip);
                    kept << added;
                    elem.codeSpans = kept;
                }
            }
            m_cursorPos += clip.length();
            emit presentationModified();
            update();
        }
        return;
    }

    // ── Navigation (Left / Right) ─────────────────────────────────────────
    if (ke->key() == Qt::Key_Left) {
        if (shift) {
            if (m_selAnchor < 0) m_selAnchor = m_cursorPos;
            if (m_cursorPos > 0) m_cursorPos--;
            if (m_selAnchor == m_cursorPos) m_selAnchor = -1;
        } else {
            // If selection exists, jump to lower bound; else move left
            if (m_selAnchor >= 0) { m_cursorPos = qMin(m_cursorPos, m_selAnchor); m_selAnchor = -1; }
            else if (m_cursorPos > 0) m_cursorPos--;
        }
        update(); return;
    }
    if (ke->key() == Qt::Key_Right) {
        if (shift) {
            if (m_selAnchor < 0) m_selAnchor = m_cursorPos;
            if (m_cursorPos < text.length()) m_cursorPos++;
            if (m_selAnchor == m_cursorPos) m_selAnchor = -1;
        } else {
            if (m_selAnchor >= 0) { m_cursorPos = qMax(m_cursorPos, m_selAnchor); m_selAnchor = -1; }
            else if (m_cursorPos < text.length()) m_cursorPos++;
        }
        update(); return;
    }
    if (ke->key() == Qt::Key_Home) {
        if (shift) { if (m_selAnchor < 0) m_selAnchor = m_cursorPos; m_cursorPos = 0; if (m_selAnchor == m_cursorPos) m_selAnchor = -1; }
        else { m_selAnchor = -1; m_cursorPos = 0; }
        update(); return;
    }
    if (ke->key() == Qt::Key_End) {
        if (shift) { if (m_selAnchor < 0) m_selAnchor = m_cursorPos; m_cursorPos = text.length(); if (m_selAnchor == m_cursorPos) m_selAnchor = -1; }
        else { m_selAnchor = -1; m_cursorPos = text.length(); }
        update(); return;
    }

    // ── Up / Down ─────────────────────────────────────────────────────────
    if (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down) {
        auto [layout, elemH] = makeLayout();
        QTextLine cur = layout->lineForTextPosition(m_cursorPos);
        if (cur.isValid()) {
            float cx = cur.cursorToX(m_cursorPos);
            int ln = cur.lineNumber() + (ke->key() == Qt::Key_Down ? 1 : -1);
            int newPos;
            if (ln >= 0 && ln < layout->lineCount())
                newPos = layout->lineAt(ln).xToCursor(cx, QTextLine::CursorBetweenCharacters);
            else
                newPos = (ke->key() == Qt::Key_Up) ? 0 : text.length();
            if (shift) { if (m_selAnchor < 0) m_selAnchor = m_cursorPos; m_cursorPos = newPos; if (m_selAnchor == m_cursorPos) m_selAnchor = -1; }
            else { m_selAnchor = -1; m_cursorPos = newPos; }
        }
        delete layout; update(); return;
    }

    // ── Backspace / Delete ────────────────────────────────────────────────
    if (ke->key() == Qt::Key_Backspace) {
        if (m_selAnchor >= 0 && m_selAnchor != m_cursorPos) deleteSelection();
        else if (m_cursorPos > 0) {
            text.remove(m_cursorPos - 1, 1);
            if (!m_editingShapeText) adjustSpansForRemoval(elem.codeSpans, m_cursorPos - 1, 1);
            m_cursorPos--;
        }
        emit presentationModified(); update(); return;
    }
    if (ke->key() == Qt::Key_Delete) {
        if (m_selAnchor >= 0 && m_selAnchor != m_cursorPos) deleteSelection();
        else if (m_cursorPos < text.length()) {
            text.remove(m_cursorPos, 1);
            if (!m_editingShapeText) adjustSpansForRemoval(elem.codeSpans, m_cursorPos, 1);
        }
        emit presentationModified(); update(); return;
    }

    // ── Enter ─────────────────────────────────────────────────────────────
    if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
        if (m_selAnchor >= 0) deleteSelection();
        text.insert(m_cursorPos, '\n');
        if (!m_editingShapeText) adjustSpansForInsertion(elem.codeSpans, m_cursorPos, 1);
        m_cursorPos++;
        emit presentationModified(); update(); return;
    }

    // ── Printable character (incl. AltGr combos) ─────────────────────────
    QString ch = ke->text();
    if (!ch.isEmpty() && ch[0].isPrint()) {
        if (m_selAnchor >= 0) deleteSelection();
        text.insert(m_cursorPos, ch);
        if (!m_editingShapeText) adjustSpansForInsertion(elem.codeSpans, m_cursorPos, ch.length());
        m_cursorPos += ch.length();
        emit presentationModified(); update(); return;
    }

    // ── Other Ctrl shortcuts → parent window (Ctrl+S, Ctrl+Z …) ─────────
    if (ctrlOnly) QWidget::keyPressEvent(ke);
}

void SlideEditor2D::pasteExcelIntoTable(SlideElement& e) {
    QString text = QApplication::clipboard()->text();
    if (text.isEmpty()) return;

    int startRow = qMax(0, m_selTableRow);
    int startCol = qMax(0, m_selTableCol);

    QStringList lines = text.split('\n');
    for (int lr = 0; lr < lines.size(); ++lr) {
        QString line = lines[lr];
        if (line.endsWith('\r')) line.chop(1); // Windows \r\n
        if (line.isEmpty() && lr == lines.size() - 1) continue; // trailing newline
        int tr = startRow + lr;
        if (tr >= e.tableRows) break;
        QStringList cells = line.split('\t');
        for (int lc = 0; lc < cells.size(); ++lc) {
            int tc = startCol + lc;
            if (tc >= e.tableCols) break;
            e.tableCells[tr][tc].text = cells[lc];
        }
    }
    emit presentationModified();
    update();
}

void SlideEditor2D::handleTableKey(QKeyEvent* ke) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return;
    SlideElement& e = s->elements[m_selectedElem];
    if (e.type != SlideElement::Table) return;

    const bool ctrl  = ke->modifiers() & Qt::ControlModifier;
    const bool shift = ke->modifiers() & Qt::ShiftModifier;

    // ── Ctrl+V: paste Excel / TSV / plain text into table ────────────────────
    if (ctrl && ke->key() == Qt::Key_V) {
        QString text = QApplication::clipboard()->text();
        if (!text.isEmpty()) { pasteExcelIntoTable(e); return; }
    }

    // ── Escape: exit table edit mode ─────────────────────────────────────────
    if (ke->key() == Qt::Key_Escape) {
        if (m_tableCellEditing) {
            m_tableCellEditing = false;
            m_tableCursorPos   = 0;
            m_tableSelAnchor   = -1;
            m_cursorBlink->stop();
            m_cursorVisible = false;
            update();
        } else {
            exitTableEditMode();
        }
        return;
    }

    // ── Navigation between cells ──────────────────────────────────────────────
    auto moveTo = [&](int nr, int nc) {
        if (nr < 0 || nr >= e.tableRows || nc < 0 || nc >= e.tableCols) return;
        m_selTableRow = nr;
        m_selTableCol = nc;
        m_tableCellEditing = true;
        m_tableCursorPos   = e.tableCells[nr][nc].text.length();
        m_tableSelAnchor   = -1;
        m_cursorBlink->start();
        m_cursorVisible = true;
        update();
        emit tableCellSelected(nr, nc);
    };

    if (!m_tableCellEditing) {
        // Arrow keys / Tab navigate cells
        if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Right) {
            int nc = m_selTableCol + 1;
            int nr = m_selTableRow;
            if (nc >= e.tableCols) { nc = 0; ++nr; }
            moveTo(nr, nc); return;
        }
        if (ke->key() == Qt::Key_Left) {
            int nc = m_selTableCol - 1;
            int nr = m_selTableRow;
            if (nc < 0) { nc = e.tableCols - 1; --nr; }
            moveTo(nr, nc); return;
        }
        if (ke->key() == Qt::Key_Down || ke->key() == Qt::Key_Return) {
            moveTo(m_selTableRow + 1, m_selTableCol); return;
        }
        if (ke->key() == Qt::Key_Up) {
            moveTo(m_selTableRow - 1, m_selTableCol); return;
        }
        // Any printable char starts editing
        QString ch = ke->text();
        if (!ch.isEmpty() && ch[0].isPrint() && !ctrl) {
            if (m_selTableRow < 0 || m_selTableRow >= e.tableRows ||
                m_selTableCol < 0 || m_selTableCol >= e.tableCols) return;
            TableCell& cell = e.tableCells[m_selTableRow][m_selTableCol];
            cell.text = ch;
            m_tableCellEditing = true;
            m_tableCursorPos   = ch.length();
            m_tableSelAnchor   = -1;
            m_cursorBlink->start();
            m_cursorVisible = true;
            emit presentationModified();
            update();
        }
        return;
    }

    // ── Cell text editing ─────────────────────────────────────────────────────
    if (m_selTableRow < 0 || m_selTableRow >= e.tableRows ||
        m_selTableCol < 0 || m_selTableCol >= e.tableCols) return;
    TableCell& cell = e.tableCells[m_selTableRow][m_selTableCol];
    QString&   text = cell.text;

    m_cursorVisible = true;
    m_cursorBlink->start();

    auto deleteSelection = [&]() {
        if (m_tableSelAnchor < 0 || m_tableSelAnchor == m_tableCursorPos) return;
        int lo = qMin(m_tableCursorPos, m_tableSelAnchor);
        int hi = qMax(m_tableCursorPos, m_tableSelAnchor);
        text.remove(lo, hi - lo);
        m_tableCursorPos = lo;
        m_tableSelAnchor = -1;
    };

    if (ke->key() == Qt::Key_Tab) {
        // finish cell and move right
        m_tableCellEditing = false;
        int nc = m_selTableCol + 1;
        int nr = m_selTableRow;
        if (nc >= e.tableCols) { nc = 0; ++nr; }
        moveTo(nr, nc); return;
    }
    if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
        // finish cell and move down
        m_tableCellEditing = false;
        moveTo(m_selTableRow + 1, m_selTableCol); return;
    }
    if (ke->key() == Qt::Key_Left) {
        if (shift) {
            if (m_tableSelAnchor < 0) m_tableSelAnchor = m_tableCursorPos;
            if (m_tableCursorPos > 0) m_tableCursorPos--;
            if (m_tableSelAnchor == m_tableCursorPos) m_tableSelAnchor = -1;
        } else {
            m_tableSelAnchor = -1;
            if (m_tableCursorPos > 0) m_tableCursorPos--;
        }
        update(); return;
    }
    if (ke->key() == Qt::Key_Right) {
        if (shift) {
            if (m_tableSelAnchor < 0) m_tableSelAnchor = m_tableCursorPos;
            if (m_tableCursorPos < text.length()) m_tableCursorPos++;
            if (m_tableSelAnchor == m_tableCursorPos) m_tableSelAnchor = -1;
        } else {
            m_tableSelAnchor = -1;
            if (m_tableCursorPos < text.length()) m_tableCursorPos++;
        }
        update(); return;
    }
    if (ke->key() == Qt::Key_Home) {
        if (shift) { if (m_tableSelAnchor < 0) m_tableSelAnchor = m_tableCursorPos; m_tableCursorPos = 0; if (m_tableSelAnchor == m_tableCursorPos) m_tableSelAnchor = -1; }
        else { m_tableSelAnchor = -1; m_tableCursorPos = 0; }
        update(); return;
    }
    if (ke->key() == Qt::Key_End) {
        if (shift) { if (m_tableSelAnchor < 0) m_tableSelAnchor = m_tableCursorPos; m_tableCursorPos = text.length(); if (m_tableSelAnchor == m_tableCursorPos) m_tableSelAnchor = -1; }
        else { m_tableSelAnchor = -1; m_tableCursorPos = text.length(); }
        update(); return;
    }
    if (ctrl && ke->key() == Qt::Key_A) {
        m_tableSelAnchor = 0; m_tableCursorPos = text.length(); update(); return;
    }
    if (ke->key() == Qt::Key_Backspace) {
        if (m_tableSelAnchor >= 0 && m_tableSelAnchor != m_tableCursorPos) deleteSelection();
        else if (m_tableCursorPos > 0) { text.remove(m_tableCursorPos - 1, 1); m_tableCursorPos--; }
        emit presentationModified(); update(); return;
    }
    if (ke->key() == Qt::Key_Delete) {
        if (m_tableSelAnchor >= 0 && m_tableSelAnchor != m_tableCursorPos) deleteSelection();
        else if (m_tableCursorPos < text.length()) text.remove(m_tableCursorPos, 1);
        emit presentationModified(); update(); return;
    }
    // Printable character
    const bool alt    = ke->modifiers() & Qt::AltModifier;
    const bool ctrlOnly = ctrl && !alt;
    QString ch = ke->text();
    if (!ch.isEmpty() && ch[0].isPrint() && !ctrlOnly) {
        if (m_tableSelAnchor >= 0) deleteSelection();
        text.insert(m_tableCursorPos, ch);
        m_tableCursorPos += ch.length();
        emit presentationModified(); update(); return;
    }
    if (ctrlOnly) QWidget::keyPressEvent(ke);
}

// Compute vertical offset caused by middle/bottom alignment of text within element rect.
static float textVOff(const QTextLayout& layout, float elemH, const QString& vAlign) {
    if (layout.lineCount() == 0 || vAlign.isEmpty() || vAlign == "top") return 0;
    QTextLine last = layout.lineAt(layout.lineCount() - 1);
    float totalH = float(last.y() + last.height());
    if (vAlign == "middle") return qMax(0.f, (elemH - totalH) * 0.5f);
    if (vAlign == "bottom") return qMax(0.f, elemH - totalH);
    return 0;
}

void SlideEditor2D::drawTextCursor(QPainter& p, const SlideElement& e) const {
    QRectF wr = elemToWidget(e);
    p.save();

    // Apply rotation for shape text editing
    bool hasRot = (m_editingShapeText && e.rotation != 0.f);
    if (hasRot) {
        p.translate(wr.center());
        p.rotate(double(e.rotation));
        p.translate(-wr.center());
    }

    // Blue editing border
    p.setPen(QPen(QColor(37, 99, 235), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRect(wr);

    const QString& editText = getEditText(e);
    const QString& vAlignStr = m_editingShapeText ? QString("middle") : e.verticalAlignment;
    const QString& alignStr  = m_editingShapeText ? QString("center") : QString();

    float sy = slideRect().height() / SLIDE_H_DEFAULT;
    QFont font = elemFont(e, sy);
    QTextLayout layout(editText, font);
    buildLayout(layout, e, font, float(wr.width()), alignStr);

    float vOff = textVOff(layout, float(wr.height()), vAlignStr);

    // ── Selection highlight ───────────────────────────────────────────────
    if (m_selAnchor >= 0 && m_selAnchor != m_cursorPos) {
        int selLo = qMin(m_cursorPos, m_selAnchor);
        int selHi = qMax(m_cursorPos, m_selAnchor);
        QColor selColor(37, 99, 235, 80);
        for (int i = 0; i < layout.lineCount(); ++i) {
            QTextLine ln = layout.lineAt(i);
            int lnStart = ln.textStart();
            int lnEnd   = lnStart + ln.textLength();
            if (lnEnd <= selLo || lnStart >= selHi) continue;
            float x1 = float(ln.cursorToX(qMax(selLo, lnStart)));
            float x2 = float(ln.cursorToX(qMin(selHi, lnEnd)));
            float ry  = float(wr.y()) + vOff + float(ln.y());
            p.fillRect(QRectF(wr.x() + x1, ry, x2 - x1, ln.height()), selColor);
        }
    }

    // ── Cursor line ───────────────────────────────────────────────────────
    if (m_cursorVisible) {
        int pos = qBound(0, m_cursorPos, editText.length());
        QTextLine line = layout.lineForTextPosition(pos);
        if (!line.isValid() && layout.lineCount() > 0)
            line = layout.lineAt(layout.lineCount() - 1);
        if (line.isValid()) {
            float cx = float(wr.x()) + float(line.cursorToX(pos));
            float cy = float(wr.y()) + vOff + float(line.y());
            float ch = float(line.height());
            QColor cursorColor = e.color.isValid() ? e.color : Qt::black;
            p.setPen(QPen(cursorColor, 2));
            p.drawLine(QPointF(cx, cy + 1), QPointF(cx, cy + ch - 1));
        }
    }

    p.restore();
}

int SlideEditor2D::textPositionAt(const SlideElement& e, QPointF widgetPos) const {
    QRectF wr = elemToWidget(e);

    // Undo rotation applied when drawing shape text (see drawTextCursor)
    QPointF pos = widgetPos;
    if (m_editingShapeText && e.rotation != 0.f) {
        QPointF center = wr.center();
        QTransform t;
        t.translate(center.x(), center.y());
        t.rotate(-double(e.rotation));
        t.translate(-center.x(), -center.y());
        pos = t.map(widgetPos);
    }

    float sy = slideRect().height() / SLIDE_H_DEFAULT;
    QFont font = elemFont(e, sy);
    const QString& text     = getEditText(e);
    const QString& vAlignStr = m_editingShapeText ? QString("middle") : e.verticalAlignment;
    const QString& alignStr  = m_editingShapeText ? QString("center") : QString();

    QTextLayout layout(text, font);
    buildLayout(layout, e, font, float(wr.width()), alignStr);

    float vOff = textVOff(layout, float(wr.height()), vAlignStr);

    // Adjust click position by vertical alignment offset
    QPointF local = pos - wr.topLeft();
    local.setY(local.y() - vOff);

    int bestLine = layout.lineCount() > 0 ? 0 : -1;
    float bestDist = 1e9f;
    for (int i = 0; i < layout.lineCount(); ++i) {
        QTextLine ln = layout.lineAt(i);
        float mid = float(ln.y() + ln.height() * 0.5f);
        float d   = qAbs(float(local.y()) - mid);
        if (d < bestDist) { bestDist = d; bestLine = i; }
    }
    if (bestLine < 0) return 0;
    return layout.lineAt(bestLine).xToCursor(float(local.x()), QTextLine::CursorBetweenCharacters);
}
