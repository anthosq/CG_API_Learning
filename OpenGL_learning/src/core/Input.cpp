#include "Input.h"

namespace GLRenderer {

// 静态成员初始化
GLFWwindow* Input::s_Window = nullptr;
glm::vec2 Input::s_MousePosition = glm::vec2(0.0f);
glm::vec2 Input::s_LastMousePosition = glm::vec2(0.0f);
glm::vec2 Input::s_MouseDelta = glm::vec2(0.0f);
float Input::s_ScrollOffset = 0.0f;
bool Input::s_FirstMouse = true;

void Input::Init(GLFWwindow* window) {
    s_Window = window;

    // 获取初始鼠标位置
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    s_MousePosition = glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos));
    s_LastMousePosition = s_MousePosition;

    // 注册回调
    glfwSetCursorPosCallback(window, MouseCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
}

void Input::OnUpdate() {
    // 重置每帧数据
    s_MouseDelta = glm::vec2(0.0f);
    s_ScrollOffset = 0.0f;
}

// 键盘输入
bool Input::IsKeyPressed(int keycode) {
    int state = glfwGetKey(s_Window, keycode);
    return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::IsKeyReleased(int keycode) {
    return glfwGetKey(s_Window, keycode) == GLFW_RELEASE;
}

// 鼠标输入
bool Input::IsMouseButtonPressed(int button) {
    return glfwGetMouseButton(s_Window, button) == GLFW_PRESS;
}

bool Input::IsMouseButtonReleased(int button) {
    return glfwGetMouseButton(s_Window, button) == GLFW_RELEASE;
}

glm::vec2 Input::GetMousePosition() {
    return s_MousePosition;
}

float Input::GetMouseX() {
    return s_MousePosition.x;
}

float Input::GetMouseY() {
    return s_MousePosition.y;
}

glm::vec2 Input::GetMouseDelta() {
    return s_MouseDelta;
}

float Input::GetScrollOffset() {
    return s_ScrollOffset;
}

void Input::ResetMouseDelta() {
    s_MouseDelta = glm::vec2(0.0f);
}

void Input::ResetScrollOffset() {
    s_ScrollOffset = 0.0f;
}

void Input::ResetFirstMouse() {
    s_FirstMouse = true;
}

// GLFW 回调
void Input::MouseCallback(GLFWwindow* window, double xpos, double ypos) {
    s_MousePosition = glm::vec2(static_cast<float>(xpos), static_cast<float>(ypos));

    if (s_FirstMouse) {
        s_LastMousePosition = s_MousePosition;
        s_FirstMouse = false;
    }

    // 计算增量
    s_MouseDelta.x = s_MousePosition.x - s_LastMousePosition.x;
    s_MouseDelta.y = s_LastMousePosition.y - s_MousePosition.y;  // Y 轴反转

    s_LastMousePosition = s_MousePosition;
}

void Input::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    s_ScrollOffset = static_cast<float>(yoffset);
}

void Input::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // 可以在这里添加按键事件处理
    // 目前使用轮询方式，不需要额外处理
}

void Input::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    // 可以在这里添加鼠标按键事件处理
}

} // namespace GLRenderer
