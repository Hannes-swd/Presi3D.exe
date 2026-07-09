#pragma once
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QMatrix4x4>
#include <QVector3D>
#include <QMap>
#include <QSet>
#include "models/DataModel.h"

class WorldObjectMesh;

// A small pointer-based view over "something with a position + per-axis
// rotation" — either a Slide or a WorldObject. Lets the Move/Rotate gizmo and
// its hit-testing be shared between both entity kinds without templating or
// a shared base class (which would ripple into every other file using Slide).
struct GizmoTarget {
    float *px = nullptr, *py = nullptr, *pz = nullptr;
    float *rx = nullptr, *ry = nullptr, *rz = nullptr;
    float *pscale = nullptr;
    bool valid() const { return px != nullptr; }
    QVector3D pos() const { return {*px, *py, *pz}; }
};
// Slide::scale is viewport zoom (inverted: bigger number = zoomed OUT), not a
// geometric size — deliberately left out of the gizmo so drag-to-scale can't
// give it the opposite-of-expected "bigger drag = smaller slide" feel.
inline GizmoTarget targetOf(Slide& s) { return {&s.posX,&s.posY,&s.posZ,&s.rotX,&s.rotY,&s.rotZ,nullptr}; }
inline GizmoTarget targetOf(WorldObject& w) { return {&w.posX,&w.posY,&w.posZ,&w.rotX,&w.rotY,&w.rotZ,&w.scale}; }

class SlideEditor3D : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    enum class GizmoMode { Move, Rotate, Scale };
    enum class SelectionKind { None, Slide, WorldObject };

    explicit SlideEditor3D(QWidget* parent = nullptr);
    ~SlideEditor3D() override;

    void setPresentation(Presentation* pres, int selectedSlide);
    void setGizmoMode(GizmoMode m) { m_gizmoMode = m; update(); }
    GizmoMode gizmoMode() const { return m_gizmoMode; }

    void  setDistance(float d) { m_distance = qMax(200.f, d); emit distanceChanged(m_distance); update(); }
    float distance() const { return m_distance; }

    // Loads path via tinygltf, appends a new WorldObject at the current
    // camera target, and selects it. Shows a warning dialog (never crashes)
    // if the file can't be loaded.
    void addWorldObject(const QString& path);

signals:
    void presentationModified();
    void slideSelected(int index);
    void worldObjectSelected(int index);
    void distanceChanged(float d);

public slots:
    void markAllDirty() {
        if (m_pres)
            for (const auto& s : m_pres->slides) m_dirty.insert(s.id);
        update();
    }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    // ── GL setup ─────────────────────────────────────────────────────────
    bool initShaders();
    bool initMeshShader();
    void initGeometry();

    // ── Rendering ────────────────────────────────────────────────────────
    void renderSlide(const Slide&, bool selected);
    void renderWorldObject(const WorldObject&, bool selected);
    void renderAxes();
    QOpenGLTexture* buildSlideTexture(const Slide&);
    void renderMoveGizmo(const GizmoTarget&);
    void renderRotateGizmo(const GizmoTarget&);
    void renderScaleGizmo(const GizmoTarget&);
    void drawLines(const QVector<QVector3D>& pts, const QVector4D& color,
                   float lineWidth = 1.f);

    // Builds a GizmoTarget for whichever entity is currently selected
    // (m_selKind), or an invalid (all-null) GizmoTarget if nothing is selected.
    GizmoTarget currentTarget() const;

    // ── World object mesh cache ─────────────────────────────────────────
    // Returns the cached mesh for modelPath, lazily loading it (and logging
    // any load error) on first access. May return nullptr if loading failed.
    WorldObjectMesh* meshFor(const QString& modelPath);

    // ── Gizmo geometry helpers ────────────────────────────────────────────
    float     gizmoSize() const { return m_distance * 0.12f; }
    QPointF   project(const QVector3D& p) const;
    float     distSeg2D(const QPointF& p, const QPointF& a, const QPointF& b) const;

    // ── Hit testing ───────────────────────────────────────────────────────
    int  hitMoveAxis(const QPoint& pt, const GizmoTarget&) const;
    int  hitRotateAxis(const QPoint& pt, const GizmoTarget&) const;
    int  pickSlide(const QPoint& pt, float* outDist = nullptr) const;
    int  pickWorldObject(const QPoint& pt, float* outDist = nullptr) const;

    // ── Ray helpers ───────────────────────────────────────────────────────
    void getRay(const QPoint& pt, QVector3D& ro, QVector3D& rd) const;
    bool rayPlane(const QVector3D& ro, const QVector3D& rd,
                  const QVector3D& pn, const QVector3D& pp,
                  QVector3D& hit) const;

    // ── Camera maths ──────────────────────────────────────────────────────
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projMatrix() const;
    QMatrix4x4 slideModel(const Slide&) const;
    QMatrix4x4 worldObjectModel(const WorldObject&, float meshNormScale) const;
    QVector3D  camPos() const;

    // ── State ─────────────────────────────────────────────────────────────
    Presentation* m_pres            = nullptr;
    SelectionKind m_selKind         = SelectionKind::None;
    int           m_selectedSlide   = -1; // valid iff m_selKind == Slide
    int           m_selectedWorldObj = -1; // valid iff m_selKind == WorldObject
    bool          m_initialized     = false;

    GizmoMode m_gizmoMode = GizmoMode::Move;
    int       m_gizmoAxis = -1; // 0=X, 1=Y, 2=Z, -1=none

    // Gizmo drag state
    QVector3D m_gizmoDragPlaneN;
    QVector3D m_gizmoDragHit0;
    QVector3D m_gizmoDragPos0;
    float     m_gizmoDragRot0   = 0.f;
    float     m_gizmoDragScale0 = 1.f;
    QPoint    m_gizmoDragPt0;

    // OpenGL
    QOpenGLShaderProgram*    m_prog = nullptr;
    QOpenGLShaderProgram*    m_meshProg = nullptr; // lit shader for WorldObject meshes
    QOpenGLVertexArrayObject m_quadVAO;
    QOpenGLBuffer            m_quadVBO{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer            m_quadEBO{QOpenGLBuffer::IndexBuffer};

    // Slide textures
    QMap<QString, QOpenGLTexture*> m_textures;
    QSet<QString>                  m_dirty;

    // World object meshes, keyed by WorldObject::modelPath
    QMap<QString, WorldObjectMesh*> m_meshCache;
    QSet<QString>                   m_meshLoadFailed; // paths we already warned about, don't retry every frame

    // Camera (spherical)
    float     m_azimuth   = 30.f;
    float     m_elevation = 25.f;
    float     m_distance  = 6000.f;
    QVector3D m_target    = {0, 0, 0};

    // Mouse state
    QPoint m_lastMouse;
    bool   m_orbiting = false;
    bool   m_panning  = false;
};
