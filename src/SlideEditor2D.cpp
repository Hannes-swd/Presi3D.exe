#include "SlideEditor2D.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QApplication>
#include <QClipboard>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QMenu>
#include <QFrame>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextFormat>
#include <QUuid>
#include <QImageWriter>

SlideElement SlideEditor2D::s_clipboard;
bool         SlideEditor2D::s_hasClipboard = false;

// Defaults — overridden by m_pres->slideWidth/slideHeight when available
static constexpr float SLIDE_W_DEFAULT = 1920.f;
static constexpr float SLIDE_H_DEFAULT = 1080.f;
static constexpr float HANDLE_R  = 9.f;   // half-size of hit area for handles
static constexpr float HANDLE_V  = 7.f;   // visual half-size of handle squares
static constexpr float MIN_SIZE  = 20.f;  // minimum element size
static constexpr float SNAP_PX   = 10.f;  // snap threshold in screen pixels

// ── Handle index layout ───────────────────────────────────────────────────────
// 0=TL  4=TC  1=TR
// 6=ML        7=MR
// 2=BL  5=BC  3=BR

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

// ── Constructor ───────────────────────────────────────────────────────────────

SlideEditor2D::SlideEditor2D(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setBackgroundRole(QPalette::Dark);
    setAutoFillBackground(true);
    setMouseTracking(true);

    setAcceptDrops(true);

    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->hide();
    m_textEdit->setFrameShape(QFrame::NoFrame);
    m_textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textEdit->setStyleSheet("background: rgba(255,255,200,220); font-size: 14px;");
    m_textEdit->installEventFilter(this);
}

// ── Public API ────────────────────────────────────────────────────────────────

void SlideEditor2D::setSlide(Presentation* pres, int slideIndex) {
    finishTextEdit();
    if (pres != m_pres) m_pixmapCache.clear();
    m_pres         = pres;
    m_slideIndex   = slideIndex;
    m_selectedElem = -1;
    m_resizingHandle = -1;
    update();
    emit elementSelected(-1);
}

// ── Coordinate helpers ────────────────────────────────────────────────────────

QRectF SlideEditor2D::slideRect() const {
    const float margin = 20.f;
    const float aw = width()  - 2 * margin;
    const float ah = height() - 2 * margin;
    const float scale = qMin(aw / SLIDE_W_DEFAULT, ah / SLIDE_H_DEFAULT);
    const float sw = SLIDE_W_DEFAULT * scale;
    const float sh = SLIDE_H_DEFAULT * scale;
    return QRectF((width()  - sw) / 2.f,
                  (height() - sh) / 2.f, sw, sh);
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
    for (int i = s->elements.size() - 1; i >= 0; --i)
        if (elemToWidget(s->elements[i]).contains(wpos)) return i;
    return -1;
}

int SlideEditor2D::hitHandle(const QPointF& wpos) const {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return -1;
    QRectF wr = elemToWidget(s->elements[m_selectedElem]);
    const auto pts = handlePoints(wr);
    for (int i = 0; i < pts.size(); ++i) {
        QRectF hr(pts[i].x() - HANDLE_R, pts[i].y() - HANDLE_R,
                  HANDLE_R * 2, HANDLE_R * 2);
        if (hr.contains(wpos)) return i;
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

// ── Text edit event filter ────────────────────────────────────────────────────

bool SlideEditor2D::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_textEdit) {
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape) { finishTextEdit(); return true; }
        } else if (event->type() == QEvent::FocusOut) {
            if (m_editingElem >= 0) finishTextEdit();
        }
    }
    return QWidget::eventFilter(obj, event);
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
        if (i == m_selectedElem) continue;
        const SlideElement& o = s->elements[i];
        xCands << o.x << (o.x + o.width  * 0.5f) << (o.x + o.width);
        yCands << o.y << (o.y + o.height * 0.5f) << (o.y + o.height);
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
    for (int i = 0; i < slide->elements.size(); ++i)
        drawElement(p, slide->elements[i], i == m_selectedElem);
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

    // Selection handles (drawn outside clip so they extend beyond slide edge)
    if (m_selectedElem >= 0 && m_selectedElem < slide->elements.size())
        drawHandles(p, elemToWidget(slide->elements[m_selectedElem]));
}

