#include "MeshFactory.h"
#include "PrimitiveData.h"
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace GLRenderer {

// 单例

MeshFactory& MeshFactory::Get() {
    static MeshFactory instance;
    return instance;
}

// 初始化/关闭

void MeshFactory::Initialize() {
    if (m_Initialized) return;

    // 创建共享的基础几何体
    m_Cube = CreateCube(1.0f);
    m_Cube->m_Type = PrimitiveType::Cube;

    m_Sphere = CreateSphere(0.5f, 32, 16);
    m_Sphere->m_Type = PrimitiveType::Sphere;

    m_Plane = CreatePlane(1.0f, 1.0f, 1);
    m_Plane->m_Type = PrimitiveType::Plane;

    m_Quad = CreateFromVertices(
        PrimitiveData::QuadVertices, PrimitiveData::QuadVertexCount,
        PrimitiveData::QuadIndices, PrimitiveData::QuadIndexCount);
    m_Quad->m_Type = PrimitiveType::Quad;

    m_Cylinder = CreateCylinder(0.5f, 1.0f, 32);
    m_Cylinder->m_Type = PrimitiveType::Cylinder;

    // 创建共享的线框几何体
    m_CubeWireframe = CreateCubeWireframe(1.0f);
    m_CubeWireframe->m_Type = PrimitiveType::Cube;

    m_SphereWireframe = CreateSphereWireframe(0.5f, 16);
    m_SphereWireframe->m_Type = PrimitiveType::Sphere;

    m_Initialized = true;
}

void MeshFactory::Shutdown() {
    m_Cube.reset();
    m_Sphere.reset();
    m_Plane.reset();
    m_Quad.reset();
    m_Cylinder.reset();
    m_CubeWireframe.reset();
    m_SphereWireframe.reset();
    m_Initialized = false;
}

// ===========================================================================
// 共享几何体访问
// ===========================================================================

StaticMesh* MeshFactory::GetCube() {
    if (!m_Initialized) Initialize();
    return m_Cube.get();
}

StaticMesh* MeshFactory::GetSphere() {
    if (!m_Initialized) Initialize();
    return m_Sphere.get();
}

StaticMesh* MeshFactory::GetPlane() {
    if (!m_Initialized) Initialize();
    return m_Plane.get();
}

StaticMesh* MeshFactory::GetQuad() {
    if (!m_Initialized) Initialize();
    return m_Quad.get();
}

StaticMesh* MeshFactory::GetCylinder() {
    if (!m_Initialized) Initialize();
    return m_Cylinder.get();
}

StaticMesh* MeshFactory::GetCubeWireframe() {
    if (!m_Initialized) Initialize();
    return m_CubeWireframe.get();
}

StaticMesh* MeshFactory::GetSphereWireframe() {
    if (!m_Initialized) Initialize();
    return m_SphereWireframe.get();
}

// ===========================================================================
// 创建立方体
// ===========================================================================

std::unique_ptr<StaticMesh> MeshFactory::CreateCube(float size) {
    // 使用预定义数据，缩放顶点
    std::vector<PrimitiveVertex> vertices;
    vertices.reserve(PrimitiveData::CubeVertexCount);

    for (uint32_t i = 0; i < PrimitiveData::CubeVertexCount; ++i) {
        PrimitiveVertex v = PrimitiveData::CubeVertices[i];
        v.Position *= size;
        vertices.push_back(v);
    }

    auto mesh = std::make_unique<StaticMesh>();
    mesh->CreateFromPrimitiveVertices(vertices.data(), static_cast<uint32_t>(vertices.size()));
    mesh->m_Type = PrimitiveType::Cube;
    return mesh;
}

std::unique_ptr<StaticMesh> MeshFactory::CreateBox(const glm::vec3& halfExtents) {
    std::vector<PrimitiveVertex> vertices;
    vertices.reserve(PrimitiveData::CubeVertexCount);

    glm::vec3 scale = halfExtents * 2.0f;  // 从 -0.5 到 0.5 缩放

    for (uint32_t i = 0; i < PrimitiveData::CubeVertexCount; ++i) {
        PrimitiveVertex v = PrimitiveData::CubeVertices[i];
        v.Position *= scale;
        vertices.push_back(v);
    }

    auto mesh = std::make_unique<StaticMesh>();
    mesh->CreateFromPrimitiveVertices(vertices.data(), static_cast<uint32_t>(vertices.size()));
    mesh->m_Type = PrimitiveType::Cube;
    return mesh;
}

// ===========================================================================
// 创建球体 (UV Sphere)
// ===========================================================================

std::unique_ptr<StaticMesh> MeshFactory::CreateSphere(
    float radius, uint32_t segments, uint32_t rings)
{
    std::vector<PrimitiveVertex> vertices;
    std::vector<uint32_t> indices;

    // 生成顶点
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

    // 生成索引
    for (uint32_t ring = 0; ring < rings; ++ring) {
        for (uint32_t seg = 0; seg < segments; ++seg) {
            uint32_t current = ring * (segments + 1) + seg;
            uint32_t next = current + segments + 1;

            // 两个三角形组成一个四边形
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    auto mesh = std::make_unique<StaticMesh>();
    mesh->CreateFromPrimitiveVertices(
        vertices.data(), static_cast<uint32_t>(vertices.size()),
        indices.data(), static_cast<uint32_t>(indices.size()));
    mesh->m_Type = PrimitiveType::Sphere;
    return mesh;
}

// ===========================================================================
// 创建平面
// ===========================================================================

std::unique_ptr<StaticMesh> MeshFactory::CreatePlane(
    float width, float depth, uint32_t subdivisions)
{
    std::vector<PrimitiveVertex> vertices;
    std::vector<uint32_t> indices;

    float halfW = width * 0.5f;
    float halfD = depth * 0.5f;
    uint32_t vertsPerSide = subdivisions + 1;

    // 生成顶点
    for (uint32_t z = 0; z <= subdivisions; ++z) {
        for (uint32_t x = 0; x <= subdivisions; ++x) {
            float u = static_cast<float>(x) / static_cast<float>(subdivisions);
            float v = static_cast<float>(z) / static_cast<float>(subdivisions);

            PrimitiveVertex vertex;
            vertex.Position = glm::vec3(
                -halfW + u * width,
                0.0f,
                -halfD + v * depth
            );
            vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
            vertex.TexCoords = glm::vec2(u, v);
            vertices.push_back(vertex);
        }
    }

    // 生成索引
    for (uint32_t z = 0; z < subdivisions; ++z) {
        for (uint32_t x = 0; x < subdivisions; ++x) {
            uint32_t topLeft = z * vertsPerSide + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (z + 1) * vertsPerSide + x;
            uint32_t bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    auto mesh = std::make_unique<StaticMesh>();
    mesh->CreateFromPrimitiveVertices(
        vertices.data(), static_cast<uint32_t>(vertices.size()),
        indices.data(), static_cast<uint32_t>(indices.size()));
    mesh->m_Type = PrimitiveType::Plane;
    return mesh;
}

// ===========================================================================
// 创建圆柱体
// ===========================================================================

std::unique_ptr<StaticMesh> MeshFactory::CreateCylinder(
    float radius, float height, uint32_t segments)
{
    std::vector<PrimitiveVertex> vertices;
    std::vector<uint32_t> indices;

    float halfHeight = height * 0.5f;

    // 侧面顶点
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(segments);
        float x = std::cos(angle);
        float z = std::sin(angle);
        float u = static_cast<float>(i) / static_cast<float>(segments);

        // 底部顶点
        PrimitiveVertex bottom;
        bottom.Position = glm::vec3(x * radius, -halfHeight, z * radius);
        bottom.Normal = glm::vec3(x, 0.0f, z);
        bottom.TexCoords = glm::vec2(u, 0.0f);
        vertices.push_back(bottom);

        // 顶部顶点
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

        // 顶部盖子
        PrimitiveVertex topVert;
        topVert.Position = glm::vec3(x * radius, halfHeight, z * radius);
        topVert.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        topVert.TexCoords = glm::vec2(x * 0.5f + 0.5f, z * 0.5f + 0.5f);
        vertices.push_back(topVert);

        // 底部盖子
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

    auto mesh = std::make_unique<StaticMesh>();
    mesh->CreateFromPrimitiveVertices(
        vertices.data(), static_cast<uint32_t>(vertices.size()),
        indices.data(), static_cast<uint32_t>(indices.size()));
    mesh->m_Type = PrimitiveType::Cylinder;
    return mesh;
}

// 创建线框几何体

std::unique_ptr<StaticMesh> MeshFactory::CreateCubeWireframe(float size) {
    std::vector<WireframeVertex> vertices;
    vertices.reserve(PrimitiveData::CubeWireframeVertexCount);

    for (uint32_t i = 0; i < PrimitiveData::CubeWireframeVertexCount; ++i) {
        WireframeVertex v = PrimitiveData::CubeWireframeVertices[i];
        v.Position *= size;
        vertices.push_back(v);
    }

    auto mesh = std::make_unique<StaticMesh>();
    mesh->CreateFromWireframeVertices(
        vertices.data(), static_cast<uint32_t>(vertices.size()),
        PrimitiveData::CubeWireframeIndices, PrimitiveData::CubeWireframeIndexCount);
    mesh->m_Type = PrimitiveType::Cube;
    return mesh;
}

std::unique_ptr<StaticMesh> MeshFactory::CreateBoxWireframe(const glm::vec3& halfExtents) {
    std::vector<WireframeVertex> vertices;
    vertices.reserve(PrimitiveData::CubeWireframeVertexCount);

    glm::vec3 scale = halfExtents * 2.0f;

    for (uint32_t i = 0; i < PrimitiveData::CubeWireframeVertexCount; ++i) {
        WireframeVertex v = PrimitiveData::CubeWireframeVertices[i];
        v.Position *= scale;
        vertices.push_back(v);
    }

    auto mesh = std::make_unique<StaticMesh>();
    mesh->CreateFromWireframeVertices(
        vertices.data(), static_cast<uint32_t>(vertices.size()),
        PrimitiveData::CubeWireframeIndices, PrimitiveData::CubeWireframeIndexCount);
    mesh->m_Type = PrimitiveType::Cube;
    return mesh;
}

std::unique_ptr<StaticMesh> MeshFactory::CreateSphereWireframe(
    float radius, uint32_t segments)
{
    std::vector<WireframeVertex> vertices;
    std::vector<uint32_t> indices;

    // 三个圆环 (XY, XZ, YZ 平面)
    auto addCircle = [&](int axis1, int axis2, int fixedAxis) {
        uint32_t startIdx = static_cast<uint32_t>(vertices.size());
        for (uint32_t i = 0; i <= segments; ++i) {
            float angle = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(segments);
            glm::vec3 pos(0.0f);
            pos[axis1] = std::cos(angle) * radius;
            pos[axis2] = std::sin(angle) * radius;
            pos[fixedAxis] = 0.0f;
            vertices.push_back(WireframeVertex(pos));

            if (i > 0) {
                indices.push_back(startIdx + i - 1);
                indices.push_back(startIdx + i);
            }
        }
    };

    addCircle(0, 1, 2);  // XY 平面
    addCircle(0, 2, 1);  // XZ 平面
    addCircle(1, 2, 0);  // YZ 平面

    auto mesh = std::make_unique<StaticMesh>();
    mesh->CreateFromWireframeVertices(
        vertices.data(), static_cast<uint32_t>(vertices.size()),
        indices.data(), static_cast<uint32_t>(indices.size()));
    mesh->m_Type = PrimitiveType::Sphere;
    return mesh;
}

// 从顶点数据创建
std::unique_ptr<StaticMesh> MeshFactory::CreateFromVertices(
    const PrimitiveVertex* vertices, uint32_t vertexCount,
    const uint32_t* indices, uint32_t indexCount)
{
    auto mesh = std::make_unique<StaticMesh>();
    mesh->CreateFromPrimitiveVertices(vertices, vertexCount, indices, indexCount);
    return mesh;
}

std::unique_ptr<StaticMesh> MeshFactory::CreateFromWireframeVertices(
    const WireframeVertex* vertices, uint32_t vertexCount,
    const uint32_t* indices, uint32_t indexCount)
{
    auto mesh = std::make_unique<StaticMesh>();
    mesh->CreateFromWireframeVertices(vertices, vertexCount, indices, indexCount);
    return mesh;
}

} // namespace GLRenderer
