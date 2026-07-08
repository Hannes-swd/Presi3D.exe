#include "WorldObjectMesh.h"
#include <QFileInfo>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QImage>
#include <QMap>
#include <QtGlobal>
#include <cstring>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

namespace {

constexpr int   MAX_TRIANGLES_SOFT = 500000;
constexpr int   MAX_TRIANGLES_HARD = 5000000;
constexpr float TARGET_SIZE        = 800.f; // world units the longest bbox axis is normalized to

// One vertex as laid out in WorldObjectMesh's interleaved VBO.
struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

QMatrix4x4 nodeLocalMatrix(const tinygltf::Node& node) {
    QMatrix4x4 m;
    if (node.matrix.size() == 16) {
        // glTF matrices are column-major; QMatrix4x4's flat-array ctor is row-major.
        float vals[16];
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                vals[row * 4 + col] = float(node.matrix[size_t(col) * 4 + size_t(row)]);
        m = QMatrix4x4(vals);
        return m;
    }
    QVector3D t(0, 0, 0), s(1, 1, 1);
    QQuaternion q;
    if (node.translation.size() == 3)
        t = QVector3D(float(node.translation[0]), float(node.translation[1]), float(node.translation[2]));
    if (node.scale.size() == 3)
        s = QVector3D(float(node.scale[0]), float(node.scale[1]), float(node.scale[2]));
    if (node.rotation.size() == 4)
        q = QQuaternion(float(node.rotation[3]), float(node.rotation[0]),
                         float(node.rotation[1]), float(node.rotation[2]));
    m.translate(t);
    m.rotate(q);
    m.scale(s);
    return m;
}

// Reads an accessor's raw components into a flat float array, honoring
// ByteStride (interleaved buffer views) and normalized integer formats.
QVector<float> readAccessorFloats(const tinygltf::Model& model, int accessorIdx) {
    QVector<float> out;
    if (accessorIdx < 0 || accessorIdx >= int(model.accessors.size())) return out;
    const tinygltf::Accessor& acc = model.accessors[size_t(accessorIdx)];
    if (acc.bufferView < 0 || acc.bufferView >= int(model.bufferViews.size())) return out;
    const tinygltf::BufferView& bv = model.bufferViews[size_t(acc.bufferView)];
    if (bv.buffer < 0 || bv.buffer >= int(model.buffers.size())) return out;
    const tinygltf::Buffer& buf = model.buffers[size_t(bv.buffer)];

    int numComponents = tinygltf::GetNumComponentsInType(uint32_t(acc.type));
    int stride = acc.ByteStride(bv);
    if (numComponents <= 0 || stride <= 0) return out;

    size_t needed = bv.byteOffset + acc.byteOffset + size_t(stride) * acc.count;
    if (needed > buf.data.size()) return out; // malformed/truncated buffer

    const unsigned char* base = buf.data.data() + bv.byteOffset + acc.byteOffset;
    out.resize(int(acc.count) * numComponents);
    for (size_t i = 0; i < acc.count; ++i) {
        const unsigned char* elem = base + i * size_t(stride);
        for (int c = 0; c < numComponents; ++c) {
            float v = 0.f;
            switch (acc.componentType) {
                case TINYGLTF_COMPONENT_TYPE_FLOAT: {
                    float f; std::memcpy(&f, elem + c * 4, 4); v = f; break;
                }
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                    unsigned char b = elem[c];
                    v = acc.normalized ? float(b) / 255.f : float(b); break;
                }
                case TINYGLTF_COMPONENT_TYPE_BYTE: {
                    auto b = reinterpret_cast<const signed char*>(elem)[c];
                    v = acc.normalized ? qMax(float(b) / 127.f, -1.f) : float(b); break;
                }
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                    unsigned short s; std::memcpy(&s, elem + c * 2, 2);
                    v = acc.normalized ? float(s) / 65535.f : float(s); break;
                }
                case TINYGLTF_COMPONENT_TYPE_SHORT: {
                    short s; std::memcpy(&s, elem + c * 2, 2);
                    v = acc.normalized ? qMax(float(s) / 32767.f, -1.f) : float(s); break;
                }
                default: v = 0.f;
            }
            out[int(i) * numComponents + c] = v;
        }
    }
    return out;
}