void SlideEditor2D::drawElement(QPainter& p, const SlideElement& e, bool selected) const {
    QRectF wr = elemToWidget(e);
    QRectF sr = slideRect();
    float scaleY = sr.height() / SLIDE_H_DEFAULT;

    if (e.type == SlideElement::Text) {
        if (e.backgroundColor.isValid() && e.backgroundColor != Qt::transparent)
            p.fillRect(wr, e.backgroundColor);

        if (wr.width() < 1.0 || wr.height() < 1.0) return;

        QString displayText = e.content;
        if (e.listStyle != SlideElement::NoList) {
            QStringList lines = e.content.split('\n');
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
        font.setUnderline(e.underline);
        font.setStrikeOut(e.strikethrough);

        Qt::Alignment align = Qt::AlignLeft;
        if (e.textAlignment == "center") align = Qt::AlignHCenter;
        else if (e.textAlignment == "right") align = Qt::AlignRight;

        Qt::Alignment valign = Qt::AlignTop;
        if (e.verticalAlignment == "middle") valign = Qt::AlignVCenter;
        else if (e.verticalAlignment == "bottom") valign = Qt::AlignBottom;

        p.save();
        p.setFont(font);
        p.setPen(e.color.isValid() ? e.color : Qt::black);
        p.setClipRect(wr, Qt::IntersectClip);
        p.drawText(wr.toRect(),
                   int(Qt::TextWordWrap | valign | align),
                   displayText);
        p.restore();

    } else if (e.type == SlideElement::Shape) {
        p.setPen(e.borderWidth > 0
                 ? QPen(e.borderColor.isValid() ? e.borderColor : Qt::darkGray, e.borderWidth)
                 : Qt::NoPen);
        p.setBrush(e.backgroundColor == Qt::transparent ? Qt::NoBrush : QBrush(e.backgroundColor));
        QRectF sr2 = slideRect();
        float rx = e.cornerRadius * sr2.width()  / SLIDE_W_DEFAULT;
        float ry = e.cornerRadius * sr2.height() / SLIDE_H_DEFAULT;
        if      (e.content == "circle") p.drawEllipse(wr);
        else if (e.content == "line")   p.drawLine(wr.topLeft(), wr.bottomRight());
        else if (rx > 0 || ry > 0)      p.drawRoundedRect(wr, rx, ry);
        else                            p.drawRect(wr);

    } else if (e.type == SlideElement::Image) {
        if (!e.content.isEmpty()) {
            if (!m_pixmapCache.contains(e.content))
                m_pixmapCache[e.content] = QPixmap(e.content);
            const QPixmap& px = m_pixmapCache[e.content];
            if (!px.isNull()) { p.drawPixmap(wr.toRect(), px); goto done; }
        }
        p.fillRect(wr, QColor(180, 180, 200));
        p.setPen(Qt::darkGray);
        p.drawText(wr, Qt::AlignCenter,
                   e.content.isEmpty() ? "[Bild]" : QFileInfo(e.content).fileName());
    }

done:
    // Dashed outline for selected element (inside the clip area)
    if (selected) {
        p.setPen(QPen(QColor(0, 120, 215), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(wr);
    }
}

void SlideEditor2D::drawHandles(QPainter& p, const QRectF& r) const {
    // Blue selection border
    p.setPen(QPen(QColor(0, 120, 215), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(r);

    // 8 resize handles
    const auto pts = handlePoints(r);
    for (const auto& pt : pts) {
        // White fill with blue border
        p.setPen(QPen(QColor(0, 120, 215), 1.5));
        p.setBrush(Qt::white);
        p.drawRect(QRectF(pt.x() - HANDLE_V, pt.y() - HANDLE_V,
                          HANDLE_V * 2,       HANDLE_V * 2));
    }
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
    if (e->button() != Qt::LeftButton) return;

    // Format painter mode: apply format to clicked element
    if (m_formatPainterMode) {
        int hit = hitTest(e->position());
        if (hit >= 0 && m_pres) {
            Slide* s = m_pres->slideAt(m_slideIndex);
            applyFormat(s->elements[hit], m_formatTemplate);
            m_selectedElem = hit;
            emit elementSelected(hit);
            emit presentationModified();
        }
        m_formatPainterMode = false;
        setCursor(Qt::ArrowCursor);
        update();
        return;
    }

    if (m_editingElem >= 0) finishTextEdit();

    // 1. Check if a resize handle of the selected element is hit
    int handle = hitHandle(e->position());
    if (handle >= 0) {
        Slide* s = m_pres->slideAt(m_slideIndex);
        const SlideElement& elem = s->elements[m_selectedElem];
        m_resizingHandle = handle;
        m_resizeOrigX = elem.x;   m_resizeOrigY = elem.y;
        m_resizeOrigW = elem.width; m_resizeOrigH = elem.height;
        m_dragStartSlide = widgetToSlide(e->position());
        return;
    }

    // 2. Hit-test elements for selection / move
    int hit = hitTest(e->position());
    m_selectedElem = hit;

    if (hit >= 0 && m_pres) {
        m_dragging       = true;
        m_dragStartSlide = widgetToSlide(e->position());
        Slide* s         = m_pres->slideAt(m_slideIndex);
        m_dragOrigin     = QPointF(s->elements[hit].x, s->elements[hit].y);
    }

    emit elementSelected(m_selectedElem);
    update();
}

void SlideEditor2D::mouseMoveEvent(QMouseEvent* e) {
    QPointF curSlide = widgetToSlide(e->position());

    // Resize mode
    if (m_resizingHandle >= 0) {
        Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
        if (s && m_selectedElem >= 0 && m_selectedElem < s->elements.size()) {
            bool constrain = (e->modifiers() & Qt::ControlModifier) != 0;
            applyResize(s->elements[m_selectedElem], m_resizingHandle,
                        curSlide,
                        m_resizeOrigX, m_resizeOrigY,
                        m_resizeOrigW, m_resizeOrigH, constrain);
            update();
            emit presentationModified();
        }
        return;
    }

    // Move mode
    if (m_dragging && m_selectedElem >= 0) {
        Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
        if (!s) return;
        QPointF delta = curSlide - m_dragStartSlide;
        SlideElement& elem = s->elements[m_selectedElem];
        elem.x = float(m_dragOrigin.x() + delta.x());
        elem.y = float(m_dragOrigin.y() + delta.y());
        applySnapAndGuides(elem);
        update();
        emit presentationModified();
        return;
    }

    // Cursor hints when hovering
    int handle = hitHandle(e->position());
    if (handle >= 0) {
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
    if (e->button() == Qt::LeftButton) {
        m_dragging       = false;
        m_resizingHandle = -1;
        m_snapGuides.clear();
        update();
    }
}

void SlideEditor2D::mouseDoubleClickEvent(QMouseEvent* e) {
    int hit = hitTest(e->position());
    if (hit < 0 || !m_pres) return;
    Slide* s = m_pres->slideAt(m_slideIndex);
    if (!s) return;
    if (s->elements[hit].type == SlideElement::Text)
        startTextEdit(hit);
}

void SlideEditor2D::keyPressEvent(QKeyEvent* e) {
    if (m_editingElem >= 0) { QWidget::keyPressEvent(e); return; }

    const bool ctrl = e->modifiers() & Qt::ControlModifier;

    if (ctrl && e->key() == Qt::Key_C) {
        copySelectedElement();
    } else if (ctrl && e->key() == Qt::Key_X) {
        copySelectedElement(); deleteSelectedElement();
    } else if (ctrl && e->key() == Qt::Key_V) {
        // Check for image in system clipboard first
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
        pasteElement();
    } else if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
        deleteSelectedElement();
    } else if (e->key() == Qt::Key_Escape) {
        m_selectedElem = -1;
        emit elementSelected(-1);
        update();
    } else {
        QWidget::keyPressEvent(e);
    }
}

void SlideEditor2D::copySelectedElement() {
    const Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return;
    s_clipboard    = s->elements[m_selectedElem];
    s_hasClipboard = true;
}

void SlideEditor2D::pasteElement() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || !s_hasClipboard) return;
    SlideElement newElem = s_clipboard;
    newElem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    newElem.x += 20; newElem.y += 20;
    s->elements.append(newElem);
    m_selectedElem = s->elements.size() - 1;
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
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
        static const QStringList exts = {"png","jpg","jpeg","bmp","gif","webp","svg"};
        for (const QUrl& url : md->urls()) {
            if (!url.isLocalFile()) continue;
            QString path = url.toLocalFile();
            if (exts.contains(QFileInfo(path).suffix().toLower())) {
                addImageFromPath(path, dropPos);
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
    QMenu menu(this);
    if (m_selectedElem >= 0) {
        menu.addAction("Kopieren  (Strg+C)", this, &SlideEditor2D::copySelectedElement);
        menu.addAction("Ausschneiden (Strg+X)", this, [this]() {
            copySelectedElement(); deleteSelectedElement();
        });
        menu.addSeparator();
        menu.addAction("Vordergrund",          this, &SlideEditor2D::bringToFront);
        menu.addAction("Eine Ebene nach oben", this, &SlideEditor2D::bringForward);
        menu.addAction("Eine Ebene nach unten",this, &SlideEditor2D::sendBackward);
        menu.addAction("Hintergrund",          this, &SlideEditor2D::sendToBack);
        menu.addSeparator();
        menu.addAction("Element löschen",      this, &SlideEditor2D::deleteSelectedElement);
        menu.addSeparator();
    }
    auto* pasteAct = menu.addAction("Einfügen (Strg+V)", this, &SlideEditor2D::pasteElement);
    pasteAct->setEnabled(s_hasClipboard);
    menu.exec(e->globalPos());
}

// ── Element operations ────────────────────────────────────────────────────────

void SlideEditor2D::addTextElement() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.type = SlideElement::Text; e.content = "Text eingeben";
    e.x = 200; e.y = 200; e.width = 700; e.height = 90; e.fontSize = 36;
    s->elements.append(e);
    m_selectedElem = s->elements.size() - 1;
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
    m_selectedElem = s->elements.size() - 1;
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
    m_selectedElem = s->elements.size() - 1;
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::addImageElement() {
    QString path = QFileDialog::getOpenFileName(nullptr, "Bild öffnen", {},
        "Bilder (*.png *.jpg *.jpeg *.bmp *.gif *.webp);;Alle Dateien (*)");
    if (path.isEmpty()) return;
    addImageFromPath(path);
}

void SlideEditor2D::deleteSelectedElement() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return;
    s->elements.removeAt(m_selectedElem);
    m_selectedElem = -1;
    update();
    emit presentationModified();
    emit elementSelected(-1);
}

// ── Layer / z-order ───────────────────────────────────────────────────────────

void SlideEditor2D::bringToFront() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size() - 1) return;
    s->elements.move(m_selectedElem, s->elements.size() - 1);
    m_selectedElem = s->elements.size() - 1;
    update(); emit presentationModified(); emit elementSelected(m_selectedElem);
}

void SlideEditor2D::bringForward() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size() - 1) return;
    s->elements.swapItemsAt(m_selectedElem, m_selectedElem + 1);
    m_selectedElem++;
    update(); emit presentationModified(); emit elementSelected(m_selectedElem);
}

