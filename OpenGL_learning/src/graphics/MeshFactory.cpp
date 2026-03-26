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
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(v.Normal, up)) > 0.99f) {
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    v.Tangent = glm::normalize(glm::cross(up, v.Normal));
    v.Binormal = glm::normalize(glm::cross(v.Normal, v.Tangent));
}

// 注意: MeshSource::Create(vertices, indices) 构造函数已自动创建 submesh 并上传 GPU 缓冲，
// 不要在此之后再调用 AddSubmesh 或 CreateGPUBuffers，否则会双重初始化导致顶点数据错乱。

AssetHandle MeshFactory::CreateCube() {
    auto& am = AssetManager::Get();
    AssetHandle existing = am.FindAssetByKey("__prim:cube");
    if (existing.IsValid() && am.IsAssetLoaded(existing))
        return existing;

    std::vector<Vertex> vertices;
    std::vector<Index> indices;

    const float s = 0.5f;

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
            vertex.Normal   = faces[f].normal;
            vertex.TexCoord = faces[f].texcoords[v];
            CalculateTangentBasis(vertex);
            vertices.push_back(vertex);
        }
        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 1);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 3);
    }

    auto meshSource = MeshSource::Create(vertices, indices);
    AssetHandle handle = am.AddMemoryOnlyAsset(meshSource);
    am.RegisterKey("__prim:cube", handle);
    return handle;
}

