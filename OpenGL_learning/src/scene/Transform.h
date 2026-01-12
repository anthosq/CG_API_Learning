#pragma once

#include "utils/GLCommon.h"
#include <vector>
#include <memory>

namespace GLRenderer {

// ============================================================================
// Transform - 变换组件
// ============================================================================
// 管理场景对象的位置、旋转、缩放，支持父子层级变换。
//
// 设计思路：
// 1. 使用四元数存储旋转，避免万向节锁
// 2. 延迟计算变换矩阵（脏标记模式）
// 3. 父子变换自动传递：子物体的世界变换 = 父变换 * 本地变换
//
// 使用示例：
//   Transform transform;
//   transform.SetPosition(glm::vec3(1, 2, 3));
//   transform.SetRotation(glm::vec3(0, 45, 0));  // 欧拉角输入
//   transform.SetScale(glm::vec3(1, 1, 1));
//   glm::mat4 worldMatrix = transform.GetWorldMatrix();
// ============================================================================

class Transform {
public:
    Transform() = default;
    ~Transform() = default;

    // ========================================================================
    // 本地变换操作
    // ========================================================================

    // 位置
    void SetPosition(const glm::vec3& position);
    void SetPosition(float x, float y, float z);
    const glm::vec3& GetPosition() const { return m_Position; }

    // 平移
    void Translate(const glm::vec3& delta);
    void Translate(float dx, float dy, float dz);

    // 旋转（欧拉角，单位：度）
    void SetRotation(const glm::vec3& eulerAngles);
    void SetRotation(float pitch, float yaw, float roll);
    glm::vec3 GetRotationEuler() const;

    // 旋转（四元数）
    void SetRotation(const glm::quat& rotation);
    const glm::quat& GetRotation() const { return m_Rotation; }

    // 围绕轴旋转
    void Rotate(const glm::vec3& axis, float angleDegrees);
    void RotateAround(const glm::vec3& point, const glm::vec3& axis, float angleDegrees);

    // 缩放
    void SetScale(const glm::vec3& scale);
    void SetScale(float uniformScale);
    const glm::vec3& GetScale() const { return m_Scale; }

    // ========================================================================
    // 方向向量（基于当前旋转）
    // ========================================================================

    glm::vec3 GetForward() const;   // -Z 方向
    glm::vec3 GetRight() const;     // +X 方向
    glm::vec3 GetUp() const;        // +Y 方向

    // 朝向目标点
    void LookAt(const glm::vec3& target, const glm::vec3& worldUp = glm::vec3(0, 1, 0));

    // ========================================================================
    // 变换矩阵
    // ========================================================================

    // 本地变换矩阵 (TRS: Translation * Rotation * Scale)
    const glm::mat4& GetLocalMatrix() const;

    // 世界变换矩阵 (考虑父变换)
    const glm::mat4& GetWorldMatrix() const;

    // 逆世界变换矩阵
    glm::mat4 GetWorldMatrixInverse() const;

    // 法线变换矩阵 (用于变换法线向量)
    glm::mat3 GetNormalMatrix() const;

    // ========================================================================
    // 父子层级
    // ========================================================================

    void SetParent(Transform* parent);
    Transform* GetParent() const { return m_Parent; }

    const std::vector<Transform*>& GetChildren() const { return m_Children; }

    // 获取世界坐标位置
    glm::vec3 GetWorldPosition() const;

    // 世界坐标转本地坐标
    glm::vec3 WorldToLocal(const glm::vec3& worldPoint) const;

    // 本地坐标转世界坐标
    glm::vec3 LocalToWorld(const glm::vec3& localPoint) const;

    // ========================================================================
    // 脏标记管理
    // ========================================================================

    void MarkDirty();
    bool IsDirty() const { return m_Dirty; }

private:
    void UpdateLocalMatrix() const;
    void UpdateWorldMatrix() const;
    void AddChild(Transform* child);
    void RemoveChild(Transform* child);

    // 本地变换数据
    glm::vec3 m_Position = glm::vec3(0.0f);
    glm::quat m_Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // 单位四元数
    glm::vec3 m_Scale = glm::vec3(1.0f);

    // 父子关系
    Transform* m_Parent = nullptr;
    std::vector<Transform*> m_Children;

    // 缓存的变换矩阵
    mutable glm::mat4 m_LocalMatrix = glm::mat4(1.0f);
    mutable glm::mat4 m_WorldMatrix = glm::mat4(1.0f);
    mutable bool m_Dirty = true;
    mutable bool m_WorldDirty = true;
};

} // namespace GLRenderer

using GLRenderer::Transform;
