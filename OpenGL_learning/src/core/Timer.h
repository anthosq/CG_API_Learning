#pragma once

#include "utils/GLCommon.h"

namespace GLRenderer {

// Timer - 时间管理类
class Timer {
public:
    Timer() : m_LastFrameTime(0.0f), m_DeltaTime(0.0f) {}

    // 每帧调用，更新时间
    void Update() {
        float currentTime = static_cast<float>(glfwGetTime());
        m_DeltaTime = currentTime - m_LastFrameTime;
        m_LastFrameTime = currentTime;

        // 防止 deltaTime 过大（如窗口拖动时）
        if (m_DeltaTime > 0.1f) {
            m_DeltaTime = 0.016f;  // 约 60 FPS
        }
    }

    // 获取帧时间间隔
    float GetDeltaTime() const { return m_DeltaTime; }

    // 获取程序运行总时间
    float GetTime() const { return m_LastFrameTime; }

    // 获取 FPS
    float GetFPS() const {
        return m_DeltaTime > 0.0f ? 1.0f / m_DeltaTime : 0.0f;
    }

    // 重置计时器
    void Reset() {
        m_LastFrameTime = static_cast<float>(glfwGetTime());
        m_DeltaTime = 0.0f;
    }

private:
    float m_LastFrameTime;
    float m_DeltaTime;
};

} // namespace GLRenderer

using GLRenderer::Timer;
