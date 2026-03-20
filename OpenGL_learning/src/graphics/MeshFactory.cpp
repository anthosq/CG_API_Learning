#include "MeshFactory.h"
#include "PrimitiveData.h"
#include "asset/AssetManager.h"
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace GLRenderer {

AssetHandle MeshFactory::CreateCube() {
    auto mesh = Ref<StaticMesh>::Create();
    mesh->CreateFromPrimitiveVertices(
        PrimitiveData::CubeVertices,
        PrimitiveData::CubeVertexCount);
    mesh->m_Type = PrimitiveType::Cube;
    return AssetManager::Get().AddMemoryOnlyAsset(mesh);
}

AssetHandle MeshFactory::CreateSphere() {
    constexpr float radius = 0.5f;
    constexpr uint32_t segments = 32;
    constexpr uint32_t rings = 16;

    std::vector<PrimitiveVertex> vertices;
    std::vector<uint32_t> indices;

    for (uint32_t ring = 0; ring <= rings; ++ring) {
        float phi = static_cast<float>(M_PI) * static_cast<float>(ring) / static_cast<float>(rings);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (uint32_t seg = 0; seg <= segments; ++seg) {
            float theta = 2.0f * static_cast<float>(M_PI) * static_cast<float>(seg) / static_cast<float>(segments);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            PrimitiveVertex vertex;
            vertex.Normal = glm::vec3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
            vertex.Position = vertex.Normal * radius;
            vertex.TexCoords = glm::vec2(
                static_cast<float>(seg) / static_cast<float>(segments),
                static_cast<float>(ring) / static_cast<float>(rings)
            );
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

    auto mesh = Ref<StaticMesh>::Create();
    mesh->CreateFromPrimitiveVertices(
        vertices.data(), static_cast<uint32_t>(vertices.size()),
        indices.data(), static_cast<uint32_t>(indices.size()));
    mesh->m_Type = PrimitiveType::Sphere;
    return AssetManager::Get().AddMemoryOnlyAsset(mesh);
}

AssetHandle MeshFactory::CreatePlane() {
    auto mesh = Ref<StaticMesh>::Create();
    mesh->CreateFromPrimitiveVertices(
        PrimitiveData::QuadVertices, PrimitiveData::QuadVertexCount,
        PrimitiveData::QuadIndices, PrimitiveData::QuadIndexCount);
    mesh->m_Type = PrimitiveType::Plane;
    return AssetManager::Get().AddMemoryOnlyAsset(mesh);
}

AssetHandle MeshFactory::CreateQuad() {
    auto mesh = Ref<StaticMesh>::Create();
    mesh->CreateFromPrimitiveVertices(
        PrimitiveData::QuadVertices, PrimitiveData::QuadVertexCount,
        PrimitiveData::QuadIndices, PrimitiveData::QuadIndexCount);
    mesh->m_Type = PrimitiveType::Quad;
    return AssetManager::Get().AddMemoryOnlyAsset(mesh);
}

AssetHandle MeshFactory::CreateCylinder() {
    constexpr float radius = 0.5f;
    constexpr float height = 1.0f;
    constexpr uint32_t segments = 32;

    std::vector<PrimitiveVertex> vertices;
    std::vector<uint32_t> indices;

    float halfHeight = height * 0.5f;

    // 侧面顶点
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(segments);
        float x = std::cos(angle);
        float z = std::sin(angle);
        float u = static_cast<float>(i) / static_cast<float>(segments);

        PrimitiveVertex bottom;
        bottom.Position = glm::vec3(x * radius, -halfHeight, z * radius);
        bottom.Normal = glm::vec3(x, 0.0f, z);
        bottom.TexCoords = glm::vec2(u, 0.0f);
        vertices.push_back(bottom);

        PrimitiveVertex top;
        top.Position = glm::vec3(x * radius, halfHeight, z * radius);
        top.Normal = glm::vec3(x, 0.0f, z);
        top.TexCoords = glm::vec2(u, 1.0f);
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
    PrimitiveVertex topCenter;
    topCenter.Position = glm::vec3(0.0f, halfHeight, 0.0f);
    topCenter.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
    topCenter.TexCoords = glm::vec2(0.5f, 0.5f);
    vertices.push_back(topCenter);
    uint32_t topCenterIdx = static_cast<uint32_t>(vertices.size()) - 1;

    // 底部圆心
    PrimitiveVertex bottomCenter;
    bottomCenter.Position = glm::vec3(0.0f, -halfHeight, 0.0f);
    bottomCenter.Normal = glm::vec3(0.0f, -1.0f, 0.0f);
    bottomCenter.TexCoords = glm::vec2(0.5f, 0.5f);
    vertices.push_back(bottomCenter);
    uint32_t bottomCenterIdx = static_cast<uint32_t>(vertices.size()) - 1;

    // 顶部/底部盖子顶点
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(segments);
        float x = std::cos(angle);
        float z = std::sin(angle);

        PrimitiveVertex topVert;
        topVert.Position = glm::vec3(x * radius, halfHeight, z * radius);
        topVert.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        topVert.TexCoords = glm::vec2(x * 0.5f + 0.5f, z * 0.5f + 0.5f);
        vertices.push_back(topVert);

        PrimitiveVertex bottomVert;
        bottomVert.Position = glm::vec3(x * radius, -halfHeight, z * radius);
        bottomVert.Normal = glm::vec3(0.0f, -1.0f, 0.0f);
        bottomVert.TexCoords = glm::vec2(x * 0.5f + 0.5f, z * 0.5f + 0.5f);
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

    auto mesh = Ref<StaticMesh>::Create();
    mesh->CreateFromPrimitiveVertices(
        vertices.data(), static_cast<uint32_t>(vertices.size()),
        indices.data(), static_cast<uint32_t>(indices.size()));
    mesh->m_Type = PrimitiveType::Cylinder;
    return AssetManager::Get().AddMemoryOnlyAsset(mesh);
}

AssetHandle MeshFactory::CreateCapsule() {
    constexpr float radius = 0.5f;
    constexpr float height = 1.0f;
    constexpr uint32_t segments = 16;
    constexpr uint32_t rings = 8;

    std::vector<PrimitiveVertex> vertices;
    std::vector<uint32_t> indices;

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

            PrimitiveVertex vertex;
            vertex.Normal = glm::vec3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
            vertex.Position = vertex.Normal * radius;
            vertex.Position.y += halfHeight;
            vertex.TexCoords = glm::vec2(
                static_cast<float>(seg) / static_cast<float>(segments),
                static_cast<float>(ring) / static_cast<float>(rings * 2 + 1)
            );
            vertices.push_back(vertex);
        }
    }

    // 圆柱中段
    uint32_t cylinderStartIdx = static_cast<uint32_t>(vertices.size());
    for (uint32_t seg = 0; seg <= segments; ++seg) {
        float theta = 2.0f * static_cast<float>(M_PI) * static_cast<float>(seg) / static_cast<float>(segments);
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        PrimitiveVertex top, bottom;
        top.Normal = bottom.Normal = glm::vec3(cosTheta, 0.0f, sinTheta);
        top.Position = glm::vec3(cosTheta * radius, halfHeight, sinTheta * radius);
        bottom.Position = glm::vec3(cosTheta * radius, -halfHeight, sinTheta * radius);
        top.TexCoords = glm::vec2(static_cast<float>(seg) / static_cast<float>(segments), 0.5f - 0.1f);
        bottom.TexCoords = glm::vec2(static_cast<float>(seg) / static_cast<float>(segments), 0.5f + 0.1f);

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

            PrimitiveVertex vertex;
            vertex.Normal = glm::vec3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
            vertex.Position = vertex.Normal * radius;
            vertex.Position.y -= halfHeight;
            vertex.TexCoords = glm::vec2(
                static_cast<float>(seg) / static_cast<float>(segments),
                0.5f + 0.5f * static_cast<float>(ring) / static_cast<float>(rings)
            );
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

    auto mesh = Ref<StaticMesh>::Create();
    mesh->CreateFromPrimitiveVertices(
        vertices.data(), static_cast<uint32_t>(vertices.size()),
        indices.data(), static_cast<uint32_t>(indices.size()));
    mesh->m_Type = PrimitiveType::Capsule;
    return AssetManager::Get().AddMemoryOnlyAsset(mesh);
}

} // namespace GLRenderer
