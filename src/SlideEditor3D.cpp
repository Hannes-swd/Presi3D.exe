#include "SlideEditor3D.h"
#include "ShapeUtils.h"
#include "models/WorldObjectMesh.h"
#include "rendering/ChartRenderer.h"
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPixmap>
#include <QFileInfo>
#include <QImage>
#include <QtMath>
#include <cmath>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextFormat>
#include <QIcon>
#include <QMessageBox>
#include <utility>

// ── GLSL shaders ──────────────────────────────────────────────────────────────

static const char* VERT_SRC = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uMVP;
out vec2 vUV;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vUV = aUV;
}
)glsl";

static const char* FRAG_SRC = R"glsl(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform vec4      uColor;
uniform bool      uUseTexture;
uniform sampler2D uTex;
void main() {
    if (uUseTexture)
        FragColor = texture(uTex, vUV);
    else
        FragColor = uColor;
}
)glsl";

// Lit shader for WorldObject meshes: simple directional + ambient lighting.
// Separate from VERT_SRC/FRAG_SRC above so the existing flat-quad slide path
// stays completely untouched.
static const char* MESH_VERT_SRC = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vNormal;
out vec2 vUV;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vNormal = mat3(uModel) * aNormal;
    vUV = aUV;
}
)glsl";

static const char* MESH_FRAG_SRC = R"glsl(
#version 330 core
in vec3 vNormal;
in vec2 vUV;
out vec4 FragColor;
uniform vec4      uColor;
uniform bool      uUseTexture;
uniform sampler2D uTex;
uniform vec3      uLightDir;
uniform float     uAmbient;
uniform float     uOpacity;
void main() {
    vec4 base = uUseTexture ? texture(uTex, vUV) : uColor;
    vec3 n = normalize(vNormal);
    float diff = max(dot(n, normalize(uLightDir)), 0.0);
    float lightAmt = uAmbient + (1.0 - uAmbient) * diff;
    // glTF materials default to alphaMode=OPAQUE, meaning the alpha channel
    // of baseColor/texture must be ignored — only the WorldObject-level
    // uOpacity fade should affect transparency.
    FragColor = vec4(base.rgb * lightAmt, uOpacity);
}
)glsl";

// ── Constants ─────────────────────────────────────────────────────────────────

static constexpr float GIZMO_HIT_PX = 14.f;  // hit threshold in screen pixels

static const QVector3D AXIS_VEC[3]   = {{1,0,0},{0,1,0},{0,0,1}};
static const QVector4D AXIS_COLOR[3] = {
    {1.f, 0.25f, 0.25f, 1.f},   // X = red
    {0.2f, 1.f,  0.2f,  1.f},   // Y = green
    {0.35f,0.6f, 1.f,   1.f},   // Z = blue
};
static const QVector4D COLOR_HIGHLIGHT = {1.f, 1.f, 0.f, 1.f}; // yellow = active

// ── Geometry helpers ──────────────────────────────────────────────────────────

static void addCone(QVector<QVector3D>& pts,
                    const QVector3D& tip, const QVector3D& dir,
                    float radius, float height) {
    QVector3D n  = dir.normalized();
    QVector3D base = tip - n * height;
    QVector3D up = qAbs(QVector3D::dotProduct(n, {0,1,0})) < 0.9f
                   ? QVector3D(0,1,0) : QVector3D(1,0,0);
    QVector3D s1 = QVector3D::crossProduct(n, up).normalized() * radius;
    QVector3D s2 = QVector3D::crossProduct(n, s1).normalized() * radius;
    const int N = 8;
    for (int i = 0; i < N; ++i) {
        float a0 = float(2 * M_PI * i / N);
        float a1 = float(2 * M_PI * (i+1) / N);
        QVector3D p0 = base + s1 * cosf(a0) + s2 * sinf(a0);
        QVector3D p1 = base + s1 * cosf(a1) + s2 * sinf(a1);
        pts.append(tip); pts.append(p0);  // cone edge
        pts.append(p0);  pts.append(p1);  // base ring
    }
}

static QVector3D ringPt(const QVector3D& c, int axis, float r, float a) {
    switch (axis) {
        case 0: return c + QVector3D(0,        r*cosf(a), r*sinf(a));
        case 1: return c + QVector3D(r*cosf(a), 0,        r*sinf(a));
        default:return c + QVector3D(r*cosf(a), r*sinf(a), 0       );
    }
}

static float getRot(const GizmoTarget& t, int ax) {
    return ax == 0 ? *t.rx : ax == 1 ? *t.ry : *t.rz;
}
static void setRot(const GizmoTarget& t, int ax, float v) {
    if (ax == 0) *t.rx = v; else if (ax == 1) *t.ry = v; else *t.rz = v;
}

// ── Ctor / Dtor ───────────────────────────────────────────────────────────────

SlideEditor3D::SlideEditor3D(QWidget* parent)
    : QOpenGLWidget(parent), QOpenGLFunctions()
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

SlideEditor3D::~SlideEditor3D() {
    makeCurrent();
    m_quadVAO.destroy();
    m_quadVBO.destroy();
    m_quadEBO.destroy();
    qDeleteAll(m_textures);
    m_textures.clear();
    qDeleteAll(m_meshCache);
    m_meshCache.clear();
    delete m_prog;
    delete m_meshProg;
    doneCurrent();
}

void SlideEditor3D::setPresentation(Presentation* pres, int sel) {
    m_pres = pres;
    m_selectedSlide = sel;
    m_selKind = (sel >= 0) ? SelectionKind::Slide : SelectionKind::None;
    m_selectedWorldObj = -1;
    // Mark all slides dirty so textures rebuild on next paint
    if (pres)
        for (const auto& s : pres->slides) m_dirty.insert(s.id);
    update();
}