void SlideEditor2D::sendBackward() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem <= 0) return;
    s->elements.swapItemsAt(m_selectedElem, m_selectedElem - 1);
    m_selectedElem--;
    update(); emit presentationModified(); emit elementSelected(m_selectedElem);
}

void SlideEditor2D::sendToBack() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem <= 0) return;
    s->elements.move(m_selectedElem, 0);
    m_selectedElem = 0;
    update(); emit presentationModified(); emit elementSelected(m_selectedElem);
}

// ── Inline text edit ──────────────────────────────────────────────────────────

void SlideEditor2D::startTextEdit(int idx) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || idx < 0 || idx >= s->elements.size()) return;
    finishTextEdit();
    m_editingElem = idx;
    SlideElement& e = s->elements[idx];
    QRectF wr = elemToWidget(e);
    m_textEdit->setGeometry(wr.toRect());
    m_textEdit->setPlainText(e.content);
    float sy = slideRect().height() / SLIDE_H_DEFAULT;
    m_textEdit->setFont(QFont(e.fontFamily, qMax(8, (int)(e.fontSize * sy))));
    m_textEdit->show();
    m_textEdit->setFocus();
    m_textEdit->selectAll();
}

void SlideEditor2D::finishTextEdit() {
    if (m_editingElem < 0) return;
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (s && m_editingElem < s->elements.size()) {
        s->elements[m_editingElem].content = m_textEdit->toPlainText();
        emit presentationModified();
    }
    m_editingElem = -1;
    m_textEdit->hide();
    update();
}
