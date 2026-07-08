#pragma once
#include <QString>
#include <QVector>
#include <QVector3D>
#include <QVector4D>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>

// One drawable range within WorldObjectMesh's shared vertex/index buffers,
// corresponding to a single glTF primitive.
struct WorldObjectSubMesh {
    int       indexOffset   = 0;   // offset into the mesh's index buffer, in indices (not bytes)
    int       indexCount    = 0;
    int       textureIndex  = -1;  // index into WorldObjectMesh::m_textures, -1 = untextured
    QVector4D baseColorFactor = {1.f, 1.f, 1.f, 1.f};
};

// A loaded, GPU-resident glTF/GLB mesh. All node transforms are baked into
// vertex positions/normals at load time, so a WorldObjectMesh renders as one
// flat list of submeshes sharing a single VAO — no runtime node graph.
//
// Static geometry only: skeletal animation and morph targets are not supported.
class WorldObjectMesh : protected QOpenGLFunctions {
public:
    // Requires a current OpenGL context. Returns nullptr and fills errorOut on
    // any failure (malformed file, unsupported content, too many triangles) —
    // never throws or crashes on bad input.
    static WorldObjectMesh* load(const QString& path, QString& errorOut);
    ~WorldObjectMesh();

    // Binds the VAO and draws every submesh, setting uUseTexture/uTex/uColor
    // on prog for each. Caller is responsible for uMVP/uModel and any lighting
    // uniforms before calling this.
    void draw(QOpenGLShaderProgram* prog);

    QVector3D bboxMin{0.f, 0.f, 0.f};
    QVector3D bboxMax{0.f, 0.f, 0.f};
    float     normalizationScale = 1.f; // TARGET_SIZE / largest bbox extent

private:
    WorldObjectMesh();

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer            m_vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer            m_ebo{QOpenGLBuffer::IndexBuffer};
    QVector<WorldObjectSubMesh> m_subMeshes;
    QVector<QOpenGLTexture*>    m_textures;
};
