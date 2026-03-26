#pragma once

#include "asset/Asset.h"
#include "asset/MaterialAsset.h"
#include "Buffer.h"
#include "core/AABB.h"
#include <vector>
#include <string>
#include <filesystem>
#include <glm/glm.hpp>

namespace GLRenderer {

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec3 Tangent;
    glm::vec3 Binormal;
    glm::vec2 TexCoord;
};

struct Submesh {
    uint32_t BaseVertex = 0;
    uint32_t BaseIndex = 0;
    uint32_t VertexCount = 0;
    uint32_t IndexCount = 0;
    uint32_t MaterialIndex = 0;

    glm::mat4 Transform{1.0f};
    glm::mat4 LocalTransform{1.0f};
    AABB BoundingBox;

    std::string MeshName;
    std::string NodeName;
};

using Index = uint32_t;

class MeshSource : public Asset {
public:
    MeshSource() = default;
    MeshSource(const std::vector<Vertex>& vertices,
               const std::vector<Index>& indices,
               const glm::mat4& transform = glm::mat4(1.0f));
    ~MeshSource() = default;

    const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
    const std::vector<Index>& GetIndices() const { return m_Indices; }
    const std::vector<Submesh>& GetSubmeshes() const { return m_Submeshes; }
    const std::vector<AssetHandle>& GetMaterials() const { return m_Materials; }

    Ref<VertexBuffer> GetVertexBuffer() const { return m_VertexBuffer; }
    Ref<IndexBuffer> GetIndexBuffer() const { return m_IndexBuffer; }
    Ref<VertexArray> GetVertexArray() const { return m_VertexArray; }

    const AABB& GetBoundingBox() const { return m_BoundingBox; }
    const std::filesystem::path& GetFilePath() const { return m_FilePath; }

    uint32_t GetVertexCount() const { return static_cast<uint32_t>(m_Vertices.size()); }
    uint32_t GetIndexCount() const { return static_cast<uint32_t>(m_Indices.size()); }
    uint32_t GetSubmeshCount() const { return static_cast<uint32_t>(m_Submeshes.size()); }

    void AddSubmesh(const Submesh& submesh);
    void SetMaterials(const std::vector<AssetHandle>& materials) { m_Materials = materials; }

    void CreateGPUBuffers();
    bool HasGPUBuffers() const { return m_VertexBuffer && m_IndexBuffer; }

    static AssetType GetStaticType() { return AssetType::MeshSource; }
    AssetType GetAssetType() const override { return GetStaticType(); }

    static Ref<MeshSource> Create(const std::vector<Vertex>& vertices,
                                   const std::vector<Index>& indices,
                                   const glm::mat4& transform = glm::mat4(1.0f));
    static Ref<MeshSource> Create(const std::filesystem::path& path);

private:
    friend class AssimpMeshImporter;

    void CalculateBoundingBox();
    void CalculateTangents();

    std::vector<Vertex> m_Vertices;
    std::vector<Index> m_Indices;
    std::vector<Submesh> m_Submeshes;
    std::vector<AssetHandle> m_Materials;

    Ref<VertexBuffer> m_VertexBuffer;
    Ref<IndexBuffer> m_IndexBuffer;
    Ref<VertexArray> m_VertexArray;

    AABB m_BoundingBox;
    std::filesystem::path m_FilePath;
};

class StaticMesh : public Asset {
public:
    StaticMesh() = default;
    explicit StaticMesh(AssetHandle meshSource);
    StaticMesh(AssetHandle meshSource, const std::vector<uint32_t>& submeshes);
    ~StaticMesh() = default;

    AssetHandle GetMeshSource() const { return m_MeshSource; }
    void SetMeshSource(AssetHandle handle) { m_MeshSource = handle; }

    Ref<MaterialTable> GetMaterials() const { return m_Materials; }
    void SetMaterials(const Ref<MaterialTable>& materials) { m_Materials = materials; }

    const std::vector<uint32_t>& GetSubmeshes() const { return m_Submeshes; }
    void SetSubmeshes(const std::vector<uint32_t>& submeshes) { m_Submeshes = submeshes; }

    static AssetType GetStaticType() { return AssetType::StaticMesh; }
    AssetType GetAssetType() const override { return GetStaticType(); }

    static Ref<StaticMesh> Create(AssetHandle meshSource);
    static Ref<StaticMesh> Create(const Ref<MeshSource>& meshSource);
    static Ref<StaticMesh> Create(AssetHandle meshSource, const std::vector<uint32_t>& submeshes);

private:
    AssetHandle m_MeshSource = 0;
    std::vector<uint32_t> m_Submeshes;
    Ref<MaterialTable> m_Materials;
};

} // namespace GLRenderer
