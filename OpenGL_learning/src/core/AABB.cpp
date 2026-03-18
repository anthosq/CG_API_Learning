#include "AABB.h"
#include <algorithm>
#include <cmath>

namespace GLRenderer {

AABB AABB::FromPoints(const glm::vec3* points, size_t count) {
    AABB result;
    for (size_t i = 0; i < count; ++i) {
        result.Encapsulate(points[i]);
    }
    return result;
}

AABB AABB::FromCenterExtents(const glm::vec3& center, const glm::vec3& extents) {
    return AABB(center - extents, center + extents);
}

void AABB::Encapsulate(const glm::vec3& point) {
    Min = glm::min(Min, point);
    Max = glm::max(Max, point);
}

void AABB::Encapsulate(const AABB& other) {
    if (!other.IsValid()) return;
    Min = glm::min(Min, other.Min);
    Max = glm::max(Max, other.Max);
}

AABB AABB::Transform(const glm::mat4& transform) const {
    if (!IsValid()) return *this;

    // 获取 8 个角点并变换
    auto corners = GetCorners();

    AABB result;
    for (const auto& corner : corners) {
        glm::vec4 transformed = transform * glm::vec4(corner, 1.0f);
        result.Encapsulate(glm::vec3(transformed));
    }

    return result;
}

bool AABB::Contains(const glm::vec3& point) const {
    return point.x >= Min.x && point.x <= Max.x &&
           point.y >= Min.y && point.y <= Max.y &&
           point.z >= Min.z && point.z <= Max.z;
}

bool AABB::Intersects(const AABB& other) const {
    if (!IsValid() || !other.IsValid()) return false;

    return Min.x <= other.Max.x && Max.x >= other.Min.x &&
           Min.y <= other.Max.y && Max.y >= other.Min.y &&
           Min.z <= other.Max.z && Max.z >= other.Min.z;
}

bool AABB::RayIntersect(const glm::vec3& origin, const glm::vec3& direction,
                        float& tMin, float& tMax) const {
    if (!IsValid()) return false;

    tMin = 0.0f;
    tMax = std::numeric_limits<float>::max();

    for (int i = 0; i < 3; ++i) {
        if (std::abs(direction[i]) < 1e-8f) {
            // 射线与该轴平行
            if (origin[i] < Min[i] || origin[i] > Max[i]) {
                return false;
            }
        } else {
            float invD = 1.0f / direction[i];
            float t0 = (Min[i] - origin[i]) * invD;
            float t1 = (Max[i] - origin[i]) * invD;

            if (invD < 0.0f) {
                std::swap(t0, t1);
            }

            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);

            if (tMin > tMax) {
                return false;
            }
        }
    }

    return true;
}

std::array<glm::vec3, 8> AABB::GetCorners() const {
    return {
        glm::vec3(Min.x, Min.y, Min.z),  // 0: 左下后
        glm::vec3(Max.x, Min.y, Min.z),  // 1: 右下后
        glm::vec3(Max.x, Max.y, Min.z),  // 2: 右上后
        glm::vec3(Min.x, Max.y, Min.z),  // 3: 左上后
        glm::vec3(Min.x, Min.y, Max.z),  // 4: 左下前
        glm::vec3(Max.x, Min.y, Max.z),  // 5: 右下前
        glm::vec3(Max.x, Max.y, Max.z),  // 6: 右上前
        glm::vec3(Min.x, Max.y, Max.z)   // 7: 左上前
    };
}

float AABB::GetSurfaceArea() const {
    if (!IsValid()) return 0.0f;
    glm::vec3 size = GetSize();
    return 2.0f * (size.x * size.y + size.y * size.z + size.z * size.x);
}

float AABB::GetVolume() const {
    if (!IsValid()) return 0.0f;
    glm::vec3 size = GetSize();
    return size.x * size.y * size.z;
}

} // namespace GLRenderer