// Reads an index accessor as uint32; if accessorIdx < 0 (non-indexed draw),
// generates a sequential 0..vertexCount-1 index list instead.
QVector<quint32> readIndices(const tinygltf::Model& model, int accessorIdx, int vertexCount) {
    QVector<quint32> out;
    if (accessorIdx < 0) {
        out.resize(vertexCount);
        for (int i = 0; i < vertexCount; ++i) out[i] = quint32(i);
        return out;
    }
    if (accessorIdx >= int(model.accessors.size())) return out;
    const tinygltf::Accessor& acc = model.accessors[size_t(accessorIdx)];
    if (acc.bufferView < 0 || acc.bufferView >= int(model.bufferViews.size())) return out;
    const tinygltf::BufferView& bv = model.bufferViews[size_t(acc.bufferView)];
    if (bv.buffer < 0 || bv.buffer >= int(model.buffers.size())) return out;
    const tinygltf::Buffer& buf = model.buffers[size_t(bv.buffer)];

    int stride = acc.ByteStride(bv);
    if (stride <= 0) return out;
    size_t needed = bv.byteOffset + acc.byteOffset + size_t(stride) * acc.count;
    if (needed > buf.data.size()) return out;

    const unsigned char* base = buf.data.data() + bv.byteOffset + acc.byteOffset;
    out.resize(int(acc.count));
    for (size_t i = 0; i < acc.count; ++i) {
        const unsigned char* elem = base + i * size_t(stride);
        quint32 v = 0;
        switch (acc.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: v = elem[0]; break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                unsigned short s; std::memcpy(&s, elem, 2); v = s; break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                unsigned int u; std::memcpy(&u, elem, 4); v = u; break;
            }
            default: v = 0;
        }
        out[int(i)] = v;
    }
    return out;
}

} // namespace

WorldObjectMesh::WorldObjectMesh() {
    initializeOpenGLFunctions();
}

WorldObjectMesh::~WorldObjectMesh() {
    m_vao.destroy();
    m_vbo.destroy();
    m_ebo.destroy();
    qDeleteAll(m_textures);
}

