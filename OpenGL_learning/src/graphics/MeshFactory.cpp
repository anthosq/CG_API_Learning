#include "MeshFactory.h"
#include "MeshSource.h"
#include "asset/AssetManager.h"
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace GLRenderer {

// 辅助函数：从 Position/Normal/TexCoord 计算 Tangent/Binormal
static void CalculateTangentBasis(Vertex& v) {
    // 对于基元几何体，使用简单的切线空间计算
    // Tangent 通常沿着纹理 U 方向
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(v.Normal, up)) > 0.99f) {
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    v.Tangent = glm::normalize(glm::cross(up, v.Normal));
    v.Binormal = glm::normalize(glm::cross(v.Normal, v.Tangent));
}

AssetHandle MeshFactory::CreateCube() {
    std::vector<Vertex> vertices;
    std::vector<Index> indices;

    // 立方体 6 个面，每面 4 个顶点
    const float s = 0.5f;

    // 定义每个面的顶点 (position, normal, texcoord)
    struct FaceData {
        glm::vec3 positions[4];
        glm::vec3 normal;
        glm::vec2 texcoords[4];
    };

    FaceData faces[6] = {
        // Front (+Z)
        {{{-s, -s, s}, {s, -s, s}, {s, s, s}, {-s, s, s}}, {0, 0, 1}, {{0,0}, {1,0}, {1,1}, {0,1}}},
        // Back (-Z)
        {{{s, -s, -s}, {-s, -s, -s}, {-s, s, -s}, {s, s, -s}}, {0, 0, -1}, {{0,0}, {1,0}, {1,1}, {0,1}}},
        // Right (+X)
        {{{s, -s, s}, {s, -s, -s}, {s, s, -s}, {s, s, s}}, {1, 0, 0}, {{0,0}, {1,0}, {1,1}, {0,1}}},
        // Left (-X)
        {{{-s, -s, -s}, {-s, -s, s}, {-s, s, s}, {-s, s, -s}}, {-1, 0, 0}, {{0,0}, {1,0}, {1,1}, {0,1}}},
        // Top (+Y)
        {{{-s, s, s}, {s, s, s}, {s, s, -s}, {-s, s, -s}}, {0, 1, 0}, {{0,0}, {1,0}, {1,1}, {0,1}}},
        // Bottom (-Y)
        {{{-s, -s, -s}, {s, -s, -s}, {s, -s, s}, {-s, -s, s}}, {0, -1, 0}, {{0,0}, {1,0}, {1,1}, {0,1}}}
    };

    for (int f = 0; f < 6; ++f) {
        uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

        for (int v = 0; v < 4; ++v) {
            Vertex vertex;
            vertex.Position = faces[f].positions[v];
            vertex.Normal = faces[f].normal;
            vertex.TexCoord = faces[f].texcoords[v];
            CalculateTangentBasis(vertex);
            vertices.push_back(vertex);
        }

        // 两个三角形组成一个面
        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 1);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 3);
    }

    auto meshSource = MeshSource::Create(vertices, indices);

    // 添加单个 submesh
    Submesh submesh;
    submesh.BaseVertex = 0;
    submesh.BaseIndex = 0;
    submesh.VertexCount = static_cast<uint32_t>(vertices.size());
    submesh.IndexCount = static_cast<uint32_t>(indices.size());
    submesh.MeshName = "Cube";
    meshSource->AddSubmesh(submesh);
    meshSource->CreateGPUBuffers();

    AssetHandle meshSourceHandle = AssetManager::Get().AddMemoryOnlyAsset(meshSource);
    auto staticMesh = StaticMesh::Create(meshSourceHandle);
    return AssetManager::Get().AddMemoryOnlyAsset(staticMesh);
}

