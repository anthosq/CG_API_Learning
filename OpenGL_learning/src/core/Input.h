#pragma once

#include "utils/GLCommon.h"
#include <unordered_set>

namespace GLRenderer {

// Input - 输入管理类（静态类）
class Input {
public:
    // 初始化输入系统（绑定到窗口）
    static void Init(GLFWwindow* window);

    // 每帧更新（重置增量值）
    static void OnUpdate();

    // 键盘输入

    static bool IsKeyPressed(int keycode);
    static bool IsKeyReleased(int keycode);
    // 仅在按下的那一帧返回 true（边缘检测，由 KeyCallback 维护）
    static bool IsKeyJustPressed(int keycode);

    // 鼠标输入

    static bool IsMouseButtonPressed(int button);
    static bool IsMouseButtonReleased(int button);

    // 获取鼠标位置
    static glm::vec2 GetMousePosition();
    static float GetMouseX();
    static float GetMouseY();

    // 获取鼠标移动增量（用于相机控制）
    static glm::vec2 GetMouseDelta();

    // 获取滚轮偏移（每帧）
    static float GetScrollOffset();

    // 状态重置

    // 重置鼠标增量（通常在处理后调用）
    static void ResetMouseDelta();

    // 重置滚轮偏移
    static void ResetScrollOffset();

    // 重置首次鼠标标志（切换相机控制时调用）
    static void ResetFirstMouse();

private:
    // GLFW 回调函数
    static void MouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

    static GLFWwindow* s_Window;
    static glm::vec2 s_MousePosition;
    static glm::vec2 s_LastMousePosition;
    static glm::vec2 s_MouseDelta;
    static float s_ScrollOffset;
    static bool s_FirstMouse;
    static std::unordered_set<int> s_JustPressedKeys;  // 当帧按下的键，OnUpdate 清空
};

} // namespace GLRenderer

using GLRenderer::Input;