void SlideEditor3D::addWorldObject(const QString& path) {
    if (!m_pres) return;

    QString err;
    makeCurrent();
    WorldObjectMesh* mesh = WorldObjectMesh::load(path, err);
    doneCurrent();
    if (!mesh) {
        QMessageBox::warning(this, tr("Could Not Load Model"),
            tr("This file could not be loaded as a glTF/GLB model:\n%1").arg(err));
        return;
    }
    m_meshCache[path] = mesh;
    m_meshLoadFailed.remove(path);

    WorldObject w;
    w.modelPath = path;
    w.posX = m_target.x(); w.posY = m_target.y(); w.posZ = m_target.z();
    m_pres->worldObjects.append(w);

    m_selKind          = SelectionKind::WorldObject;
    m_selectedWorldObj = m_pres->worldObjects.size() - 1;
    m_selectedSlide    = -1;

    emit worldObjectSelected(m_selectedWorldObj);
    emit presentationModified();
    update();
}

GizmoTarget SlideEditor3D::currentTarget() const {
    if (!m_pres) return {};
    if (m_selKind == SelectionKind::Slide &&
        m_selectedSlide >= 0 && m_selectedSlide < m_pres->slides.size())
        return targetOf(m_pres->slides[m_selectedSlide]);
    if (m_selKind == SelectionKind::WorldObject &&
        m_selectedWorldObj >= 0 && m_selectedWorldObj < m_pres->worldObjects.size())
        return targetOf(m_pres->worldObjects[m_selectedWorldObj]);
    return {};
}

// ── GL init ───────────────────────────────────────────────────────────────────

void SlideEditor3D::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.13f, 0.13f, 0.15f, 1.f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!initShaders()) { qWarning("SlideEditor3D: shader error"); return; }
    if (!initMeshShader()) { qWarning("SlideEditor3D: mesh shader error"); }
    initGeometry();
    m_initialized = true;
}

bool SlideEditor3D::initShaders() {
    m_prog = new QOpenGLShaderProgram(this);
    if (!m_prog->addShaderFromSourceCode(QOpenGLShader::Vertex,   VERT_SRC)) {
        qWarning() << "VS:" << m_prog->log(); return false; }
    if (!m_prog->addShaderFromSourceCode(QOpenGLShader::Fragment, FRAG_SRC)) {
        qWarning() << "FS:" << m_prog->log(); return false; }
    return m_prog->link();
}

bool SlideEditor3D::initMeshShader() {
    m_meshProg = new QOpenGLShaderProgram(this);
    if (!m_meshProg->addShaderFromSourceCode(QOpenGLShader::Vertex,   MESH_VERT_SRC)) {
        qWarning() << "Mesh VS:" << m_meshProg->log(); return false; }
    if (!m_meshProg->addShaderFromSourceCode(QOpenGLShader::Fragment, MESH_FRAG_SRC)) {
        qWarning() << "Mesh FS:" << m_meshProg->log(); return false; }
    return m_meshProg->link();
}

void SlideEditor3D::initGeometry() {
    static constexpr float HW = 960.f, HH = 540.f;
    float verts[] = {
        -HW,-HH,0,  0,0,
         HW,-HH,0,  1,0,
         HW, HH,0,  1,1,
        -HW, HH,0,  0,1,
    };
    quint32 idx[] = {0,1,2, 2,3,0};

    m_quadVAO.create(); m_quadVAO.bind();
    m_quadVBO.create();
    m_quadVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_quadVBO.bind();
    m_quadVBO.allocate(verts, sizeof(verts));
    m_quadEBO.create();
    m_quadEBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_quadEBO.bind();
    m_quadEBO.allocate(idx, sizeof(idx));

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    m_quadVAO.release();
    m_quadVBO.release();
}

void SlideEditor3D::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

// ── Camera ────────────────────────────────────────────────────────────────────

QVector3D SlideEditor3D::camPos() const {
    float az = qDegreesToRadians(m_azimuth);
    float el = qDegreesToRadians(m_elevation);
    return m_target + QVector3D(m_distance * qCos(el) * qSin(az),
                                 m_distance * qSin(el),
                                 m_distance * qCos(el) * qCos(az));
}

QMatrix4x4 SlideEditor3D::viewMatrix() const {
    QMatrix4x4 v; v.lookAt(camPos(), m_target, {0,1,0}); return v;
}

QMatrix4x4 SlideEditor3D::projMatrix() const {
    QMatrix4x4 p;
    p.perspective(45.f, width() > 0 ? float(width())/height() : 1.f, 10.f, 200000.f);
    return p;
}

QMatrix4x4 SlideEditor3D::slideModel(const Slide& s) const {
    QMatrix4x4 m;
    m.translate(s.posX, s.posY, s.posZ);
    m.rotate(s.rotZ, 0, 0, 1);
    m.rotate(s.rotY, 0, 1, 0);
    m.rotate(s.rotX, 1, 0, 0);
    // scale is NOT applied to the geometry — it only affects the viewport rectangle
    float effW = (s.slideWidth  > 0) ? s.slideWidth  : (m_pres ? m_pres->slideWidth  : 1920.f);
    float effH = (s.slideHeight > 0) ? s.slideHeight : (m_pres ? m_pres->slideHeight : 1080.f);
    float sw = effW / 1920.f;
    float sh = effH / 1080.f;
    if (sw != 1.f || sh != 1.f) m.scale(sw, sh, 1.f);
    return m;
}

QMatrix4x4 SlideEditor3D::worldObjectModel(const WorldObject& w, float meshNormScale) const {
    QMatrix4x4 m;
    m.translate(w.posX, w.posY, w.posZ);
    m.rotate(w.rotZ, 0, 0, 1);
    m.rotate(w.rotY, 0, 1, 0);
    m.rotate(w.rotX, 1, 0, 0);
    // Unlike slideModel(), scale IS applied to the geometry here — a
    // WorldObject has no separate "viewport" concept, only a real mesh.
    m.scale(w.scale * meshNormScale);
    return m;
}

// ── Ray helpers ───────────────────────────────────────────────────────────────

