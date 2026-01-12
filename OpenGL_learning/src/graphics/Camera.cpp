#include "Camera.h"

namespace GLRenderer {

Camera::Camera(const glm::vec3& position, const glm::vec3& up, float yaw, float pitch)
    : m_Position(position),
      m_WorldUp(up),
      m_Yaw(yaw),
      m_Pitch(pitch),
      m_Front(glm::vec3(0.0f, 0.0f, -1.0f)) {
    UpdateCameraVectors();
    SyncPublicMembers();
}

Camera::Camera(float posX, float posY, float posZ,
               float upX, float upY, float upZ,
               float yaw, float pitch)
    : Camera(glm::vec3(posX, posY, posZ),
             glm::vec3(upX, upY, upZ),
             yaw, pitch) {
}

// ============================================================================
// 矩阵获取
// ============================================================================

glm::mat4 Camera::GetViewMatrix() const {
    return glm::lookAt(m_Position, m_Position + m_Front, m_Up);
}

glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const {
    return glm::perspective(
        glm::radians(m_Settings.Zoom),
        aspectRatio,
        m_Settings.NearPlane,
        m_Settings.FarPlane
    );
}

glm::mat4 Camera::GetViewProjectionMatrix(float aspectRatio) const {
    return GetProjectionMatrix(aspectRatio) * GetViewMatrix();
}

// ============================================================================
// 输入处理
// ============================================================================

void Camera::ProcessKeyboard(CameraMovement direction, float deltaTime) {
    float velocity = m_Settings.MovementSpeed * deltaTime;

    switch (direction) {
        case CameraMovement::Forward:
            m_Position += m_Front * velocity;
            break;
        case CameraMovement::Backward:
            m_Position -= m_Front * velocity;
            break;
        case CameraMovement::Left:
            m_Position -= m_Right * velocity;
            break;
        case CameraMovement::Right:
            m_Position += m_Right * velocity;
            break;
        case CameraMovement::Up:
            m_Position += m_WorldUp * velocity;
            break;
        case CameraMovement::Down:
            m_Position -= m_WorldUp * velocity;
            break;
    }

    SyncPublicMembers();
}

void Camera::ProcessKeyboard(Camera_Movement direction, float deltaTime) {
    ProcessKeyboard(static_cast<CameraMovement>(direction), deltaTime);
}

void Camera::ProcessMouseMovement(float xOffset, float yOffset, bool constrainPitch) {
    xOffset *= m_Settings.MouseSensitivity;
    yOffset *= m_Settings.MouseSensitivity;

    m_Yaw += xOffset;
    m_Pitch += yOffset;

    // 限制俯仰角
    if (constrainPitch) {
        m_Pitch = glm::clamp(m_Pitch, m_Settings.MinPitch, m_Settings.MaxPitch);
    }

    UpdateCameraVectors();
    SyncPublicMembers();
}

void Camera::ProcessMouseScroll(float yOffset) {
    m_Settings.Zoom -= yOffset;
    m_Settings.Zoom = glm::clamp(m_Settings.Zoom, m_Settings.MinZoom, m_Settings.MaxZoom);
    SyncPublicMembers();
}

// ============================================================================
// 属性设置
// ============================================================================

void Camera::SetPosition(const glm::vec3& position) {
    m_Position = position;
    SyncPublicMembers();
}

void Camera::SetYaw(float yaw) {
    m_Yaw = yaw;
    UpdateCameraVectors();
    SyncPublicMembers();
}

void Camera::SetPitch(float pitch) {
    m_Pitch = glm::clamp(pitch, m_Settings.MinPitch, m_Settings.MaxPitch);
    UpdateCameraVectors();
    SyncPublicMembers();
}

void Camera::SetSettings(const CameraSettings& settings) {
    m_Settings = settings;
    SyncPublicMembers();
}

void Camera::SetZoom(float zoom) {
    m_Settings.Zoom = glm::clamp(zoom, m_Settings.MinZoom, m_Settings.MaxZoom);
    SyncPublicMembers();
}

// ============================================================================
// 私有方法
// ============================================================================

void Camera::UpdateCameraVectors() {
    // 计算前向向量
    glm::vec3 front;
    front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    front.y = sin(glm::radians(m_Pitch));
    front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    m_Front = glm::normalize(front);

    // 计算右向量和上向量
    m_Right = glm::normalize(glm::cross(m_Front, m_WorldUp));
    m_Up = glm::normalize(glm::cross(m_Right, m_Front));
}

void Camera::SyncPublicMembers() {
    // 同步私有成员到公共成员（兼容旧代码）
    Position = m_Position;
    Front = m_Front;
    Up = m_Up;
    Right = m_Right;
    WorldUp = m_WorldUp;
    Yaw = m_Yaw;
    Pitch = m_Pitch;
    MovementSpeed = m_Settings.MovementSpeed;
    MouseSensitivity = m_Settings.MouseSensitivity;
    Zoom = m_Settings.Zoom;
}

} // namespace GLRenderer