AssetHandle MeshFactory::CreateSphere() {
    auto& am = AssetManager::Get();
    AssetHandle existing = am.FindAssetByKey("__prim:sphere");
    if (existing.IsValid() && am.IsAssetLoaded(existing))
        return existing;

    constexpr float    radius   = 0.5f;
    constexpr uint32_t segments = 32;
    constexpr uint32_t rings    = 16;

    std::vector<Vertex> vertices;
    std::vector<Index>  indices;

    for (uint32_t ring = 0; ring <= rings; ++ring) {
        float phi    = static_cast<float>(M_PI) * static_cast<float>(ring) / static_cast<float>(rings);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (uint32_t seg = 0; seg <= segments; ++seg) {
            float theta    = 2.0f * static_cast<float>(M_PI) * static_cast<float>(seg) / static_cast<float>(segments);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            Vertex vertex;
            vertex.Normal   = glm::vec3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
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
            uint32_t next    = current + segments + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    auto meshSource = MeshSource::Create(vertices, indices);
    AssetHandle handle = am.AddMemoryOnlyAsset(meshSource);
    am.RegisterKey("__prim:sphere", handle);
    return handle;
}

AssetHandle MeshFactory::CreatePlane() {
    auto& am = AssetManager::Get();
    AssetHandle existing = am.FindAssetByKey("__prim:plane");
    if (existing.IsValid() && am.IsAssetLoaded(existing))
        return existing;

    const float    s       = 0.5f;
    glm::vec3 normal  (0.0f, 1.0f, 0.0f);
    glm::vec3 tangent (1.0f, 0.0f, 0.0f);
    glm::vec3 binormal(0.0f, 0.0f, 1.0f);

    std::vector<Vertex> vertices = {
        {glm::vec3(-s, 0, -s), normal, tangent, binormal, glm::vec2(0, 0)},
        {glm::vec3( s, 0, -s), normal, tangent, binormal, glm::vec2(1, 0)},
        {glm::vec3( s, 0,  s), normal, tangent, binormal, glm::vec2(1, 1)},
        {glm::vec3(-s, 0,  s), normal, tangent, binormal, glm::vec2(0, 1)},
    };
    std::vector<Index> indices = {0, 1, 2, 0, 2, 3};

    auto meshSource = MeshSource::Create(vertices, indices);
    AssetHandle handle = am.AddMemoryOnlyAsset(meshSource);
    am.RegisterKey("__prim:plane", handle);
    return handle;
}

AssetHandle MeshFactory::CreateQuad() {
    return CreatePlane();
}

AssetHandle MeshFactory::CreateCylinder() {
    auto& am = AssetManager::Get();
    AssetHandle existing = am.FindAssetByKey("__prim:cylinder");
    if (existing.IsValid() && am.IsAssetLoaded(existing))
        return existing;

    constexpr float    radius   = 0.5f;
    constexpr float    height   = 1.0f;
    constexpr uint32_t segments = 32;
    const float halfHeight = height * 0.5f;

    std::vector<Vertex> vertices;
    std::vector<Index>  indices;

    // 侧面顶点（底部在偶数索引，顶部在奇数索引）
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(segments);
        float x = std::cos(angle);
        float z = std::sin(angle);
        float u = static_cast<float>(i) / static_cast<float>(segments);

        Vertex bottom, top;
        bottom.Position = glm::vec3(x * radius, -halfHeight, z * radius);
        bottom.Normal   = glm::vec3(x, 0.0f, z);
        bottom.TexCoord = glm::vec2(u, 0.0f);
        CalculateTangentBasis(bottom);

        top.Position = glm::vec3(x * radius, halfHeight, z * radius);
        top.Normal   = glm::vec3(x, 0.0f, z);
        top.TexCoord = glm::vec2(u, 1.0f);
        CalculateTangentBasis(top);

        vertices.push_back(bottom);
        vertices.push_back(top);
    }

    // 侧面索引
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t b0 = i * 2,      t0 = i * 2 + 1;
        uint32_t b1 = (i+1) * 2,  t1 = (i+1) * 2 + 1;
        indices.push_back(b0); indices.push_back(b1); indices.push_back(t0);
        indices.push_back(t0); indices.push_back(b1); indices.push_back(t1);
    }

    // 顶部圆心
    uint32_t topCenterIdx = static_cast<uint32_t>(vertices.size());
    {
        Vertex v;
        v.Position = glm::vec3(0.0f, halfHeight, 0.0f);
        v.Normal   = glm::vec3(0.0f, 1.0f, 0.0f);
        v.TexCoord = glm::vec2(0.5f, 0.5f);
        v.Tangent  = glm::vec3(1.0f, 0.0f, 0.0f);
        v.Binormal = glm::vec3(0.0f, 0.0f, 1.0f);
        vertices.push_back(v);
    }

    // 底部圆心
    uint32_t bottomCenterIdx = static_cast<uint32_t>(vertices.size());
    {
        Vertex v;
        v.Position = glm::vec3(0.0f, -halfHeight, 0.0f);
        v.Normal   = glm::vec3(0.0f, -1.0f, 0.0f);
        v.TexCoord = glm::vec2(0.5f, 0.5f);
        v.Tangent  = glm::vec3(1.0f, 0.0f, 0.0f);
        v.Binormal = glm::vec3(0.0f, 0.0f, -1.0f);
        vertices.push_back(v);
    }

    // 盖子顶点（顶部偶数索引，底部奇数索引）
    uint32_t capStartIdx = static_cast<uint32_t>(vertices.size());
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(segments);
        float x = std::cos(angle);
        float z = std::sin(angle);

        Vertex topVert;
        topVert.Position = glm::vec3(x * radius, halfHeight, z * radius);
        topVert.Normal   = glm::vec3(0.0f, 1.0f, 0.0f);
        topVert.TexCoord = glm::vec2(x * 0.5f + 0.5f, z * 0.5f + 0.5f);
        topVert.Tangent  = glm::vec3(1.0f, 0.0f, 0.0f);
        topVert.Binormal = glm::vec3(0.0f, 0.0f, 1.0f);
        vertices.push_back(topVert);

        Vertex bottomVert;
        bottomVert.Position = glm::vec3(x * radius, -halfHeight, z * radius);
        bottomVert.Normal   = glm::vec3(0.0f, -1.0f, 0.0f);
        bottomVert.TexCoord = glm::vec2(x * 0.5f + 0.5f, z * 0.5f + 0.5f);
        bottomVert.Tangent  = glm::vec3(1.0f, 0.0f, 0.0f);
        bottomVert.Binormal = glm::vec3(0.0f, 0.0f, -1.0f);
        vertices.push_back(bottomVert);
    }

    // 顶部盖子索引
    for (uint32_t i = 0; i < segments; ++i) {
        indices.push_back(topCenterIdx);
        indices.push_back(capStartIdx + i * 2);
        indices.push_back(capStartIdx + (i + 1) * 2);
    }

    // 底部盖子索引（反向绕序）
    for (uint32_t i = 0; i < segments; ++i) {
        indices.push_back(bottomCenterIdx);
        indices.push_back(capStartIdx + (i + 1) * 2 + 1);
        indices.push_back(capStartIdx + i * 2 + 1);
    }

    auto meshSource = MeshSource::Create(vertices, indices);
    AssetHandle handle = am.AddMemoryOnlyAsset(meshSource);
    am.RegisterKey("__prim:cylinder", handle);
    return handle;
}

