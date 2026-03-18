#pragma once

// AABB - 轴对齐包围盒
//
// 用于:
// - 视锥剔除
// - 碰撞检测
// - 模型边界可视化
// - 物理系统

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <array>

namespace GLRenderer {

class AABB {
public:
    glm::vec3 Min = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 Max = glm::vec3(std::numeric_limits<float>::lowest());

    // 构造函数
    AABB() = default;
    AABB(const glm::vec3& min, const glm::vec3& max) : Min(min), Max(max) {}

    // 从点集计算 AABB
    static AABB FromPoints(const glm::vec3* points, size_t count);

    // 从中心和半尺寸创建
    static AABB FromCenterExtents(const glm::vec3& center, const glm::vec3& extents);

    // 扩展 AABB 以包含点
    void Encapsulate(const glm::vec3& point);

    // 扩展 AABB 以包含另一个 AABB
    void Encapsulate(const AABB& other);

    // 变换 AABB（应用矩阵后重新计算）
    // 注意：变换后的 AABB 仍然是轴对齐的，可能比原来大
    AABB Transform(const glm::mat4& transform) const;

    // 属性
    glm::vec3 GetCenter() const { return (Min + Max) * 0.5f; }
    glm::vec3 GetExtents() const { return (Max - Min) * 0.5f; }
    glm::vec3 GetSize() const { return Max - Min; }
    float GetRadius() const { return glm::length(GetExtents()); }

    // 碰撞检测
    bool Contains(const glm::vec3& point) const;
    bool Intersects(const AABB& other) const;

    // 射线相交测试
    // @param origin 射线起点
    // @param direction 射线方向（需要归一化）
    // @param tMin 最近交点参数（输出）
    // @param tMax 最远交点参数（输出）
    // @return 是否相交
    bool RayIntersect(const glm::vec3& origin, const glm::vec3& direction,
                      float& tMin, float& tMax) const;

    // 有效性检查
    bool IsValid() const {
        return Min.x <= Max.x && Min.y <= Max.y && Min.z <= Max.z;
    }

    // 重置为无效状态
    void Reset() {
        Min = glm::vec3(std::numeric_limits<float>::max());
        Max = glm::vec3(std::numeric_limits<float>::lowest());
    }

    // 获取 8 个角点
    std::array<glm::vec3, 8> GetCorners() const;

    // 获取表面积（用于 BVH 构建）
    float GetSurfaceArea() const;

    // 获取体积
    float GetVolume() const;
};

} // namespace GLRenderer