void SlideEditor3D::getRay(const QPoint& pt, QVector3D& ro, QVector3D& rd) const {
    float ndcX =  (2.f * pt.x() / width())  - 1.f;
    float ndcY = -(2.f * pt.y() / height()) + 1.f;
    bool ok;
    QMatrix4x4 inv = (projMatrix() * viewMatrix()).inverted(&ok);
    if (!ok) { ro = camPos(); rd = {0,0,-1}; return; }
    QVector3D n3 = (inv * QVector4D(ndcX, ndcY, -1.f, 1.f)).toVector3DAffine();
    QVector3D f3 = (inv * QVector4D(ndcX, ndcY,  1.f, 1.f)).toVector3DAffine();
    ro = n3;
    rd = (f3 - n3).normalized();
}

bool SlideEditor3D::rayPlane(const QVector3D& ro, const QVector3D& rd,
                              const QVector3D& pn, const QVector3D& pp,
                              QVector3D& hit) const {
    float denom = QVector3D::dotProduct(pn, rd);
    if (qAbs(denom) < 1e-6f) return false;
    float t = QVector3D::dotProduct(pn, pp - ro) / denom;
    if (t < 0.f) return false;
    hit = ro + t * rd;
    return true;
}

// ── Screen helpers ────────────────────────────────────────────────────────────

QPointF SlideEditor3D::project(const QVector3D& p) const {
    QVector4D clip = (projMatrix() * viewMatrix()) * QVector4D(p, 1.f);
    if (clip.w() <= 0.f) return {-1e6, -1e6};
    return {(clip.x()/clip.w() + 1.f) * 0.5 * width(),
            (1.f - clip.y()/clip.w()) * 0.5 * height()};
}

float SlideEditor3D::distSeg2D(const QPointF& p,
                                const QPointF& a, const QPointF& b) const {
    QPointF ab = b - a, ap = p - a;
    float len2 = float(ab.x()*ab.x() + ab.y()*ab.y());
    if (len2 < 0.0001f) {
        float dx = float(p.x()-a.x()), dy = float(p.y()-a.y());
        return sqrtf(dx*dx + dy*dy);
    }
    float t = qBound(0.f, float(ap.x()*ab.x() + ap.y()*ab.y()) / len2, 1.f);
    QPointF pr = a + t * ab;
    float dx = float(p.x()-pr.x()), dy = float(p.y()-pr.y());
    return sqrtf(dx*dx + dy*dy);
}

// ── Hit testing ───────────────────────────────────────────────────────────────

int SlideEditor3D::hitMoveAxis(const QPoint& pt, const GizmoTarget& t) const {
    if (!t.valid()) return -1;
    float sz = gizmoSize();
    QVector3D center = t.pos();
    QPointF mp(pt), sc = project(center);
    for (int ax = 0; ax < 3; ++ax) {
        QPointF tip = project(center + AXIS_VEC[ax] * sz);
        if (distSeg2D(mp, sc, tip) < GIZMO_HIT_PX) return ax;
    }
    return -1;
}

int SlideEditor3D::hitRotateAxis(const QPoint& pt, const GizmoTarget& t) const {
    if (!t.valid()) return -1;
    float sz = gizmoSize();
    QVector3D center = t.pos();
    QPointF mp(pt);
    const int N = 48;
    for (int ax = 0; ax < 3; ++ax) {
        for (int i = 0; i < N; ++i) {
            float a0 = float(2*M_PI*i/N), a1 = float(2*M_PI*(i+1)/N);
            if (distSeg2D(mp, project(ringPt(center,ax,sz,a0)),
                              project(ringPt(center,ax,sz,a1))) < GIZMO_HIT_PX)
                return ax;
        }
    }
    return -1;
}

int SlideEditor3D::pickSlide(const QPoint& pt, float* outDist) const {
    if (!m_pres) return -1;
    QVector3D ro, rd;
    getRay(pt, ro, rd);
    static constexpr float HW = 960.f, HH = 540.f;
    int best = -1; float bestDist = 1e18f;
    for (int i = 0; i < m_pres->slides.size(); ++i) {
        const Slide& sl = m_pres->slides[i];
        bool ok;
        QMatrix4x4 mInv = slideModel(sl).inverted(&ok);
        if (!ok) continue;
        QVector3D lo = mInv.map(ro), ld = mInv.mapVector(rd).normalized();
        if (qAbs(ld.z()) < 1e-6f) continue;
        float t = -lo.z() / ld.z();
        if (t < 0) continue;
        QVector3D h = lo + t * ld;
        if (h.x() >= -HW && h.x() <= HW && h.y() >= -HH && h.y() <= HH) {
            float dist = (slideModel(sl).map(h) - camPos()).length();
            if (dist < bestDist) { bestDist = dist; best = i; }
        }
    }
    if (outDist) *outDist = bestDist;
    return best;
}

// Standard slab-based ray/AABB intersection. Returns false if the ray misses
// the box entirely; tmin/tmax are the (possibly negative) entry/exit
// distances along the ray otherwise.
static bool rayAabb(const QVector3D& ro, const QVector3D& rd,
                     const QVector3D& bmin, const QVector3D& bmax,
                     float& tmin, float& tmax) {
    tmin = -1e18f; tmax = 1e18f;
    for (int a = 0; a < 3; ++a) {
        float o = (a==0)?ro.x():(a==1)?ro.y():ro.z();
        float d = (a==0)?rd.x():(a==1)?rd.y():rd.z();
        float lo = (a==0)?bmin.x():(a==1)?bmin.y():bmin.z();
        float hi = (a==0)?bmax.x():(a==1)?bmax.y():bmax.z();
        if (qAbs(d) < 1e-9f) {
            if (o < lo || o > hi) return false;
            continue;
        }
        float t0 = (lo - o) / d, t1 = (hi - o) / d;
        if (t0 > t1) std::swap(t0, t1);
        tmin = qMax(tmin, t0);
        tmax = qMin(tmax, t1);
        if (tmin > tmax) return false;
    }
    return true;
}

