#pragma once

// Frustum - 视锥体，用于视锥裁剪（Frustum Culling）
//
// 由 6 个平面（Left/Right/Bottom/Top/Near/Far）围成，法线朝内。
// 使用 Gribb-Hartmann 方法从 VP 矩阵直接提取，O(1)，无需计算角点。
//
// 使用示例:
//   Frustum f = Frustum::FromViewProjection(proj * view);
//   if (f.TestAABB(worldAABB)) { /* 可见，加入绘制列表 */ }

#include "AABB.h"
#include <glm/glm.hpp>
#include <array>

namespace GLRenderer {

class Frustum {
public:
    // 平面枚举（便于调试）
    enum Plane { Left = 0, Right, Bottom, Top, Near, Far };

    // 6 个裁剪平面，存为 vec4(nx, ny, nz, d)
    // 约定：dot(point, plane.xyz) + plane.w >= 0 → 点在平面内侧
    std::array<glm::vec4, 6> Planes;

    // 从 View-Projection 矩阵提取视锥体（Gribb-Hartmann 方法）
    // vp = projection * view（列主序，glm 默认）
    static Frustum FromViewProjection(const glm::mat4& vp) {
        Frustum f;

        // glm 以列主序存储: vp[col][row]
        // 第 i 行向量 = (vp[0][i], vp[1][i], vp[2][i], vp[3][i])
        //
        // 设行向量 r0~r3，各平面 = r3 ± ri：
        //   Left   = r3 + r0    Right  = r3 - r0
        //   Bottom = r3 + r1    Top    = r3 - r1
        //   Near   = r3 + r2    Far    = r3 - r2

        // Left:  row3 + row0
        f.Planes[Left]   = { vp[0][3] + vp[0][0],
                              vp[1][3] + vp[1][0],
                              vp[2][3] + vp[2][0],
                              vp[3][3] + vp[3][0] };

        // Right: row3 - row0
        f.Planes[Right]  = { vp[0][3] - vp[0][0],
                              vp[1][3] - vp[1][0],
                              vp[2][3] - vp[2][0],
                              vp[3][3] - vp[3][0] };

        // Bottom: row3 + row1
        f.Planes[Bottom] = { vp[0][3] + vp[0][1],
                              vp[1][3] + vp[1][1],
                              vp[2][3] + vp[2][1],
                              vp[3][3] + vp[3][1] };

        // Top: row3 - row1
        f.Planes[Top]    = { vp[0][3] - vp[0][1],
                              vp[1][3] - vp[1][1],
                              vp[2][3] - vp[2][1],
                              vp[3][3] - vp[3][1] };

        // Near: row3 + row2
        f.Planes[Near]   = { vp[0][3] + vp[0][2],
                              vp[1][3] + vp[1][2],
                              vp[2][3] + vp[2][2],
                              vp[3][3] + vp[3][2] };

        // Far: row3 - row2
        f.Planes[Far]    = { vp[0][3] - vp[0][2],
                              vp[1][3] - vp[1][2],
                              vp[2][3] - vp[2][2],
                              vp[3][3] - vp[3][2] };

        // 归一化（可选，TestAABB 不需要，但 TestSphere 需要精确距离）
        for (auto& p : f.Planes) {
            float len = glm::length(glm::vec3(p));
            if (len > 1e-6f) p /= len;
        }

        return f;
    }

    // AABB vs Frustum 测试
    // 对每个平面取 p-vertex（沿法线方向最远的角点），
    // 若 p-vertex 在平面外侧则 AABB 完全在视锥外 → 返回 false（剔除）。
    // 时间复杂度 O(6)，不会漏剔（可能有 false positive，保守正确）。
    bool TestAABB(const AABB& aabb) const {
        for (const auto& plane : Planes) {
            // 取 p-vertex：沿法线方向选 Max 或 Min 分量
            glm::vec3 pv = {
                plane.x >= 0.0f ? aabb.Max.x : aabb.Min.x,
                plane.y >= 0.0f ? aabb.Max.y : aabb.Min.y,
                plane.z >= 0.0f ? aabb.Max.z : aabb.Min.z,
            };
            // dot(pv, normal) + d < 0 → p-vertex 在外侧 → 整个 AABB 在外
            if (glm::dot(glm::vec3(plane), pv) + plane.w < 0.0f)
                return false;
        }
        return true;
    }

    // 球体 vs Frustum 粗测试（比 AABB 快，用于预剔除）
    // 需要归一化平面（FromViewProjection 已归一化）
    bool TestSphere(const glm::vec3& center, float radius) const {
        for (const auto& plane : Planes) {
            if (glm::dot(glm::vec3(plane), center) + plane.w < -radius)
                return false;
        }
        return true;
    }

    // 获取视锥 8 个角点（用于 cascade 包围盒计算）
    std::array<glm::vec3, 8> GetCorners() const {
        // 通过求相邻平面交点获得角点
        // 使用逆 VP 矩阵变换 NDC 单位立方体的 8 个角点更简单
        // 注意: 调用者通常直接传 invVP，这里留空由 CalculateCascades 内联实现
        return {};
    }
};

} // namespace GLRenderer
