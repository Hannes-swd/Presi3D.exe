#include "SlideEditor2D.h"
#include "ShapeUtils.h"
#include "rendering/ChartRenderer.h"
#include "rendering/LatexRenderer.h"
#include "dialogs/ChartEditorDialog.h"
#include "dialogs/InsertFormulaDialog.h"
#include "dialogs/InsertIFrameDialog.h"
#include "dialogs/InsertButtonDialog.h"
#include <QPainter>
#include <QtMath>
#include <QMouseEvent>
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
#include <QTextDocument>
#include <QTextCursor>
#include <QTextFormat>
#include <QFocusEvent>
#include <QTimer>
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
}

// ── Public API ────────────────────────────────────────────────────────────────

void SlideEditor2D::setSlide(Presentation* pres, int slideIndex) {
    finishTextEdit();
    exitTableEditMode();
    if (pres != m_pres) m_pixmapCache.clear();
    m_pres           = pres;
    m_slideIndex     = slideIndex;
    m_selectedElem   = -1;
    m_resizingHandle = -1;
    m_dragDivider    = {};
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
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return -1;
    const SlideElement& elem = s->elements[m_selectedElem];
    QRectF wr = elemToWidget(elem);
    float rot = elem.rotation;

    // Check rotation handle first: circle 30px above TC in local space, then rotated
    QPointF localRotHandle(wr.center().x(), wr.top() - 30);
    QPointF rotHandleW = (rot != 0.f) ? rotatePt(localRotHandle, wr.center(), rot)
                                       : localRotHandle;
    QRectF rhr(rotHandleW.x() - HANDLE_R, rotHandleW.y() - HANDLE_R,
               HANDLE_R * 2, HANDLE_R * 2);
    if (rhr.contains(wpos)) return 8;

    // Resize handles: un-rotate click into local space first
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
    for (int i = 0; i < slide->elements.size(); ++i) {
        bool showSel = (i == m_selectedElem) && (i != m_editingElem);
        drawElement(p, slide->elements[i], showSel);
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

    // Selection handles (drawn outside clip so they extend beyond slide edge)
    if (m_selectedElem >= 0 && m_selectedElem < slide->elements.size())
        drawHandles(p, elemToWidget(slide->elements[m_selectedElem]),
                    slide->elements[m_selectedElem].rotation);
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
        font.setUnderline(e.underline || !e.hyperlink.trimmed().isEmpty());
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
        p.setBrush(e.backgroundColor == Qt::transparent ? Qt::NoBrush : QBrush(e.backgroundColor));
        {
            QRectF sr2 = slideRect();
            float rx = e.cornerRadius * sr2.width()  / SLIDE_W_DEFAULT;
            float ry = e.cornerRadius * sr2.height() / SLIDE_H_DEFAULT;
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
            QFont font(e.fontFamily, qMax(6, int(e.fontSize * scaleY)));
            font.setBold(e.bold);
            font.setItalic(e.italic);
            p.save();
            p.setFont(font);
            p.setPen(e.color.isValid() ? e.color : Qt::white);
            p.setClipRect(wr, Qt::IntersectClip);
            p.drawText(wr, Qt::AlignCenter | Qt::TextWordWrap, e.shapeText);
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

    } else if (e.type == SlideElement::Table) {
        drawTableElement(p, e, selected);
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
        ChartRenderer::paint(p, wr, e.chartData);
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
        p.drawText(wr, Qt::AlignCenter | Qt::TextWordWrap,
                   e.content.isEmpty() ? "Button" : e.content);
        p.restore();

        if (selected) {
            p.save();
            p.setPen(QColor(37, 99, 235, 220));
            p.setFont(QFont("Arial", qMax(6, int(9 * scaleY))));
            p.drawText(QRectF(wr.x(), wr.bottom() - 16 * scaleY, wr.width(), 16 * scaleY),
                       Qt::AlignCenter, "Double-click: choose target slide");
            p.restore();
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

void SlideEditor2D::drawTableElement(QPainter& p, const SlideElement& e, bool selected) const {
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
                p.drawText(tr, int(Qt::TextWordWrap | Qt::AlignVCenter | align), cell.text);
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

    // Format painter mode
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

    // Text editing: click inside same element → reposition cursor; elsewhere → finish
    if (m_editingElem >= 0) {
        Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
        if (s && hitTest(e->position()) == m_editingElem) {
            if (!m_editingShapeText)
                m_cursorPos = textPositionAt(s->elements[m_editingElem], e->position());
            m_selAnchor     = -1;
            m_textSelecting = !m_editingShapeText;
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
        const SlideElement& elem = s->elements[m_selectedElem];
        m_resizingHandle = handle;
        m_resizeOrigX = elem.x;   m_resizeOrigY = elem.y;
        m_resizeOrigW = elem.width; m_resizeOrigH = elem.height;
        m_dragStartSlide = widgetToSlide(e->position());
        return;
    }

    // Hit-test elements for selection / move
    int hit = hitTest(e->position());
    if (hit != m_selectedElem && m_tableEditMode) {
        exitTableEditMode();
    }
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
    if (e->button() == Qt::LeftButton) {
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

void SlideEditor2D::mouseDoubleClickEvent(QMouseEvent* e) {
    int hit = hitTest(e->position());
    if (hit < 0 || !m_pres) return;
    Slide* s = m_pres->slideAt(m_slideIndex);
    if (!s) return;
    const SlideElement& elem = s->elements[hit];

    if (elem.type == SlideElement::Chart) {
        m_selectedElem = hit;
        openChartEditor();
        return;
    } else if (elem.type == SlideElement::Formula) {
        m_selectedElem = hit;
        openFormulaEditor();
        return;
    } else if (elem.type == SlideElement::IFrame) {
        m_selectedElem = hit;
        openIFrameEditor();
        return;
    } else if (elem.type == SlideElement::Button) {
        m_selectedElem = hit;
        openButtonEditor();
        return;
    } else if (elem.type == SlideElement::Text) {
        startTextEdit(hit, e->position());
    } else if (elem.type == SlideElement::Shape) {
        m_selectedElem = hit;
        startTextEdit(hit, e->position());
    } else if (elem.type == SlideElement::Table) {
        m_selectedElem  = hit;
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
        emit tableCellSelected(m_selTableRow, m_selTableCol);
        update();
    }
}

void SlideEditor2D::keyPressEvent(QKeyEvent* e) {
    if (m_editingElem >= 0) { handleTextEditKey(e); return; }
    if (m_tableEditMode)    { handleTableKey(e);     return; }

    const bool ctrl = e->modifiers() & Qt::ControlModifier;

    if (ctrl && e->key() == Qt::Key_C) {
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
    if (m_selectedElem >= 0) {
        menu.addAction("Copy  (Ctrl+C)", this, &SlideEditor2D::copySelectedElement);
        menu.addAction("Cut (Ctrl+X)", this, [this]() {
            copySelectedElement(); deleteSelectedElement();
        });
        menu.addSeparator();
        menu.addAction("Bring to Front",       this, &SlideEditor2D::bringToFront);
        menu.addAction("Move One Layer Up",    this, &SlideEditor2D::bringForward);
        menu.addAction("Move One Layer Down",  this, &SlideEditor2D::sendBackward);
        menu.addAction("Send to Back",         this, &SlideEditor2D::sendToBack);
        menu.addSeparator();
        menu.addAction("Delete Element",       this, &SlideEditor2D::deleteSelectedElement);
        menu.addSeparator();
    }
    auto* pasteAct = menu.addAction("Paste (Ctrl+V)", this, &SlideEditor2D::pasteElement);
    pasteAct->setEnabled(s_hasClipboard);
    menu.exec(e->globalPos());
}

// ── Element operations ────────────────────────────────────────────────────────

void SlideEditor2D::addTextElement() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.type = SlideElement::Text; e.content = "Enter text";
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

void SlideEditor2D::addTableElement(int rows, int cols) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.initTable(rows, cols);
    e.x = 260.f; e.y = 340.f;
    e.width = 1400.f; e.height = float(rows) * 60.f;
    s->elements.append(e);
    m_selectedElem = s->elements.size() - 1;
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
    m_selectedElem = s->elements.size() - 1;
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
    m_selectedElem = s->elements.size() - 1;
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
    m_selectedElem = s->elements.size() - 1;
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

static QVector<QPair<QString, QString>> slideListForButtonTarget(const Presentation* pres) {
    QVector<QPair<QString, QString>> slides;
    if (!pres) return slides;
    for (const Slide& s : pres->slides)
        slides.append({s.id, s.name.isEmpty() ? QString("Slide %1").arg(slides.size() + 1) : s.name});
    return slides;
}

void SlideEditor2D::addButtonElement(const QString& label, const QString& targetSlideId) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s) return;
    SlideElement e;
    e.type            = SlideElement::Button;
    e.content         = label.isEmpty() ? "Next" : label;
    e.targetSlideId   = targetSlideId;
    e.x = 300.f; e.y = 300.f; e.width = 240.f; e.height = 70.f;
    e.fontSize        = 24;
    e.color           = Qt::white;
    e.backgroundColor = QColor(37, 99, 235);
    e.cornerRadius    = 8.f;
    e.bold            = true;
    s->elements.append(e);
    m_selectedElem = s->elements.size() - 1;
    update();
    emit presentationModified();
    emit elementSelected(m_selectedElem);
}

void SlideEditor2D::openButtonEditor() {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_selectedElem < 0 || m_selectedElem >= s->elements.size()) return;
    SlideElement& e = s->elements[m_selectedElem];
    if (e.type != SlideElement::Button) return;

    InsertButtonDialog dlg(this, slideListForButtonTarget(m_pres), e.content, e.targetSlideId);
    if (dlg.exec() == QDialog::Accepted) {
        e.content       = dlg.label().isEmpty() ? "Next" : dlg.label();
        e.targetSlideId = dlg.targetSlideId();
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

// ── Inline text edit (WYSIWYG — direct canvas cursor, no overlay widget) ─────

// Build a QTextLayout for a text element at widget scale and do line layout.
// The layout origin is at (0,0); add elemToWidget(e).topLeft() to get widget coords.
static void buildLayout(QTextLayout& layout, const SlideElement& e,
                        const QFont& font, float width,
                        const QString& alignOverride = {}) {
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

void SlideEditor2D::startTextEdit(int idx, QPointF clickPos) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || idx < 0 || idx >= s->elements.size()) return;
    SlideElement& elem = s->elements[idx];
    if (elem.type != SlideElement::Text && elem.type != SlideElement::Shape) return;
    finishTextEdit();
    m_editingElem      = idx;
    m_editingShapeText = (elem.type == SlideElement::Shape);

    const QString& text = m_editingShapeText ? elem.shapeText : elem.content;
    if (clickPos.x() >= 0 && !m_editingShapeText)
        m_cursorPos = textPositionAt(elem, clickPos);
    else
        m_cursorPos = text.length();

    m_selAnchor     = -1;
    m_cursorVisible = true;
    m_cursorBlink->start();
    setFocus();
    update();
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
}

void SlideEditor2D::handleTextEditKey(QKeyEvent* ke) {
    Slide* s = m_pres ? m_pres->slideAt(m_slideIndex) : nullptr;
    if (!s || m_editingElem < 0 || m_editingElem >= s->elements.size()) return;
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
        else if (m_cursorPos > 0) { text.remove(m_cursorPos - 1, 1); m_cursorPos--; }
        emit presentationModified(); update(); return;
    }
    if (ke->key() == Qt::Key_Delete) {
        if (m_selAnchor >= 0 && m_selAnchor != m_cursorPos) deleteSelection();
        else if (m_cursorPos < text.length()) text.remove(m_cursorPos, 1);
        emit presentationModified(); update(); return;
    }

    // ── Enter ─────────────────────────────────────────────────────────────
    if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
        if (m_selAnchor >= 0) deleteSelection();
        text.insert(m_cursorPos, '\n'); m_cursorPos++;
        emit presentationModified(); update(); return;
    }

    // ── Printable character (incl. AltGr combos) ─────────────────────────
    QString ch = ke->text();
    if (!ch.isEmpty() && ch[0].isPrint()) {
        if (m_selAnchor >= 0) deleteSelection();
        text.insert(m_cursorPos, ch);
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
    float sy = slideRect().height() / SLIDE_H_DEFAULT;
    QFont font = elemFont(e, sy);
    QTextLayout layout(e.content, font);
    buildLayout(layout, e, font, float(wr.width()));

    float vOff = textVOff(layout, float(wr.height()), e.verticalAlignment);

    // Adjust click position by vertical alignment offset
    QPointF local = widgetPos - wr.topLeft();
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
