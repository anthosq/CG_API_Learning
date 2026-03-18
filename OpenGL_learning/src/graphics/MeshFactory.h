#pragma once

// MeshFactory - 几何体工厂
//
// 用于:
// - 创建基础几何体 (立方体、球体、平面等)
// - 共享常用几何体 (Flyweight 模式)
// - 创建碰撞体线框
//
// 设计原则:
// - 单例模式，全局访问
// - 共享几何体使用原始指针返回，不转移所有权
// - 自定义几何体返回 unique_ptr，调用者拥有

#include "StaticMesh.h"
#include <memory>
#include <glm/glm.hpp>

namespace GLRenderer {

class MeshFactory {
public:
    // 单例访问
    static MeshFactory& Get();

    // 禁用拷贝
    MeshFactory(const MeshFactory&) = delete;
    MeshFactory& operator=(const MeshFactory&) = delete;

    // 初始化/关闭 (需要在 OpenGL 上下文有效时调用)
    void Initialize();
    void Shutdown();
    bool IsInitialized() const { return m_Initialized; }

    // 共享基础几何体 (缓存，不要删除返回的指针)

    // 获取单位立方体 (1x1x1，中心在原点)
    StaticMesh* GetCube();

    // 获取单位球体 (半径 0.5)
    StaticMesh* GetSphere();

    // 获取单位平面 (XZ 平面，1x1，法线朝上)
    StaticMesh* GetPlane();

    // 获取单位四边形 (XZ 平面)
    StaticMesh* GetQuad();

    // 获取单位圆柱体 (半径 0.5, 高度 1)
    StaticMesh* GetCylinder();

    // 共享线框几何体 (用于碰撞/调试可视化)

    // 获取立方体线框
    StaticMesh* GetCubeWireframe();

    // 获取球体线框
    StaticMesh* GetSphereWireframe();

    // 创建新实例 (调用者拥有返回的指针)

    // 创建指定尺寸的立方体
    std::unique_ptr<StaticMesh> CreateCube(float size = 1.0f);

    // 创建指定尺寸的立方体 (非均匀缩放)
    std::unique_ptr<StaticMesh> CreateBox(const glm::vec3& halfExtents);

    // 创建球体
    // @param radius 球体半径
    // @param segments 经度分段数 (水平方向)
    // @param rings 纬度分段数 (垂直方向)
    std::unique_ptr<StaticMesh> CreateSphere(
        float radius = 0.5f,
        uint32_t segments = 32,
        uint32_t rings = 16);

    // 创建平面
    // @param width X 轴宽度
    // @param depth Z 轴深度
    // @param subdivisions 细分级别 (1 = 1x1, 2 = 2x2, ...)
    std::unique_ptr<StaticMesh> CreatePlane(
        float width = 1.0f,
        float depth = 1.0f,
        uint32_t subdivisions = 1);

    // 创建圆柱体
    // @param radius 半径
    // @param height 高度
    // @param segments 圆周分段数
    std::unique_ptr<StaticMesh> CreateCylinder(
        float radius = 0.5f,
        float height = 1.0f,
        uint32_t segments = 32);

    // 创建线框几何体

    // 创建立方体线框
    std::unique_ptr<StaticMesh> CreateCubeWireframe(float size = 1.0f);

    // 创建盒体线框 (非均匀缩放)
    std::unique_ptr<StaticMesh> CreateBoxWireframe(const glm::vec3& halfExtents);

    // 创建球体线框
    std::unique_ptr<StaticMesh> CreateSphereWireframe(
        float radius = 0.5f,
        uint32_t segments = 16);

    // 从顶点数据创建

    // 从 PrimitiveVertex 数组创建
    std::unique_ptr<StaticMesh> CreateFromVertices(
        const PrimitiveVertex* vertices, uint32_t vertexCount,
        const uint32_t* indices = nullptr, uint32_t indexCount = 0);

    // 从 WireframeVertex 数组创建
    std::unique_ptr<StaticMesh> CreateFromWireframeVertices(
        const WireframeVertex* vertices, uint32_t vertexCount,
        const uint32_t* indices = nullptr, uint32_t indexCount = 0);

private:
    MeshFactory() = default;
    ~MeshFactory() = default;

    // 缓存的共享几何体
    std::unique_ptr<StaticMesh> m_Cube;
    std::unique_ptr<StaticMesh> m_Sphere;
    std::unique_ptr<StaticMesh> m_Plane;
    std::unique_ptr<StaticMesh> m_Quad;
    std::unique_ptr<StaticMesh> m_Cylinder;
    std::unique_ptr<StaticMesh> m_CubeWireframe;
    std::unique_ptr<StaticMesh> m_SphereWireframe;

    bool m_Initialized = false;
};

} // namespace GLRenderer
