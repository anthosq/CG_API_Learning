#define GLM_ENABLE_EXPERIMENTAL
#include "Transform.h"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <algorithm>

namespace GLRenderer {

// ============================================================================
// 位置操作
// ============================================================================

void Transform::SetPosition(const glm::vec3& position) {
    m_Position = position;
    MarkDirty();
}

void Transform::SetPosition(float x, float y, float z) {
    SetPosition(glm::vec3(x, y, z));
}

void Transform::Translate(const glm::vec3& delta) {
    m_Position += delta;
    MarkDirty();
}

void Transform::Translate(float dx, float dy, float dz) {
    Translate(glm::vec3(dx, dy, dz));
}

// ============================================================================
// 旋转操作
// ============================================================================

void Transform::SetRotation(const glm::vec3& eulerAngles) {
    // 欧拉角转四元数 (pitch, yaw, roll -> 角度转弧度)
    glm::vec3 radians = glm::radians(eulerAngles);
    m_Rotation = glm::quat(radians);
    MarkDirty();
}

void Transform::SetRotation(float pitch, float yaw, float roll) {
    SetRotation(glm::vec3(pitch, yaw, roll));
}

void Transform::SetRotation(const glm::quat& rotation) {
    m_Rotation = glm::normalize(rotation);
    MarkDirty();
}

glm::vec3 Transform::GetRotationEuler() const {
    return glm::degrees(glm::eulerAngles(m_Rotation));
}

void Transform::Rotate(const glm::vec3& axis, float angleDegrees) {
    glm::quat deltaRotation = glm::angleAxis(glm::radians(angleDegrees), glm::normalize(axis));
    m_Rotation = deltaRotation * m_Rotation;
    m_Rotation = glm::normalize(m_Rotation);
    MarkDirty();
}

void Transform::RotateAround(const glm::vec3& point, const glm::vec3& axis, float angleDegrees) {
    // 1. 计算从旋转中心到当前位置的向量
    glm::vec3 offset = m_Position - point;

    // 2. 围绕轴旋转这个向量
    glm::quat rotation = glm::angleAxis(glm::radians(angleDegrees), glm::normalize(axis));
    offset = rotation * offset;

    // 3. 更新位置
    m_Position = point + offset;

    // 4. 也旋转自身
    m_Rotation = rotation * m_Rotation;
    m_Rotation = glm::normalize(m_Rotation);

    MarkDirty();
}

// ============================================================================
// 缩放操作
// ============================================================================

void Transform::SetScale(const glm::vec3& scale) {
    m_Scale = scale;
    MarkDirty();
}

void Transform::SetScale(float uniformScale) {
    SetScale(glm::vec3(uniformScale));
}

// ============================================================================
// 方向向量
// ============================================================================

glm::vec3 Transform::GetForward() const {
    // OpenGL 默认相机朝向 -Z
    return glm::normalize(m_Rotation * glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::vec3 Transform::GetRight() const {
    return glm::normalize(m_Rotation * glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::vec3 Transform::GetUp() const {
    return glm::normalize(m_Rotation * glm::vec3(0.0f, 1.0f, 0.0f));
}

void Transform::LookAt(const glm::vec3& target, const glm::vec3& worldUp) {
    glm::vec3 direction = glm::normalize(target - m_Position);

    // 如果方向几乎与 worldUp 平行，使用备用 up 向量
    glm::vec3 up = worldUp;
    if (glm::abs(glm::dot(direction, worldUp)) > 0.999f) {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    // 构建旋转矩阵
    glm::vec3 right = glm::normalize(glm::cross(direction, up));
    glm::vec3 actualUp = glm::cross(right, direction);

    glm::mat3 rotMat;
    rotMat[0] = right;
    rotMat[1] = actualUp;
    rotMat[2] = -direction;  // OpenGL 相机朝向 -Z

    m_Rotation = glm::normalize(glm::quat_cast(rotMat));
    MarkDirty();
}

// ============================================================================
// 变换矩阵
// ============================================================================

const glm::mat4& Transform::GetLocalMatrix() const {
    if (m_Dirty) {
        UpdateLocalMatrix();
    }
    return m_LocalMatrix;
}

const glm::mat4& Transform::GetWorldMatrix() const {
    if (m_Dirty || m_WorldDirty) {
        UpdateWorldMatrix();
    }
    return m_WorldMatrix;
}

glm::mat4 Transform::GetWorldMatrixInverse() const {
    return glm::inverse(GetWorldMatrix());
}

glm::mat3 Transform::GetNormalMatrix() const {
    // 法线矩阵 = 世界矩阵左上角3x3的逆转置
    return glm::transpose(glm::inverse(glm::mat3(GetWorldMatrix())));
}

void Transform::UpdateLocalMatrix() const {
    // TRS: Translation * Rotation * Scale
    glm::mat4 T = glm::translate(glm::mat4(1.0f), m_Position);
    glm::mat4 R = glm::toMat4(m_Rotation);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), m_Scale);

    m_LocalMatrix = T * R * S;
    m_Dirty = false;
    m_WorldDirty = true;  // 本地变化会影响世界矩阵
}

void Transform::UpdateWorldMatrix() const {
    if (m_Dirty) {
        UpdateLocalMatrix();
    }

    if (m_Parent) {
        m_WorldMatrix = m_Parent->GetWorldMatrix() * m_LocalMatrix;
    } else {
        m_WorldMatrix = m_LocalMatrix;
    }

    m_WorldDirty = false;
}

// ============================================================================
// 父子层级
// ============================================================================

void Transform::SetParent(Transform* parent) {
    // 从旧父节点移除
    if (m_Parent) {
        m_Parent->RemoveChild(this);
    }

    // 设置新父节点
    m_Parent = parent;

    // 添加到新父节点
    if (m_Parent) {
        m_Parent->AddChild(this);
    }

    // 标记世界矩阵需要更新
    m_WorldDirty = true;
}

void Transform::AddChild(Transform* child) {
    if (std::find(m_Children.begin(), m_Children.end(), child) == m_Children.end()) {
        m_Children.push_back(child);
    }
}

void Transform::RemoveChild(Transform* child) {
    auto it = std::find(m_Children.begin(), m_Children.end(), child);
    if (it != m_Children.end()) {
        m_Children.erase(it);
    }
}

glm::vec3 Transform::GetWorldPosition() const {
    return glm::vec3(GetWorldMatrix()[3]);
}

glm::vec3 Transform::WorldToLocal(const glm::vec3& worldPoint) const {
    return glm::vec3(GetWorldMatrixInverse() * glm::vec4(worldPoint, 1.0f));
}

glm::vec3 Transform::LocalToWorld(const glm::vec3& localPoint) const {
    return glm::vec3(GetWorldMatrix() * glm::vec4(localPoint, 1.0f));
}

// ============================================================================
// 脏标记管理
// ============================================================================

void Transform::MarkDirty() {
    m_Dirty = true;
    m_WorldDirty = true;

    // 递归标记所有子节点
    for (Transform* child : m_Children) {
        child->MarkDirty();
    }
}

} // namespace GLRenderer