AssetHandle MeshFactory::CreateCapsule() {
    auto& am = AssetManager::Get();
    AssetHandle existing = am.FindAssetByKey("__prim:capsule");
    if (existing.IsValid() && am.IsAssetLoaded(existing))
        return existing;

    constexpr float    radius   = 0.5f;
    constexpr float    height   = 1.0f;
    constexpr uint32_t segments = 16;
    constexpr uint32_t rings    = 8;
    const float halfHeight = height * 0.5f;

    std::vector<Vertex> vertices;
    std::vector<Index>  indices;

    // 上半球
    for (uint32_t ring = 0; ring <= rings; ++ring) {
        float phi    = static_cast<float>(M_PI) * 0.5f * static_cast<float>(ring) / static_cast<float>(rings);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (uint32_t seg = 0; seg <= segments; ++seg) {
            float theta    = 2.0f * static_cast<float>(M_PI) * static_cast<float>(seg) / static_cast<float>(segments);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            Vertex vertex;
            vertex.Normal   = glm::vec3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
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

    // 圆柱中段（顶部偶数索引，底部奇数索引）
    uint32_t cylinderStartIdx = static_cast<uint32_t>(vertices.size());
    for (uint32_t seg = 0; seg <= segments; ++seg) {
        float theta    = 2.0f * static_cast<float>(M_PI) * static_cast<float>(seg) / static_cast<float>(segments);
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        Vertex top, bottom;
        top.Normal = bottom.Normal = glm::vec3(cosTheta, 0.0f, sinTheta);
        top.Position    = glm::vec3(cosTheta * radius,  halfHeight, sinTheta * radius);
        bottom.Position = glm::vec3(cosTheta * radius, -halfHeight, sinTheta * radius);
        top.TexCoord    = glm::vec2(static_cast<float>(seg) / static_cast<float>(segments), 0.5f - 0.1f);
        bottom.TexCoord = glm::vec2(static_cast<float>(seg) / static_cast<float>(segments), 0.5f + 0.1f);
        CalculateTangentBasis(top);
        CalculateTangentBasis(bottom);

        vertices.push_back(top);
        vertices.push_back(bottom);
    }

    // 下半球
    uint32_t bottomHemiStartIdx = static_cast<uint32_t>(vertices.size());
    for (uint32_t ring = 0; ring <= rings; ++ring) {
        float phi    = static_cast<float>(M_PI) * 0.5f + static_cast<float>(M_PI) * 0.5f * static_cast<float>(ring) / static_cast<float>(rings);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (uint32_t seg = 0; seg <= segments; ++seg) {
            float theta    = 2.0f * static_cast<float>(M_PI) * static_cast<float>(seg) / static_cast<float>(segments);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            Vertex vertex;
            vertex.Normal   = glm::vec3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
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
            uint32_t next    = current + segments + 1;
            indices.push_back(current);   indices.push_back(next);       indices.push_back(current + 1);
            indices.push_back(current+1); indices.push_back(next);       indices.push_back(next + 1);
        }
    }

    // 圆柱索引
    for (uint32_t seg = 0; seg < segments; ++seg) {
        uint32_t t0 = cylinderStartIdx + seg * 2;
        uint32_t b0 = t0 + 1;
        uint32_t t1 = cylinderStartIdx + (seg + 1) * 2;
        uint32_t b1 = t1 + 1;
        indices.push_back(t0); indices.push_back(b0); indices.push_back(t1);
        indices.push_back(t1); indices.push_back(b0); indices.push_back(b1);
    }

    // 下半球索引
    for (uint32_t ring = 0; ring < rings; ++ring) {
        for (uint32_t seg = 0; seg < segments; ++seg) {
            uint32_t current = bottomHemiStartIdx + ring * (segments + 1) + seg;
            uint32_t next    = current + segments + 1;
            indices.push_back(current);   indices.push_back(next);       indices.push_back(current + 1);
            indices.push_back(current+1); indices.push_back(next);       indices.push_back(next + 1);
        }
    }

    auto meshSource = MeshSource::Create(vertices, indices);
    AssetHandle handle = am.AddMemoryOnlyAsset(meshSource);
    am.RegisterKey("__prim:capsule", handle);
    return handle;
}

} // namespace GLRenderer