WorldObjectMesh* WorldObjectMesh::load(const QString& path, QString& errorOut) {
    try {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        QString lower = path.toLower();
        bool ok = lower.endsWith(".glb")
            ? loader.LoadBinaryFromFile(&model, &err, &warn, path.toStdString())
            : loader.LoadASCIIFromFile(&model, &err, &warn, path.toStdString());

        if (!warn.empty())
            qWarning("WorldObjectMesh: %s: %s", qPrintable(path), warn.c_str());
        if (!ok) {
            errorOut = err.empty() ? QObject::tr("Unknown glTF parse error") : QString::fromStdString(err);
            return nullptr;
        }
        if (model.meshes.empty()) {
            errorOut = QObject::tr("File contains no meshes.");
            return nullptr;
        }

        QVector<Vertex> vertices;
        QVector<quint32> indices;
        QVector<WorldObjectSubMesh> subMeshes;
        QVector3D bboxMin(1e18f, 1e18f, 1e18f), bboxMax(-1e18f, -1e18f, -1e18f);
        // (image index in model.images) -> texture index in the resulting mesh's texture list
        QMap<int, int> imageToTexIndex;
        QVector<QImage> pendingImages; // parallel to imageToTexIndex values, GL textures created after context checks

        qint64 totalTriangles = 0;
        bool hardCapHit = false;

        // Recursively walk the node graph, baking each node's world transform
        // into its mesh's vertex positions/normals.
        std::function<void(int, const QMatrix4x4&)> visit = [&](int nodeIdx, const QMatrix4x4& parent) {
            if (hardCapHit) return;
            if (nodeIdx < 0 || nodeIdx >= int(model.nodes.size())) return;
            const tinygltf::Node& node = model.nodes[size_t(nodeIdx)];
            QMatrix4x4 world = parent * nodeLocalMatrix(node);

            if (node.mesh >= 0 && node.mesh < int(model.meshes.size())) {
                const tinygltf::Mesh& mesh = model.meshes[size_t(node.mesh)];
                for (const tinygltf::Primitive& prim : mesh.primitives) {
                    if (hardCapHit) break;
                    if (prim.mode != TINYGLTF_MODE_TRIANGLES && prim.mode != -1) continue;
                    auto posIt = prim.attributes.find("POSITION");
                    if (posIt == prim.attributes.end()) continue;

                    QVector<float> pos = readAccessorFloats(model, posIt->second);
                    if (pos.isEmpty() || pos.size() % 3 != 0) continue;
                    int vcount = pos.size() / 3;

                    QVector<float> nrm;
                    auto nrmIt = prim.attributes.find("NORMAL");
                    if (nrmIt != prim.attributes.end()) nrm = readAccessorFloats(model, nrmIt->second);

                    QVector<float> uv;
                    auto uvIt = prim.attributes.find("TEXCOORD_0");
                    if (uvIt != prim.attributes.end()) uv = readAccessorFloats(model, uvIt->second);

                    QVector<quint32> localIdx = readIndices(model, prim.indices, vcount);
                    if (localIdx.isEmpty()) continue;

                    qint64 triCount = localIdx.size() / 3;
                    totalTriangles += triCount;
                    if (totalTriangles > MAX_TRIANGLES_HARD) { hardCapHit = true; break; }
                    if (totalTriangles > MAX_TRIANGLES_SOFT)
                        qWarning("WorldObjectMesh: %s exceeds %d triangles (soft limit), loading anyway",
                                 qPrintable(path), MAX_TRIANGLES_SOFT);

                    bool hasNormals = (nrm.size() == vcount * 3);
                    bool hasUv      = (uv.size() == vcount * 2);

                    QVector<Vertex> localVerts(vcount);
                    for (int i = 0; i < vcount; ++i) {
                        QVector3D p(pos[i*3+0], pos[i*3+1], pos[i*3+2]);
                        p = world.map(p);
                        QVector3D n = hasNormals
                            ? world.mapVector(QVector3D(nrm[i*3+0], nrm[i*3+1], nrm[i*3+2])).normalized()
                            : QVector3D(0, 1, 0);
                        localVerts[i] = { p.x(), p.y(), p.z(), n.x(), n.y(), n.z(),
                                          hasUv ? uv[i*2+0] : 0.f, hasUv ? uv[i*2+1] : 0.f };
                        bboxMin.setX(qMin(bboxMin.x(), p.x())); bboxMax.setX(qMax(bboxMax.x(), p.x()));
                        bboxMin.setY(qMin(bboxMin.y(), p.y())); bboxMax.setY(qMax(bboxMax.y(), p.y()));
                        bboxMin.setZ(qMin(bboxMin.z(), p.z())); bboxMax.setZ(qMax(bboxMax.z(), p.z()));
                    }

                    // Compute smooth-ish normals for primitives that didn't provide any.
                    if (!hasNormals) {
                        QVector<QVector3D> accum(vcount, QVector3D(0,0,0));
                        for (int t = 0; t + 2 < localIdx.size(); t += 3) {
                            quint32 ia = localIdx[t], ib = localIdx[t+1], ic = localIdx[t+2];
                            if (ia >= quint32(vcount) || ib >= quint32(vcount) || ic >= quint32(vcount)) continue;
                            QVector3D a(localVerts[ia].px, localVerts[ia].py, localVerts[ia].pz);
                            QVector3D b(localVerts[ib].px, localVerts[ib].py, localVerts[ib].pz);
                            QVector3D c(localVerts[ic].px, localVerts[ic].py, localVerts[ic].pz);
                            QVector3D fn = QVector3D::crossProduct(b - a, c - a);
                            accum[ia] += fn; accum[ib] += fn; accum[ic] += fn;
                        }
                        for (int i = 0; i < vcount; ++i) {
                            QVector3D n = accum[i].length() > 1e-8f ? accum[i].normalized() : QVector3D(0,1,0);
                            localVerts[i].nx = n.x(); localVerts[i].ny = n.y(); localVerts[i].nz = n.z();
                        }
                    }

                    WorldObjectSubMesh sm;
                    sm.indexOffset = indices.size();
                    sm.indexCount  = localIdx.size();

                    // Material / texture
                    if (prim.material >= 0 && prim.material < int(model.materials.size())) {
                        const tinygltf::Material& mat = model.materials[size_t(prim.material)];
                        const auto& bcf = mat.pbrMetallicRoughness.baseColorFactor;
                        if (bcf.size() == 4)
                            sm.baseColorFactor = QVector4D(float(bcf[0]), float(bcf[1]), float(bcf[2]), float(bcf[3]));
                        int texIdx = mat.pbrMetallicRoughness.baseColorTexture.index;
                        if (texIdx >= 0 && texIdx < int(model.textures.size())) {
                            int imgIdx = model.textures[size_t(texIdx)].source;
                            if (imgIdx >= 0 && imgIdx < int(model.images.size())) {
                                if (!imageToTexIndex.contains(imgIdx)) {
                                    const tinygltf::Image& img = model.images[size_t(imgIdx)];
                                    if (!img.image.empty() && img.width > 0 && img.height > 0 && img.component == 4) {
                                        QImage qimg(reinterpret_cast<const uchar*>(img.image.data()),
                                                    img.width, img.height, img.width * 4,
                                                    QImage::Format_RGBA8888);
                                        pendingImages.append(qimg.copy());
                                        imageToTexIndex[imgIdx] = pendingImages.size() - 1;
                                    }
                                }
                                if (imageToTexIndex.contains(imgIdx))
                                    sm.textureIndex = imageToTexIndex[imgIdx];
                            }
                        }
                    }

                    quint32 base = quint32(vertices.size());
                    vertices += localVerts;
                    for (quint32 li : localIdx) indices.append(li + base);
                    subMeshes.append(sm);
                }
            }

            for (int child : node.children) visit(child, world);
        };

        int sceneIdx = model.defaultScene >= 0 ? model.defaultScene
                     : (!model.scenes.empty() ? 0 : -1);
        if (sceneIdx >= 0 && sceneIdx < int(model.scenes.size())) {
            for (int root : model.scenes[size_t(sceneIdx)].nodes)
                visit(root, QMatrix4x4());
        } else {
            for (int i = 0; i < int(model.nodes.size()); ++i) visit(i, QMatrix4x4());
        }

        if (hardCapHit) {
            errorOut = QObject::tr("Model exceeds %1 triangles; not loading.").arg(MAX_TRIANGLES_HARD);
            return nullptr;
        }
        if (vertices.isEmpty() || indices.isEmpty()) {
            errorOut = QObject::tr("No renderable triangle geometry found in file.");
            return nullptr;
        }

        auto* result = new WorldObjectMesh();
        result->m_vao.create();
        result->m_vao.bind();
        result->m_vbo.create();
        result->m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        result->m_vbo.bind();
        result->m_vbo.allocate(vertices.constData(), vertices.size() * int(sizeof(Vertex)));
        result->m_ebo.create();
        result->m_ebo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        result->m_ebo.bind();
        result->m_ebo.allocate(indices.constData(), indices.size() * int(sizeof(quint32)));

        result->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, px));
        result->glEnableVertexAttribArray(0);
        result->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nx));
        result->glEnableVertexAttribArray(1);
        result->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
        result->glEnableVertexAttribArray(2);
        result->m_vao.release();
        result->m_vbo.release();

        result->m_subMeshes = subMeshes;
        for (const QImage& img : pendingImages) {
            auto* tex = new QOpenGLTexture(img);
            tex->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
            tex->setMagnificationFilter(QOpenGLTexture::Linear);
            tex->generateMipMaps();
            result->m_textures.append(tex);
        }

        result->bboxMin = bboxMin;
        result->bboxMax = bboxMax;
        QVector3D extent = bboxMax - bboxMin;
        float largest = qMax(extent.x(), qMax(extent.y(), extent.z()));
        result->normalizationScale = largest > 1e-6f ? (TARGET_SIZE / largest) : 1.f;

        return result;
    } catch (const std::exception& e) {
        errorOut = QString::fromUtf8(e.what());
        return nullptr;
    } catch (...) {
        errorOut = QObject::tr("Unknown error while loading model.");
        return nullptr;
    }
}

void WorldObjectMesh::draw(QOpenGLShaderProgram* prog) {
    if (!prog) return;
    m_vao.bind();
    for (const WorldObjectSubMesh& sm : m_subMeshes) {
        if (sm.textureIndex >= 0 && sm.textureIndex < m_textures.size() && m_textures[sm.textureIndex]->isCreated()) {
            m_textures[sm.textureIndex]->bind(0);
            prog->setUniformValue("uUseTexture", true);
            prog->setUniformValue("uTex", 0);
        } else {
            prog->setUniformValue("uUseTexture", false);
            prog->setUniformValue("uColor", sm.baseColorFactor);
        }
        glDrawElements(GL_TRIANGLES, sm.indexCount, GL_UNSIGNED_INT,
                        (void*)(qintptr(sm.indexOffset) * sizeof(quint32)));
        if (sm.textureIndex >= 0 && sm.textureIndex < m_textures.size() && m_textures[sm.textureIndex]->isCreated())
            m_textures[sm.textureIndex]->release();
    }
    m_vao.release();
}