int SlideEditor3D::pickWorldObject(const QPoint& pt, float* outDist) const {
    if (!m_pres) return -1;
    QVector3D ro, rd;
    getRay(pt, ro, rd);
    int best = -1; float bestDist = 1e18f;
    for (int i = 0; i < m_pres->worldObjects.size(); ++i) {
        const WorldObject& w = m_pres->worldObjects[i];
        WorldObjectMesh* mesh = m_meshCache.value(w.modelPath, nullptr);
        if (!mesh) continue;
        QMatrix4x4 model = worldObjectModel(w, mesh->normalizationScale);
        bool ok;
        QMatrix4x4 mInv = model.inverted(&ok);
        if (!ok) continue;
        QVector3D lo = mInv.map(ro), ld = mInv.mapVector(rd).normalized();
        float tmin, tmax;
        if (rayAabb(lo, ld, mesh->bboxMin, mesh->bboxMax, tmin, tmax) && tmax >= 0.f) {
            float tHit = qMax(tmin, 0.f);
            float dist = (model.map(lo + tHit * ld) - camPos()).length();
            if (dist < bestDist) { bestDist = dist; best = i; }
        }
    }
    if (outDist) *outDist = bestDist;
    return best;
}

// ── Rendering ─────────────────────────────────────────────────────────────────

WorldObjectMesh* SlideEditor3D::meshFor(const QString& modelPath) {
    if (modelPath.isEmpty()) return nullptr;
    auto it = m_meshCache.find(modelPath);
    if (it != m_meshCache.end()) return it.value();
    if (m_meshLoadFailed.contains(modelPath)) return nullptr;

    QString err;
    WorldObjectMesh* mesh = WorldObjectMesh::load(modelPath, err);
    if (!mesh) {
        qWarning() << "SlideEditor3D: failed to load world object model" << modelPath << ":" << err;
        m_meshLoadFailed.insert(modelPath);
        return nullptr;
    }
    m_meshCache[modelPath] = mesh;
    return mesh;
}

// ── Texture building ──────────────────────────────────────────────────────────

QOpenGLTexture* SlideEditor3D::buildSlideTexture(const Slide& slide) {
    const int W = 960, H = 540;
    QImage img(W, H, QImage::Format_RGBA8888);
    bool transparent = !slide.backgroundColor.isValid()
                       || slide.backgroundColor == Qt::transparent
                       || slide.backgroundColor.alpha() == 0;
    img.fill(transparent ? Qt::transparent : slide.backgroundColor);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const float sx = W / 1920.f;
    const float sy = H / 1080.f;

    for (const auto& elem : slide.elements) {
        QRectF r(elem.x * sx, elem.y * sy, elem.width * sx, elem.height * sy);

        if (elem.type == SlideElement::Text) {
            if (elem.backgroundColor.isValid() && elem.backgroundColor != Qt::transparent)
                p.fillRect(r, elem.backgroundColor);

            if (r.width() < 1.0 || r.height() < 1.0) continue;

            QString displayText = elem.content;
            if (elem.listStyle != SlideElement::NoList) {
                QStringList lines = elem.content.split('\n');
                QStringList fmt;
                for (int ln = 0; ln < lines.size(); ++ln)
                    fmt << (elem.listStyle == SlideElement::Bullets
                            ? QString("• ") + lines[ln]
                            : QString::number(ln + 1) + ". " + lines[ln]);
                displayText = fmt.join('\n');
            }

            QFont font(elem.fontFamily, qMax(1, int(elem.fontSize * sy)));
            font.setBold(elem.bold);
            font.setItalic(elem.italic);
            font.setUnderline(elem.underline);
            font.setStrikeOut(elem.strikethrough);

            Qt::Alignment align = Qt::AlignLeft;
            if (elem.textAlignment == "center") align = Qt::AlignHCenter;
            else if (elem.textAlignment == "right") align = Qt::AlignRight;

            p.save();
            p.setFont(font);
            p.setPen(elem.color.isValid() ? elem.color : Qt::black);
            p.setClipRect(r, Qt::IntersectClip);
            p.drawText(r.toRect(),
                       int(Qt::TextWordWrap | Qt::AlignVCenter | align),
                       displayText);
            p.restore();

        } else if (elem.type == SlideElement::Shape) {
            QBrush brush = (elem.backgroundColor.isValid() && elem.backgroundColor != Qt::transparent)
                ? QBrush(elem.backgroundColor) : Qt::NoBrush;
            p.setPen(elem.borderWidth > 0
                     ? QPen(elem.borderColor.isValid() ? elem.borderColor : Qt::darkGray,
                            elem.borderWidth * sx)
                     : Qt::NoPen);
            p.setBrush(brush);
            float rx = elem.cornerRadius * sx, ry = elem.cornerRadius * sy;
            if (elem.content == "line") {
                p.drawLine(r.topLeft(), r.bottomRight());
            } else if (elem.content == "rect") {
                if (rx > 0 || ry > 0) p.drawRoundedRect(r, rx, ry);
                else                   p.drawRect(r);
            } else {
                p.drawPath(ShapeUtils::shapeToPath(elem.content, r));
            }

        } else if (elem.type == SlideElement::Image) {
            QPixmap px(elem.content);
            if (!px.isNull())
                p.drawPixmap(r.toRect(), px.scaled(r.toRect().size(), Qt::KeepAspectRatio,
                                                    Qt::SmoothTransformation));
            else {
                p.fillRect(r, QColor(150,150,180,80));
                p.setPen(Qt::gray);
                p.drawRect(r);
                p.drawText(r, int(Qt::AlignCenter), QFileInfo(elem.content).fileName());
            }

        } else if (elem.type == SlideElement::IFrame) {
            p.fillRect(r, QColor(235, 240, 250));
            p.setPen(QPen(QColor(150, 160, 190), qMax(1.f, sx)));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r);
            p.save();
            p.setPen(QColor(90, 100, 140));
            p.setFont(QFont("Arial", qMax(6, int(20 * sy))));
            p.drawText(r, int(Qt::AlignCenter | Qt::TextWordWrap),
                       elem.content.isEmpty() ? QStringLiteral("\U0001F310 iFrame") : elem.content);
            p.restore();

        } else if (elem.type == SlideElement::Table) {
            p.fillRect(r, elem.tableDefaultBg);
            int fontPx = qMax(1, int(elem.tableFontSize * sy));
            float rowY = float(r.y());
            for (int row = 0; row < elem.tableRows && row < elem.tableCells.size(); ++row) {
                float rowH = elem.tableRowFracs[row] * float(r.height());
                float colX = float(r.x());
                for (int col = 0; col < elem.tableCols && col < elem.tableCells[row].size(); ++col) {
                    float colW = elem.tableColFracs[col] * float(r.width());
                    const TableCell& cell = elem.tableCells[row][col];
                    if (cell.merged) { colX += colW; continue; }
                    int cs = qMax(1, qMin(cell.colspan, elem.tableCols - col));
                    int rs = qMax(1, qMin(cell.rowspan, elem.tableRows - row));
                    float cw = 0, rh = 0;
                    for (int c2 = col; c2 < col + cs && c2 < elem.tableCols; ++c2)
                        cw += elem.tableColFracs[c2] * float(r.width());
                    for (int r2 = row; r2 < row + rs && r2 < elem.tableRows; ++r2)
                        rh += elem.tableRowFracs[r2] * float(r.height());
                    QRectF cr(colX, rowY, cw, rh);
                    bool isHdr = row == 0 && elem.tableHasHeader;
                    QColor bg = isHdr ? elem.tableHeaderBg
                              : (cell.bgColor.isValid() ? cell.bgColor : elem.tableDefaultBg);
                    p.fillRect(cr, bg);
                    if (elem.tableBorderWidth > 0) {
                        p.setPen(QPen(elem.tableBorderColor, elem.tableBorderWidth * sx));
                        p.setBrush(Qt::NoBrush);
                        p.drawRect(cr);
                    }
                    QColor tc = isHdr ? elem.tableHeaderText
                               : (cell.textColor.isValid() ? cell.textColor : elem.tableDefaultText);
                    QFont f(elem.tableFontFamily, fontPx);
                    f.setBold(cell.bold || isHdr);
                    f.setItalic(cell.italic);
                    p.setFont(f);
                    p.setPen(tc);
                    p.save();
                    p.setClipRect(cr, Qt::IntersectClip);
                    Qt::Alignment align = Qt::AlignLeft;
                    if (cell.textAlign == "center") align = Qt::AlignHCenter;
                    else if (cell.textAlign == "right") align = Qt::AlignRight;
                    p.drawText(cr.adjusted(4*sx,2*sy,-4*sx,-2*sy),
                               int(Qt::AlignVCenter | align | Qt::TextWordWrap), cell.text);
                    p.restore();
                    colX += colW;
                }
                rowY += rowH;
            }
            if (elem.tableBorderWidth > 0) {
                p.setPen(QPen(elem.tableBorderColor, elem.tableBorderWidth * sx + 0.5f));
                p.setBrush(Qt::NoBrush);
                p.drawRect(r);
            }
        } else if (elem.type == SlideElement::Chart) {
            p.fillRect(r, Qt::white);
            ChartRenderer::paint(p, r, elem.chartData);
        } else if (elem.type == SlideElement::Button) {
            p.setPen(elem.borderWidth > 0
                     ? QPen(elem.borderColor.isValid() ? elem.borderColor : Qt::darkGray,
                            elem.borderWidth * sx)
                     : Qt::NoPen);
            p.setBrush(elem.backgroundColor.isValid() && elem.backgroundColor != Qt::transparent
                       ? QBrush(elem.backgroundColor) : QBrush(QColor(37, 99, 235)));
            float rx = elem.cornerRadius * sx, ry = elem.cornerRadius * sy;
            if (rx > 0 || ry > 0) p.drawRoundedRect(r, rx, ry);
            else                   p.drawRect(r);
            QFont bf(elem.fontFamily, qMax(6, int(elem.fontSize * sy)));
            bf.setBold(elem.bold);
            p.setFont(bf);
            p.setPen(elem.color.isValid() ? elem.color : Qt::white);
            p.drawText(r, int(Qt::AlignCenter | Qt::TextWordWrap),
                       elem.content.isEmpty() ? "Button" : elem.content);
        }
    }
    p.end();

    // Mirror vertically: QImage (0,0)=top-left, OpenGL texture (0,0)=bottom-left
    auto* tex = new QOpenGLTexture(img.mirrored());
    tex->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
    tex->setMagnificationFilter(QOpenGLTexture::Linear);
    tex->generateMipMaps();
    return tex;
}