AssetHandle MeshFactory::CreateSphere() {
    constexpr float radius = 0.5f;
    constexpr uint32_t segments = 32;
    constexpr uint32_t rings = 16;

    std::vector<Vertex> vertices;
    std::vector<Index> indices;

    for (uint32_t ring = 0; ring <= rings; ++ring) {
        float phi = static_cast<float>(M_PI) * static_cast<float>(ring) / static_cast<float>(rings);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (uint32_t seg = 0; seg <= segments; ++seg) {
            float theta = 2.0f * static_cast<float>(M_PI) * static_cast<float>(seg) / static_cast<float>(segments);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            Vertex vertex;
            vertex.Normal = glm::vec3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
            vertex.Position = vertex.Normal * radius;
            vertex.TexCoord = glm::vec2(
                static_cast<float>(seg) / static_cast<float>(segments),
                static_cast<float>(ring) / static_cast<float>(rings)
            );
            CalculateTangentBasis(vertex);
            vertices.push_back(vertex);
        }
    }

    for (uint32_t ring = 0; ring < rings; ++ring) {
        for (uint32_t seg = 0; seg < segments; ++seg) {
            uint32_t current = ring * (segments + 1) + seg;
            uint32_t next = current + segments + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    auto meshSource = MeshSource::Create(vertices, indices);

    Submesh submesh;
    submesh.BaseVertex = 0;
    submesh.BaseIndex = 0;
    submesh.VertexCount = static_cast<uint32_t>(vertices.size());
    submesh.IndexCount = static_cast<uint32_t>(indices.size());
    submesh.MeshName = "Sphere";
    meshSource->AddSubmesh(submesh);
    meshSource->CreateGPUBuffers();

    AssetHandle meshSourceHandle = AssetManager::Get().AddMemoryOnlyAsset(meshSource);
    auto staticMesh = StaticMesh::Create(meshSourceHandle);
    return AssetManager::Get().AddMemoryOnlyAsset(staticMesh);
}

AssetHandle MeshFactory::CreatePlane() {
    std::vector<Vertex> vertices;
    std::vector<Index> indices;

    // XZ 平面，法线朝上
    const float s = 0.5f;
    glm::vec3 normal(0.0f, 1.0f, 0.0f);
    glm::vec3 tangent(1.0f, 0.0f, 0.0f);
    glm::vec3 binormal(0.0f, 0.0f, 1.0f);

    vertices.push_back({glm::vec3(-s, 0, -s), normal, tangent, binormal, glm::vec2(0, 0)});
    vertices.push_back({glm::vec3( s, 0, -s), normal, tangent, binormal, glm::vec2(1, 0)});
    vertices.push_back({glm::vec3( s, 0,  s), normal, tangent, binormal, glm::vec2(1, 1)});
    vertices.push_back({glm::vec3(-s, 0,  s), normal, tangent, binormal, glm::vec2(0, 1)});

    indices = {0, 1, 2, 0, 2, 3};

    auto meshSource = MeshSource::Create(vertices, indices);

    Submesh submesh;
    submesh.BaseVertex = 0;
    submesh.BaseIndex = 0;
    submesh.VertexCount = static_cast<uint32_t>(vertices.size());
    submesh.IndexCount = static_cast<uint32_t>(indices.size());
    submesh.MeshName = "Plane";
    meshSource->AddSubmesh(submesh);
    meshSource->CreateGPUBuffers();

    AssetHandle meshSourceHandle = AssetManager::Get().AddMemoryOnlyAsset(meshSource);
    auto staticMesh = StaticMesh::Create(meshSourceHandle);
    return AssetManager::Get().AddMemoryOnlyAsset(staticMesh);
}

AssetHandle MeshFactory::CreateQuad() {
    // Quad 与 Plane 相同
    return CreatePlane();
}

AssetHandle MeshFactory::CreateCylinder() {
    constexpr float radius = 0.5f;
    constexpr float height = 1.0f;
    constexpr uint32_t segments = 32;

    std::vector<Vertex> vertices;
    std::vector<Index> indices;

    float halfHeight = height * 0.5f;

    // 侧面顶点
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(segments);
        float x = std::cos(angle);
        float z = std::sin(angle);
        float u = static_cast<float>(i) / static_cast<float>(segments);

        Vertex bottom, top;
        bottom.Position = glm::vec3(x * radius, -halfHeight, z * radius);
        bottom.Normal = glm::vec3(x, 0.0f, z);
        bottom.TexCoord = glm::vec2(u, 0.0f);
        CalculateTangentBasis(bottom);

        top.Position = glm::vec3(x * radius, halfHeight, z * radius);
        top.Normal = glm::vec3(x, 0.0f, z);
        top.TexCoord = glm::vec2(u, 1.0f);
        CalculateTangentBasis(top);

        vertices.push_back(bottom);
        vertices.push_back(top);
    }

    // 侧面索引
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t bottom0 = i * 2;
        uint32_t top0 = i * 2 + 1;
        uint32_t bottom1 = (i + 1) * 2;
        uint32_t top1 = (i + 1) * 2 + 1;

        indices.push_back(bottom0);
        indices.push_back(bottom1);
        indices.push_back(top0);

        indices.push_back(top0);
        indices.push_back(bottom1);
        indices.push_back(top1);
    }

    uint32_t sideVertexCount = static_cast<uint32_t>(vertices.size());

    // 顶部圆心
    Vertex topCenter;
    topCenter.Position = glm::vec3(0.0f, halfHeight, 0.0f);
    topCenter.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
    topCenter.TexCoord = glm::vec2(0.5f, 0.5f);
    topCenter.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    topCenter.Binormal = glm::vec3(0.0f, 0.0f, 1.0f);
    vertices.push_back(topCenter);
    uint32_t topCenterIdx = static_cast<uint32_t>(vertices.size()) - 1;

    // 底部圆心
    Vertex bottomCenter;
    bottomCenter.Position = glm::vec3(0.0f, -halfHeight, 0.0f);
    bottomCenter.Normal = glm::vec3(0.0f, -1.0f, 0.0f);
    bottomCenter.TexCoord = glm::vec2(0.5f, 0.5f);
    bottomCenter.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    bottomCenter.Binormal = glm::vec3(0.0f, 0.0f, -1.0f);
    vertices.push_back(bottomCenter);
    uint32_t bottomCenterIdx = static_cast<uint32_t>(vertices.size()) - 1;

    // 顶部/底部盖子顶点
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(segments);
        float x = std::cos(angle);
        float z = std::sin(angle);

        Vertex topVert;
        topVert.Position = glm::vec3(x * radius, halfHeight, z * radius);
        topVert.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        topVert.TexCoord = glm::vec2(x * 0.5f + 0.5f, z * 0.5f + 0.5f);
        topVert.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        topVert.Binormal = glm::vec3(0.0f, 0.0f, 1.0f);
        vertices.push_back(topVert);

        Vertex bottomVert;
        bottomVert.Position = glm::vec3(x * radius, -halfHeight, z * radius);
        bottomVert.Normal = glm::vec3(0.0f, -1.0f, 0.0f);
        bottomVert.TexCoord = glm::vec2(x * 0.5f + 0.5f, z * 0.5f + 0.5f);
        bottomVert.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        bottomVert.Binormal = glm::vec3(0.0f, 0.0f, -1.0f);
        vertices.push_back(bottomVert);
    }

    // 顶部盖子索引
    uint32_t capStartIdx = bottomCenterIdx + 1;
    for (uint32_t i = 0; i < segments; ++i) {
        indices.push_back(topCenterIdx);
        indices.push_back(capStartIdx + i * 2);
        indices.push_back(capStartIdx + (i + 1) * 2);
    }

    // 底部盖子索引 (反向绕序)
    for (uint32_t i = 0; i < segments; ++i) {
        indices.push_back(bottomCenterIdx);
        indices.push_back(capStartIdx + (i + 1) * 2 + 1);
        indices.push_back(capStartIdx + i * 2 + 1);
    }

    auto meshSource = MeshSource::Create(vertices, indices);

    Submesh submesh;
    submesh.BaseVertex = 0;
    submesh.BaseIndex = 0;
    submesh.VertexCount = static_cast<uint32_t>(vertices.size());
    submesh.IndexCount = static_cast<uint32_t>(indices.size());
    submesh.MeshName = "Cylinder";
    meshSource->AddSubmesh(submesh);
    meshSource->CreateGPUBuffers();

    AssetHandle meshSourceHandle = AssetManager::Get().AddMemoryOnlyAsset(meshSource);
    auto staticMesh = StaticMesh::Create(meshSourceHandle);
    return AssetManager::Get().AddMemoryOnlyAsset(staticMesh);
}

