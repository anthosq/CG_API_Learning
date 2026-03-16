#pragma once

#include "utils/GLCommon.h"

namespace GLRenderer {

// CameraMovement - 相机移动方向
enum class CameraMovement {
    Forward,
    Backward,
    Left,
    Right,
    Up,
    Down
};

// 兼容旧枚举
enum Camera_Movement {
    FORWARD = static_cast<int>(CameraMovement::Forward),
    BACKWARD = static_cast<int>(CameraMovement::Backward),
    LEFT = static_cast<int>(CameraMovement::Left),
    RIGHT = static_cast<int>(CameraMovement::Right)
};

// CameraSettings - 相机配置
struct CameraSettings {
    float MovementSpeed = 5.0f;
    float MouseSensitivity = 0.1f;
    float Zoom = 45.0f;
    float NearPlane = 0.1f;
    float FarPlane = 100.0f;
    float MinZoom = 1.0f;
    float MaxZoom = 45.0f;
    float MinPitch = -89.0f;
    float MaxPitch = 89.0f;
};

// Camera - 第一人称相机
class Camera {
public:
    // 构造函数
    Camera(const glm::vec3& position = glm::vec3(0.0f, 0.0f, 3.0f),
           const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw = -90.0f,
           float pitch = 0.0f);

    // 兼容旧构造函数
    Camera(float posX, float posY, float posZ,
           float upX, float upY, float upZ,
           float yaw, float pitch);

    // 矩阵获取
    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;
    glm::mat4 GetViewProjectionMatrix(float aspectRatio) const;

    // 输入处理
    void ProcessKeyboard(CameraMovement direction, float deltaTime);
    void ProcessKeyboard(Camera_Movement direction, float deltaTime);  // 兼容旧API
    void ProcessMouseMovement(float xOffset, float yOffset, bool constrainPitch = true);
    void ProcessMouseScroll(float yOffset);

    // 属性访问（getter）
    const glm::vec3& GetPosition() const { return m_Position; }
    const glm::vec3& GetFront() const { return m_Front; }
    const glm::vec3& GetUp() const { return m_Up; }
    const glm::vec3& GetRight() const { return m_Right; }

    float GetYaw() const { return m_Yaw; }
    float GetPitch() const { return m_Pitch; }
    float GetZoom() const { return m_Settings.Zoom; }
    float GetNearPlane() const { return m_Settings.NearPlane; }
    float GetFarPlane() const { return m_Settings.FarPlane; }

    const CameraSettings& GetSettings() const { return m_Settings; }

    // 属性设置（setter）

    void SetPosition(const glm::vec3& position);
    void SetYaw(float yaw);
    void SetPitch(float pitch);
    void SetSettings(const CameraSettings& settings);

    // 单独设置属性
    void SetMovementSpeed(float speed) { m_Settings.MovementSpeed = speed; }
    void SetMouseSensitivity(float sensitivity) { m_Settings.MouseSensitivity = sensitivity; }
    void SetZoom(float zoom);
    void SetNearPlane(float nearPlane) { m_Settings.NearPlane = nearPlane; }
    void SetFarPlane(float farPlane) { m_Settings.FarPlane = farPlane; }

    // 兼容旧 API（public 成员变量）
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
    float Yaw;
    float Pitch;
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

private:
    void UpdateCameraVectors();
    void SyncPublicMembers();  // 同步公共成员和私有成员

    glm::vec3 m_Position;
    glm::vec3 m_Front;
    glm::vec3 m_Up;
    glm::vec3 m_Right;
    glm::vec3 m_WorldUp;

    float m_Yaw;
    float m_Pitch;

    CameraSettings m_Settings;
};

} // namespace GLRenderer

// 全局命名空间别名
using GLRenderer::Camera;
using GLRenderer::CameraMovement;
using GLRenderer::CameraSettings;
