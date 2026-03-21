#include "MeshSource.h"
#include "asset/AssimpMeshImporter.h"

namespace GLRenderer {

MeshSource::MeshSource(const std::vector<Vertex>& vertices,
                       const std::vector<Index>& indices,
                       const glm::mat4& transform)
    : m_Vertices(vertices), m_Indices(indices) {

    if (!m_Vertices.empty()) {
        Submesh submesh;
        submesh.BaseVertex = 0;
        submesh.BaseIndex = 0;
        submesh.VertexCount = static_cast<uint32_t>(vertices.size());
        submesh.IndexCount = static_cast<uint32_t>(indices.size());
        submesh.Transform = transform;
        submesh.LocalTransform = transform;
        submesh.MeshName = "Default";
        m_Submeshes.push_back(submesh);

        CalculateBoundingBox();
        CreateGPUBuffers();
    }
}

void MeshSource::AddSubmesh(const Submesh& submesh) {
    m_Submeshes.push_back(submesh);
}

void MeshSource::CreateGPUBuffers() {
    if (m_Vertices.empty()) return;

    m_VertexBuffer = VertexBuffer::Create(
        m_Vertices.data(),
        static_cast<uint32_t>(m_Vertices.size() * sizeof(Vertex))
    );

    if (!m_Indices.empty()) {
        m_IndexBuffer = IndexBuffer::Create(
            m_Indices.data(),
            static_cast<uint32_t>(m_Indices.size())
        );
    }

    // 创建 VAO 并设置顶点属性布局
    m_VertexArray = VertexArray::Create();

    // Vertex 结构体布局:
    // Position:  vec3, offset 0
    // Normal:    vec3, offset 12
    // Tangent:   vec3, offset 24
    // Binormal:  vec3, offset 36
    // TexCoord:  vec2, offset 48
    // Total stride: 56 bytes
    constexpr uint32_t stride = sizeof(Vertex);
    std::vector<VertexAttribute> layout = {
        VertexAttribute::Float(0, 3, stride, offsetof(Vertex, Position)),   // a_Position
        VertexAttribute::Float(1, 3, stride, offsetof(Vertex, Normal)),     // a_Normal
        VertexAttribute::Float(2, 2, stride, offsetof(Vertex, TexCoord)),   // a_TexCoords
        VertexAttribute::Float(3, 3, stride, offsetof(Vertex, Tangent)),    // a_Tangent
        VertexAttribute::Float(4, 3, stride, offsetof(Vertex, Binormal)),   // a_Binormal
    };

    m_VertexArray->AddVertexBuffer(m_VertexBuffer, layout);
    if (m_IndexBuffer) {
        m_VertexArray->SetIndexBuffer(m_IndexBuffer);
    }
}

void MeshSource::CalculateBoundingBox() {
    if (m_Vertices.empty()) return;

    glm::vec3 min = m_Vertices[0].Position;
    glm::vec3 max = m_Vertices[0].Position;

    for (const auto& vertex : m_Vertices) {
        min = glm::min(min, vertex.Position);
        max = glm::max(max, vertex.Position);
    }

    m_BoundingBox = AABB(min, max);

    for (auto& submesh : m_Submeshes) {
        submesh.BoundingBox = m_BoundingBox;
    }
}

void MeshSource::CalculateTangents() {
    if (m_Indices.empty() || m_Vertices.empty()) return;

    for (size_t i = 0; i < m_Indices.size(); i += 3) {
        Vertex& v0 = m_Vertices[m_Indices[i]];
        Vertex& v1 = m_Vertices[m_Indices[i + 1]];
        Vertex& v2 = m_Vertices[m_Indices[i + 2]];

        glm::vec3 edge1 = v1.Position - v0.Position;
        glm::vec3 edge2 = v2.Position - v0.Position;
        glm::vec2 deltaUV1 = v1.TexCoord - v0.TexCoord;
        glm::vec2 deltaUV2 = v2.TexCoord - v0.TexCoord;

        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y + 0.0001f);

        glm::vec3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
        tangent = glm::normalize(tangent);

        glm::vec3 binormal;
        binormal.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
        binormal.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
        binormal.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
        binormal = glm::normalize(binormal);

        v0.Tangent = v1.Tangent = v2.Tangent = tangent;
        v0.Binormal = v1.Binormal = v2.Binormal = binormal;
    }
}

Ref<MeshSource> MeshSource::Create(const std::vector<Vertex>& vertices,
                                    const std::vector<Index>& indices,
                                    const glm::mat4& transform) {
    return Ref<MeshSource>(new MeshSource(vertices, indices, transform));
}

Ref<MeshSource> MeshSource::Create(const std::filesystem::path& path) {
    AssimpMeshImporter importer(path);
    return importer.ImportToMeshSource();
}

StaticMesh::StaticMesh(AssetHandle meshSource)
    : m_MeshSource(meshSource) {
    m_Materials = MaterialTable::Create();
}

StaticMesh::StaticMesh(AssetHandle meshSource, const std::vector<uint32_t>& submeshes)
    : m_MeshSource(meshSource), m_Submeshes(submeshes) {
    m_Materials = MaterialTable::Create();
}

Ref<StaticMesh> StaticMesh::Create(AssetHandle meshSource) {
    return Ref<StaticMesh>(new StaticMesh(meshSource));
}

Ref<StaticMesh> StaticMesh::Create(const Ref<MeshSource>& meshSource) {
    if (!meshSource) return nullptr;
    return Ref<StaticMesh>(new StaticMesh(meshSource->Handle));
}

} // namespace GLRenderer
