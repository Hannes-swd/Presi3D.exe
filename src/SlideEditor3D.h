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

class SlideEditor3D : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    enum class GizmoMode { Move, Rotate };

    explicit SlideEditor3D(QWidget* parent = nullptr);
    ~SlideEditor3D() override;

    void setPresentation(Presentation* pres, int selectedSlide);
    void setGizmoMode(GizmoMode m) { m_gizmoMode = m; update(); }
    GizmoMode gizmoMode() const { return m_gizmoMode; }

    void  setDistance(float d) { m_distance = qMax(200.f, d); emit distanceChanged(m_distance); update(); }
    float distance() const { return m_distance; }

signals:
    void presentationModified();
    void slideSelected(int index);
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
    void initGeometry();

    // ── Rendering ────────────────────────────────────────────────────────
    void renderSlide(const Slide&, bool selected);
    void renderAxes();
    QOpenGLTexture* buildSlideTexture(const Slide&);
    void renderMoveGizmo(const Slide&);
    void renderRotateGizmo(const Slide&);
    void drawLines(const QVector<QVector3D>& pts, const QVector4D& color,
                   float lineWidth = 1.f);

    // ── Gizmo geometry helpers ────────────────────────────────────────────
    float     gizmoSize() const { return m_distance * 0.12f; }
    QPointF   project(const QVector3D& p) const;
    float     distSeg2D(const QPointF& p, const QPointF& a, const QPointF& b) const;

    // ── Hit testing ───────────────────────────────────────────────────────
    int  hitMoveAxis(const QPoint& pt) const;
    int  hitRotateAxis(const QPoint& pt) const;
    int  pickSlide(const QPoint& pt) const;

    // ── Ray helpers ───────────────────────────────────────────────────────
    void getRay(const QPoint& pt, QVector3D& ro, QVector3D& rd) const;
    bool rayPlane(const QVector3D& ro, const QVector3D& rd,
                  const QVector3D& pn, const QVector3D& pp,
                  QVector3D& hit) const;

    // ── Camera maths ──────────────────────────────────────────────────────
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projMatrix() const;
    QMatrix4x4 slideModel(const Slide&) const;
    QVector3D  camPos() const;

    // ── State ─────────────────────────────────────────────────────────────
    Presentation* m_pres          = nullptr;
    int           m_selectedSlide = -1;
    bool          m_initialized   = false;

    GizmoMode m_gizmoMode = GizmoMode::Move;
    int       m_gizmoAxis = -1; // 0=X, 1=Y, 2=Z, -1=none

    // Gizmo drag state
    QVector3D m_gizmoDragPlaneN;
    QVector3D m_gizmoDragHit0;
    QVector3D m_gizmoDragPos0;
    float     m_gizmoDragRot0 = 0.f;
    QPoint    m_gizmoDragPt0;

    // OpenGL
    QOpenGLShaderProgram*    m_prog = nullptr;
    QOpenGLVertexArrayObject m_quadVAO;
    QOpenGLBuffer            m_quadVBO{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer            m_quadEBO{QOpenGLBuffer::IndexBuffer};

    // Slide textures
    QMap<QString, QOpenGLTexture*> m_textures;
    QSet<QString>                  m_dirty;

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