// ── Line drawing ──────────────────────────────────────────────────────────────

void SlideEditor3D::drawLines(const QVector<QVector3D>& pts,
                               const QVector4D& color, float lw) {
    if (pts.isEmpty() || !m_prog) return;
    m_prog->bind();
    m_prog->setUniformValue("uMVP",        projMatrix() * viewMatrix());
    m_prog->setUniformValue("uColor",      color);
    m_prog->setUniformValue("uUseTexture", false);

    QOpenGLBuffer vbo(QOpenGLBuffer::VertexBuffer);
    vbo.create(); vbo.bind();
    vbo.allocate(pts.constData(), pts.size() * (int)sizeof(QVector3D));

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(QVector3D), nullptr);
    glEnableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    glLineWidth(lw);
    glDrawArrays(GL_LINES, 0, pts.size());

    glEnableVertexAttribArray(1);
    vbo.release(); vbo.destroy();
    m_prog->release();
}

void SlideEditor3D::renderSlide(const Slide& sl, bool selected) {
    // Rebuild texture if dirty
    if (m_dirty.contains(sl.id)) {
        delete m_textures.value(sl.id, nullptr);
        m_textures[sl.id] = buildSlideTexture(sl);
        m_dirty.remove(sl.id);
    }

    QMatrix4x4 mvp = projMatrix() * viewMatrix() * slideModel(sl);
    m_prog->bind();
    m_prog->setUniformValue("uMVP", mvp);
    m_quadVAO.bind();

    QOpenGLTexture* tex = m_textures.value(sl.id, nullptr);
    if (tex && tex->isCreated()) {
        tex->bind(0);
        m_prog->setUniformValue("uUseTexture", true);
        m_prog->setUniformValue("uTex", 0);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        tex->release();
    } else {
        // Fallback: flat color (transparent slides = invisible quad until texture builds)
        m_prog->setUniformValue("uUseTexture", false);
        bool transp = !sl.backgroundColor.isValid()
                      || sl.backgroundColor == Qt::transparent
                      || sl.backgroundColor.alpha() == 0;
        QColor bg = transp ? QColor(180, 180, 200, 40) : sl.backgroundColor;
        m_prog->setUniformValue("uColor", QVector4D(bg.redF(), bg.greenF(), bg.blueF(),
                                                    transp ? 0.15f : 0.95f));
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    }

    // Content border: blue for selected, grey for others
    m_prog->setUniformValue("uUseTexture", false);
    glLineWidth(selected ? 2.f : 1.f);
    m_prog->setUniformValue("uColor", selected
        ? QVector4D(0.f, 0.6f, 1.f, 0.8f)
        : QVector4D(0.4f, 0.4f, 0.5f, 1.f));
    glDrawArrays(GL_LINE_LOOP, 0, 4);
    glLineWidth(1.f);

    m_quadVAO.release();
    m_prog->release();
}