AssetHandle MeshFactory::CreateCapsule() {
    constexpr float radius = 0.5f;
    constexpr float height = 1.0f;
    constexpr uint32_t segments = 16;
    constexpr uint32_t rings = 8;

    std::vector<Vertex> vertices;
    std::vector<Index> indices;

    float halfHeight = height * 0.5f;

    // 上半球
    for (uint32_t ring = 0; ring <= rings; ++ring) {
        float phi = static_cast<float>(M_PI) * 0.5f * static_cast<float>(ring) / static_cast<float>(rings);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (uint32_t seg = 0; seg <= segments; ++seg) {
            float theta = 2.0f * static_cast<float>(M_PI) * static_cast<float>(seg) / static_cast<float>(segments);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            Vertex vertex;
            vertex.Normal = glm::vec3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
            vertex.Position = vertex.Normal * radius;
            vertex.Position.y += halfHeight;
            vertex.TexCoord = glm::vec2(
                static_cast<float>(seg) / static_cast<float>(segments),
                static_cast<float>(ring) / static_cast<float>(rings * 2 + 1)
            );
            CalculateTangentBasis(vertex);
            vertices.push_back(vertex);
        }
    }

    // 圆柱中段
    uint32_t cylinderStartIdx = static_cast<uint32_t>(vertices.size());
    for (uint32_t seg = 0; seg <= segments; ++seg) {
        float theta = 2.0f * static_cast<float>(M_PI) * static_cast<float>(seg) / static_cast<float>(segments);
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        Vertex top, bottom;
        top.Normal = bottom.Normal = glm::vec3(cosTheta, 0.0f, sinTheta);
        top.Position = glm::vec3(cosTheta * radius, halfHeight, sinTheta * radius);
        bottom.Position = glm::vec3(cosTheta * radius, -halfHeight, sinTheta * radius);
        top.TexCoord = glm::vec2(static_cast<float>(seg) / static_cast<float>(segments), 0.5f - 0.1f);
        bottom.TexCoord = glm::vec2(static_cast<float>(seg) / static_cast<float>(segments), 0.5f + 0.1f);
        CalculateTangentBasis(top);
        CalculateTangentBasis(bottom);

        vertices.push_back(top);
        vertices.push_back(bottom);
    }

    // 下半球
    uint32_t bottomHemiStartIdx = static_cast<uint32_t>(vertices.size());
    for (uint32_t ring = 0; ring <= rings; ++ring) {
        float phi = static_cast<float>(M_PI) * 0.5f + static_cast<float>(M_PI) * 0.5f * static_cast<float>(ring) / static_cast<float>(rings);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (uint32_t seg = 0; seg <= segments; ++seg) {
            float theta = 2.0f * static_cast<float>(M_PI) * static_cast<float>(seg) / static_cast<float>(segments);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            Vertex vertex;
            vertex.Normal = glm::vec3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
            vertex.Position = vertex.Normal * radius;
            vertex.Position.y -= halfHeight;
            vertex.TexCoord = glm::vec2(
                static_cast<float>(seg) / static_cast<float>(segments),
                0.5f + 0.5f * static_cast<float>(ring) / static_cast<float>(rings)
            );
            CalculateTangentBasis(vertex);
            vertices.push_back(vertex);
        }
    }

    // 上半球索引
    for (uint32_t ring = 0; ring < rings; ++ring) {
        for (uint32_t seg = 0; seg < segments; ++seg) {
            uint32_t current = ring * (segments + 1) + seg;
            uint32_t next = current + segments + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    // 圆柱索引
    for (uint32_t seg = 0; seg < segments; ++seg) {
        uint32_t top0 = cylinderStartIdx + seg * 2;
        uint32_t bottom0 = top0 + 1;
        uint32_t top1 = cylinderStartIdx + (seg + 1) * 2;
        uint32_t bottom1 = top1 + 1;

        indices.push_back(top0);
        indices.push_back(bottom0);
        indices.push_back(top1);

        indices.push_back(top1);
        indices.push_back(bottom0);
        indices.push_back(bottom1);
    }

    // 下半球索引
    for (uint32_t ring = 0; ring < rings; ++ring) {
        for (uint32_t seg = 0; seg < segments; ++seg) {
            uint32_t current = bottomHemiStartIdx + ring * (segments + 1) + seg;
            uint32_t next = current + segments + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    auto meshSource = MeshSource::Create(vertices, indices);

    Submesh submesh;
    submesh.BaseVertex = 0;
    submesh.BaseIndex = 0;
    submesh.VertexCount = static_cast<uint32_t>(vertices.size());
    submesh.IndexCount = static_cast<uint32_t>(indices.size());
    submesh.MeshName = "Capsule";
    meshSource->AddSubmesh(submesh);
    meshSource->CreateGPUBuffers();

    AssetHandle meshSourceHandle = AssetManager::Get().AddMemoryOnlyAsset(meshSource);
    auto staticMesh = StaticMesh::Create(meshSourceHandle);
    return AssetManager::Get().AddMemoryOnlyAsset(staticMesh);
}

} // namespace GLRenderer