void SlideEditor3D::renderWorldObject(const WorldObject& w, bool selected) {
    if (!m_meshProg) return;
    WorldObjectMesh* mesh = meshFor(w.modelPath);
    if (!mesh) return;

    QMatrix4x4 model = worldObjectModel(w, mesh->normalizationScale);
    QMatrix4x4 mvp = projMatrix() * viewMatrix() * model;

    m_meshProg->bind();
    m_meshProg->setUniformValue("uMVP", mvp);
    m_meshProg->setUniformValue("uModel", model);
    m_meshProg->setUniformValue("uLightDir", QVector3D(0.4f, 0.8f, 0.3f).normalized());
    m_meshProg->setUniformValue("uAmbient", 0.35f);
    m_meshProg->setUniformValue("uOpacity", w.opacity);
    m_meshProg->setUniformValue("uTex", 0);

    mesh->draw(m_meshProg);

    m_meshProg->release();

    if (selected) {
        // Selection outline: wireframe box around the mesh's bounding box.
        QVector3D bmin = mesh->bboxMin, bmax = mesh->bboxMax;
        QVector3D c[8] = {
            {bmin.x(),bmin.y(),bmin.z()}, {bmax.x(),bmin.y(),bmin.z()},
            {bmax.x(),bmax.y(),bmin.z()}, {bmin.x(),bmax.y(),bmin.z()},
            {bmin.x(),bmin.y(),bmax.z()}, {bmax.x(),bmin.y(),bmax.z()},
            {bmax.x(),bmax.y(),bmax.z()}, {bmin.x(),bmax.y(),bmax.z()},
        };
        for (auto& p : c) p = model.map(p);
        static const int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}
        };
        QVector<QVector3D> pts;
        for (auto& e : edges) { pts.append(c[e[0]]); pts.append(c[e[1]]); }
        glDisable(GL_DEPTH_TEST);
        drawLines(pts, QVector4D(0.f, 0.6f, 1.f, 0.8f), 2.f);
        glEnable(GL_DEPTH_TEST);
    }
}

void SlideEditor3D::renderAxes() {
    if (!m_prog) return;
    float axLen = 500.f;
    QVector<QVector3D> x = {{0,0,0},{axLen,0,0}};
    QVector<QVector3D> y = {{0,0,0},{0,axLen,0}};
    QVector<QVector3D> z = {{0,0,0},{0,0,axLen}};
    drawLines(x, {1,0,0,0.7f}, 2.f);
    drawLines(y, {0,1,0,0.7f}, 2.f);
    drawLines(z, {0.3f,0.5f,1,0.7f}, 2.f);
}

void SlideEditor3D::renderMoveGizmo(const GizmoTarget& t) {
    if (!t.valid()) return;
    float sz = gizmoSize();
    QVector3D center = t.pos();
    glDisable(GL_DEPTH_TEST);

    for (int ax = 0; ax < 3; ++ax) {
        QVector3D tip  = center + AXIS_VEC[ax] * sz;
        QVector3D shaftEnd = center + AXIS_VEC[ax] * (sz * 0.78f);

        QVector<QVector3D> pts;
        // Shaft
        pts.append(center); pts.append(shaftEnd);
        // Cone arrowhead
        addCone(pts, tip, AXIS_VEC[ax], sz * 0.05f, sz * 0.22f);

        bool active = (m_gizmoAxis == ax);
        drawLines(pts, active ? COLOR_HIGHLIGHT : AXIS_COLOR[ax], active ? 3.f : 2.f);
    }

    glEnable(GL_DEPTH_TEST);
}

void SlideEditor3D::renderRotateGizmo(const GizmoTarget& t) {
    if (!t.valid()) return;
    float sz = gizmoSize();
    QVector3D center = t.pos();
    glDisable(GL_DEPTH_TEST);

    const int N = 64;
    for (int ax = 0; ax < 3; ++ax) {
        QVector<QVector3D> pts;
        for (int i = 0; i < N; ++i) {
            float a0 = float(2*M_PI*i/N), a1 = float(2*M_PI*(i+1)/N);
            pts.append(ringPt(center,ax,sz,a0));
            pts.append(ringPt(center,ax,sz,a1));
        }
        bool active = (m_gizmoAxis == ax);
        drawLines(pts, active ? COLOR_HIGHLIGHT : AXIS_COLOR[ax], active ? 3.f : 2.f);
    }

    glEnable(GL_DEPTH_TEST);
}

namespace {
QPixmap tintedIcon(const QString& name, const QColor& color, int size) {
    QPixmap src = QIcon(":/icons/" + name + ".svg").pixmap(size, size);
    QPixmap tinted(src.size());
    tinted.fill(Qt::transparent);
    QPainter tp(&tinted);
    tp.drawPixmap(0, 0, src);
    tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
    tp.fillRect(tinted.rect(), color);
    tp.end();
    return tinted;
}
} // namespace

void SlideEditor3D::paintGL() {
    // Scene background from presentation settings
    if (m_pres && m_pres->sceneBackground.isValid()) {
        const QColor& bg = m_pres->sceneBackground;
        glClearColor(bg.redF(), bg.greenF(), bg.blueF(), 1.f);
    } else {
        glClearColor(0.13f, 0.13f, 0.15f, 1.f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_initialized || !m_prog) return;

    if (m_pres) {
        bool slideSel = (m_selKind == SelectionKind::Slide);
        bool woSel    = (m_selKind == SelectionKind::WorldObject);
        for (int i = 0; i < m_pres->slides.size(); ++i)
            renderSlide(m_pres->slides[i], slideSel && i == m_selectedSlide);
        for (int i = 0; i < m_pres->worldObjects.size(); ++i)
            renderWorldObject(m_pres->worldObjects[i], woSel && i == m_selectedWorldObj);

        GizmoTarget gt = currentTarget();
        if (gt.valid()) {
            if (m_gizmoMode == GizmoMode::Move)
                renderMoveGizmo(gt);
            else
                renderRotateGizmo(gt);
        }
    }
    renderAxes();

    // ── QPainter overlay ──
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Mode indicator
    static const QPixmap moveIcon   = tintedIcon("open_with",   QColor(80,200,255), 14);
    static const QPixmap rotateIcon = tintedIcon("3d_rotation", QColor(255,200,80), 14);
    const bool isMove = m_gizmoMode == GizmoMode::Move;
    painter.fillRect(6, 6, 160, 20, QColor(0,0,0,100));
    painter.drawPixmap(9, 9, isMove ? moveIcon : rotateIcon);
    painter.setPen(isMove ? QColor(80,200,255) : QColor(255,200,80));
    painter.setFont(QFont("Arial", 9));
    painter.drawText(28, 20, isMove ? "Move [W]" : "Rotate [E]");

    // Controls hint
    painter.setPen(QColor(200,200,200,160));
    painter.setFont(QFont("Arial", 8));
    painter.drawText(10, height() - 36, "Left-click: select slide / drag gizmo");
    painter.drawText(10, height() - 22, "Right-click+drag: camera pan  |  Scroll: zoom");
    painter.drawText(10, height() - 8,  "Left-click+drag (empty): camera orbit");

    // Viewport rectangles (projected to screen) + slide name labels
    if (m_pres) {
        static const QVector3D kC[4] = {
            {-960.f, -540.f, 0.f}, { 960.f, -540.f, 0.f},
            { 960.f,  540.f, 0.f}, {-960.f,  540.f, 0.f},
        };

        for (int i = 0; i < m_pres->slides.size(); ++i) {
            const Slide& sl = m_pres->slides[i];
            bool sel = (i == m_selectedSlide);

            float effW = (sl.slideWidth  > 0) ? sl.slideWidth  : (m_pres ? m_pres->slideWidth  : 1920.f);
            float effH = (sl.slideHeight > 0) ? sl.slideHeight : (m_pres ? m_pres->slideHeight : 1080.f);

            // Viewport model: same transform as slide but with scale×effDims
            QMatrix4x4 vm;
            vm.translate(sl.posX + sl.viewOffsetX, sl.posY + sl.viewOffsetY, sl.posZ);
            vm.rotate(sl.rotZ, 0, 0, 1);
            vm.rotate(sl.rotY, 0, 1, 0);
            vm.rotate(sl.rotX, 1, 0, 0);
            vm.scale(sl.scale * effW / 1920.f, sl.scale * effH / 1080.f, 1.f);

            // Project viewport corners to screen
            QPolygonF poly;
            bool anyVisible = false;
            for (int c = 0; c < 4; ++c) {
                QPointF sp = project(vm.map(kC[c]));
                if (sp.x() > -1e5) anyVisible = true;
                poly << sp;
            }

            if (anyVisible) {
                if (sel) {
                    painter.setPen(QPen(QColor(255, 50, 50, 230), 2.5));
                    painter.setBrush(QBrush(QColor(255, 0, 0, 18)));
                } else {
                    painter.setPen(QPen(QColor(180, 80, 80, 120), 1.0));
                    painter.setBrush(Qt::NoBrush);
                }
                painter.drawPolygon(poly);
            }

            // Slide name label
            QPointF sc = project(QVector3D(sl.posX, sl.posY, sl.posZ));
            painter.setBrush(Qt::NoBrush);
            painter.setFont(QFont("Arial", 10));
            painter.setPen(sel ? QColor(80, 200, 255) : Qt::white);
            painter.drawText(int(sc.x()) - 60, int(sc.y()) + 8, 120, 20,
                             Qt::AlignCenter, sl.name);

            if (sel) {
                painter.setPen(QColor(255, 80, 80));
                painter.setFont(QFont("Arial", 8));
                QString info = QString("Zoom: %1×").arg(sl.scale, 0, 'f', 2);
                if (sl.viewOffsetX != 0 || sl.viewOffsetY != 0)
                    info += QString("  Off: %1 / %2").arg(int(sl.viewOffsetX)).arg(int(sl.viewOffsetY));
                painter.drawText(int(sc.x()) - 80, int(sc.y()) + 26, 160, 16,
                                 Qt::AlignCenter, info);
            }
        }
    }

    // Axis labels for gizmo
    if (m_pres && currentTarget().valid()) {
        float sz = gizmoSize();
        QVector3D center = currentTarget().pos();
        const char* labels[] = {"X","Y","Z"};
        painter.setFont(QFont("Arial", 10, QFont::Bold));
        for (int ax = 0; ax < 3; ++ax) {
            QPointF tip = project(center + AXIS_VEC[ax] * (sz * 1.12f));
            QColor col(int(AXIS_COLOR[ax].x()*255), int(AXIS_COLOR[ax].y()*255),
                       int(AXIS_COLOR[ax].z()*255));
            painter.setPen(col);
            painter.drawText(int(tip.x()) - 6, int(tip.y()) + 5, labels[ax]);
        }
    }

    painter.end();
}

// ── Mouse events ──────────────────────────────────────────────────────────────

void SlideEditor3D::mousePressEvent(QMouseEvent* e) {
    setFocus();
    m_lastMouse = e->pos();

    if (e->button() == Qt::LeftButton) {
        // 1. Check gizmo hits first (only when something is selected)
        GizmoTarget gt = currentTarget();
        if (gt.valid()) {
            int gizmoAx = (m_gizmoMode == GizmoMode::Move)
                ? hitMoveAxis(e->pos(), gt) : hitRotateAxis(e->pos(), gt);

            if (gizmoAx >= 0) {
                m_gizmoAxis      = gizmoAx;
                m_gizmoDragPos0  = gt.pos();
                m_gizmoDragRot0  = getRot(gt, gizmoAx);
                m_gizmoDragPt0   = e->pos();

                if (m_gizmoMode == GizmoMode::Move) {
                    // Choose drag plane: contains chosen axis, faces camera as much as possible
                    QVector3D ax  = AXIS_VEC[gizmoAx];
                    QVector3D cd  = (m_target - camPos()).normalized();
                    QVector3D side = QVector3D::crossProduct(cd, ax);
                    if (side.length() < 0.001f) side = {0, 1, 0};
                    m_gizmoDragPlaneN =
                        QVector3D::crossProduct(ax, side.normalized()).normalized();
                    QVector3D ro, rd;
                    getRay(e->pos(), ro, rd);
                    rayPlane(ro, rd, m_gizmoDragPlaneN, m_gizmoDragPos0, m_gizmoDragHit0);
                }
                update();
                return;
            }
        }

        // 2. Pick: try both a slide and a world object, nearer-to-camera wins
        float slideDist = 1e18f, woDist = 1e18f;
        int slideHit = pickSlide(e->pos(), &slideDist);
        int woHit    = pickWorldObject(e->pos(), &woDist);

        if (slideHit < 0 && woHit < 0) {
            m_orbiting = true;
        } else if (woHit >= 0 && (slideHit < 0 || woDist < slideDist)) {
            m_selKind          = SelectionKind::WorldObject;
            m_selectedWorldObj = woHit;
            m_selectedSlide    = -1;
            emit worldObjectSelected(woHit);
            update();
        } else {
            m_selKind          = SelectionKind::Slide;
            m_selectedSlide    = slideHit;
            m_selectedWorldObj = -1;
            emit slideSelected(slideHit);
            update();
        }
    } else if (e->button() == Qt::RightButton) {
        m_panning = true;
    }
}

void SlideEditor3D::mouseMoveEvent(QMouseEvent* e) {
    QPoint delta = e->pos() - m_lastMouse;
    m_lastMouse  = e->pos();

    // Gizmo drag
    if (m_gizmoAxis >= 0) {
        GizmoTarget gt = currentTarget();
        if (gt.valid()) {
            bool snap = (e->modifiers() & Qt::ControlModifier);

            if (m_gizmoMode == GizmoMode::Move) {
                QVector3D ro, rd;
                getRay(e->pos(), ro, rd);
                QVector3D hit;
                if (rayPlane(ro, rd, m_gizmoDragPlaneN, m_gizmoDragPos0, hit)) {
                    QVector3D diff = hit - m_gizmoDragHit0;
                    float mv = QVector3D::dotProduct(diff, AXIS_VEC[m_gizmoAxis]);
                    QVector3D np = m_gizmoDragPos0 + AXIS_VEC[m_gizmoAxis] * mv;
                    if (snap) {
                        const float GRID = 100.f;
                        np.setX(std::roundf(np.x() / GRID) * GRID);
                        np.setY(std::roundf(np.y() / GRID) * GRID);
                        np.setZ(std::roundf(np.z() / GRID) * GRID);
                    }
                    *gt.px = np.x(); *gt.py = np.y(); *gt.pz = np.z();
                    if (m_selKind == SelectionKind::Slide)
                        m_dirty.insert(m_pres->slides[m_selectedSlide].id);
                    emit presentationModified();
                    update();
                }
            } else {
                // Rotation: horizontal drag for Y/Z, vertical drag for X
                int dx = e->pos().x() - m_gizmoDragPt0.x();
                int dy = e->pos().y() - m_gizmoDragPt0.y();
                float pixDelta = (m_gizmoAxis == 0) ? float(dy) : float(dx);
                float newRot = m_gizmoDragRot0 + pixDelta * 0.5f;
                if (snap) {
                    const float SNAP_DEG = 45.f;
                    newRot = std::roundf(newRot / SNAP_DEG) * SNAP_DEG;
                }
                setRot(gt, m_gizmoAxis, newRot);
                if (m_selKind == SelectionKind::Slide)
                    m_dirty.insert(m_pres->slides[m_selectedSlide].id);
                emit presentationModified();
                update();
            }
        }
        return;
    }

    // Camera controls
    if (m_orbiting) {
        m_azimuth   -= delta.x() * 0.4f;
        m_elevation += delta.y() * 0.4f;
        m_elevation  = qBound(-89.f, m_elevation, 89.f);
        update();
    } else if (m_panning) {
        QMatrix4x4 v = viewMatrix();
        QVector3D right = {v(0,0), v(0,1), v(0,2)};
        QVector3D up    = {v(1,0), v(1,1), v(1,2)};
        float speed = m_distance * 0.001f;
        m_target -= right * (delta.x() * speed);
        m_target += up    * (delta.y() * speed);
        update();
    }
}

void SlideEditor3D::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_orbiting  = false;
        m_gizmoAxis = -1;
    }
    if (e->button() == Qt::RightButton) m_panning = false;
}

void SlideEditor3D::wheelEvent(QWheelEvent* e) {
    float factor = e->angleDelta().y() > 0 ? 0.85f : 1.18f;
    m_distance = qMax(200.f, m_distance * factor);
    emit distanceChanged(m_distance);
    update();
}

void SlideEditor3D::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
        case Qt::Key_W: m_gizmoMode = GizmoMode::Move;   update(); break;
        case Qt::Key_E: m_gizmoMode = GizmoMode::Rotate; update(); break;
        default: QOpenGLWidget::keyPressEvent(e);
    }
}
