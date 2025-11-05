#include "gl_common.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <cmath>

#include "Shader.h"
#include "Camera.h"
#include "imgui_layer.h"
#include "stb_image.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


// 全局静态变量用于回调函数
static Camera* g_camera = nullptr;
static bool g_firstMouse = true;
static float g_lastX = 400.0f;
static float g_lastY = 300.0f;

class OpenGLApp {
public:
    OpenGLApp() : window(nullptr), VAO(0), VBO(0), EBO(0), 
                  camera(glm::vec3(0.0f, 0.0f, 3.0f)),
                  lastFrameTime(0.0f), deltaTime(0.0f),
                  imguiLayer(nullptr),
                  cameraControlEnabled(false) {  // 默认关闭相机控制
        g_camera = &camera;
        
        // 初始化光照和材质参数
        lightPos = glm::vec3(1.2f, 0.0f, 2.0f);
        lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
        
        lightAmbient = glm::vec3(0.2f, 0.2f, 0.2f);
        lightDiffuse = glm::vec3(0.5f, 0.5f, 0.5f);
        lightSpecular = glm::vec3(1.0f, 1.0f, 1.0f);
        
        materialAmbient = glm::vec3(1.0f, 0.5f, 0.31f);
        materialDiffuse = glm::vec3(1.0f, 0.5f, 0.31f);
        materialSpecular = glm::vec3(0.5f, 0.5f, 0.5f);
        materialShininess = 32.0f;
        
        autoRotateLight = true;
        lightRotationSpeed = 1.0f;
        lightRotationRadius = 2.0f;
    }
    
    ~OpenGLApp() { cleanup(); }

    void run() {
        initGLFW();
        createWindow();
        initGLAD();
        printGLInfo();
        initImGui();
        setupCallbacks();
        setupBuffers();
        mainLoop();
    }

private:
    GLFWwindow* window;
    unsigned int VAO, VBO, EBO;
    unsigned int texture1, texture2;
    Shader shader, light_shader, lamp_shader;
    unsigned int lightVAO;
    Camera camera;
    ImguiLayer* imguiLayer;

    // 光照参数
    glm::vec3 lightPos;
    glm::vec3 lightColor;
    glm::vec3 lightAmbient;
    glm::vec3 lightDiffuse;
    glm::vec3 lightSpecular;
    
    // 材质参数
    glm::vec3 materialAmbient;
    glm::vec3 materialDiffuse;
    glm::vec3 materialSpecular;
    float materialShininess;
    
    // 控制参数
    bool autoRotateLight;
    float lightRotationSpeed;
    float lightRotationRadius;
    bool cameraControlEnabled;  // 新增：相机控制开关

    float lastFrameTime;
    float deltaTime;
    
    const int WINDOW_WIDTH = 800;
    const int WINDOW_HEIGHT = 600;

    void initGLFW() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }

    void createWindow() {
        window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "OpenGL App", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }
        glfwMakeContextCurrent(window);
        // 不要在这里设置 GLFW_CURSOR_DISABLED，让鼠标默认可见
        // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        
        g_lastX = WINDOW_WIDTH / 2.0f;
        g_lastY = WINDOW_HEIGHT / 2.0f;
    }

    void initGLAD() {
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            throw std::runtime_error("Failed to initialize GLAD");
        }
    }

    void initImGui() {
        imguiLayer = new ImguiLayer(window);
    }

    void printGLInfo() {
        std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
        std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
        std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    }

    void setupCallbacks() {
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetScrollCallback(window, scroll_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);  // 新增
        glfwSetCharCallback(window, char_callback);  // 新增
        glfwSetKeyCallback(window, key_callback);  // 新增
        
        // 设置窗口用户指针，以便在回调中访问类实例
        glfwSetWindowUserPointer(window, this);
    }

    void setupBuffers() {
        glEnable(GL_DEPTH_TEST);

        float vertices[] = {
            -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f,
            -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f,

            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,

            -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f,
            -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f,

            0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,
            0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f,

            -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
            0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
            -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f,

            -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
            -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f
        };

        // 加载着色器
        shader = Shader(
            std::filesystem::path("assets/shaders/test_vertex.glsl"),
            std::filesystem::path("assets/shaders/test_fragment.glsl")
        );

        // 设置 VAO & VBO
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // 配置顶点属性
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // 加载纹理
        // loadTexture(texture1, "assets/pic/container.jpg", GL_RGB);
        // loadTexture(texture2, "assets/pic/awesomeface.png", GL_RGBA);

        // 设置纹理单元
        // shader.use();
        // shader.setInt("texture1", 0);
        // shader.setInt("texture2", 1);

        // 解绑
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // 光源着色器和缓冲区设置
        light_shader = Shader(
            std::filesystem::path("assets/shaders/light_vertex.glsl"),
            std::filesystem::path("assets/shaders/light_fragment.glsl")
        );

        glGenVertexArrays(1, &lightVAO);
        glBindVertexArray(lightVAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        light_shader.use();

        // 解绑
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        lamp_shader = Shader(
            std::filesystem::path("assets/shaders/lamp_vertex.glsl"),
            std::filesystem::path("assets/shaders/lamp_fragment.glsl")
        );
        lamp_shader.use();

    }

    void loadTexture(unsigned int& textureID, const char* path, GLenum format) {
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        int width, height, nrChannels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);

        if (data) {
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
        } else {
            std::cerr << "Failed to load texture: " << path << std::endl;
        }

        stbi_image_free(data);
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            updateDeltaTime();
            processInput();
            render();
            OnImGuiRender();
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    void updateDeltaTime() {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrameTime;
        lastFrameTime = currentFrame;
    }

    void processInput() {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // 按右键切换相机控制模式
        static bool tabPressed = false;
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS && !tabPressed) {
            tabPressed = true;
            cameraControlEnabled = !cameraControlEnabled;
            
            if (cameraControlEnabled) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                g_firstMouse = true;
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
        }
        if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_RELEASE) {
            tabPressed = false;
        }

        // 只在相机控制启用时处理键盘输入
        if (cameraControlEnabled) {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                camera.ProcessKeyboard(FORWARD, deltaTime);
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                camera.ProcessKeyboard(BACKWARD, deltaTime);
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                camera.ProcessKeyboard(LEFT, deltaTime);
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                camera.ProcessKeyboard(RIGHT, deltaTime);
        }
    }

    void render() {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom), 
            static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT), 
            0.1f, 
            100.0f
        );

        // 更新光源位置
        if (autoRotateLight) {
            lightPos.x = cos(glfwGetTime() * lightRotationSpeed) * lightRotationRadius;
            lightPos.z = sin(glfwGetTime() * lightRotationSpeed) * lightRotationRadius;
        }

        // 绘制光源立方体
        lamp_shader.use();
        lamp_shader.setMat4("view", view);
        lamp_shader.setMat4("projection", projection);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, lightPos);
        model = glm::scale(model, glm::vec3(0.2f));
        lamp_shader.setMat4("model", model);
        lamp_shader.setFloat3("lightColor", lightColor.x, lightColor.y, lightColor.z);
        
        glBindVertexArray(lightVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // 绘制被照亮的物体
        light_shader.use();
        light_shader.setMat4("view", view);
        light_shader.setMat4("projection", projection);
        
        model = glm::mat4(1.0f);
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
        light_shader.setMat3("normalMatrix", normalMatrix);
        light_shader.setMat4("model", model);
        
        // 设置光源属性
        light_shader.setFloat3("light.position", lightPos.x, lightPos.y, lightPos.z);
        light_shader.setFloat3("light.ambient", lightAmbient.x, lightAmbient.y, lightAmbient.z);
        light_shader.setFloat3("light.diffuse", lightDiffuse.x, lightDiffuse.y, lightDiffuse.z);
        light_shader.setFloat3("light.specular", lightSpecular.x, lightSpecular.y, lightSpecular.z);
        
        // 设置材质属性
        light_shader.setFloat3("material.ambient", materialAmbient.x, materialAmbient.y, materialAmbient.z);
        light_shader.setFloat3("material.diffuse", materialDiffuse.x, materialDiffuse.y, materialDiffuse.z);
        light_shader.setFloat3("material.specular", materialSpecular.x, materialSpecular.y, materialSpecular.z);
        light_shader.setFloat("material.shininess", materialShininess);
        
        // 设置观察位置
        light_shader.setFloat3("viewPos", camera.Position.x, camera.Position.y, camera.Position.z);

        glBindVertexArray(lightVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glBindVertexArray(0);
    }

    void OnImGuiRender() {
        imguiLayer->Begin();

        ImGui::Begin("Lighting Control Panel");
        
        // FPS 显示
        ImGui::Text("FPS: %.1f (%.3f ms/frame)", 
                    ImGui::GetIO().Framerate, 
                    1000.0f / ImGui::GetIO().Framerate);
        ImGui::Separator();

        // 相机信息
        if (ImGui::CollapsingHeader("Camera Info", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", 
                       camera.Position.x, camera.Position.y, camera.Position.z);
            ImGui::Text("Yaw: %.2f, Pitch: %.2f", camera.Yaw, camera.Pitch);
            ImGui::Text("FOV: %.2f", camera.Zoom);
        }

        ImGui::Separator();

        // 光源控制
        if (ImGui::CollapsingHeader("Light Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Auto Rotate Light", &autoRotateLight);
            
            if (!autoRotateLight) {
                ImGui::SliderFloat3("Light Position", &lightPos.x, -5.0f, 5.0f);
            } else {
                ImGui::SliderFloat("Rotation Speed", &lightRotationSpeed, 0.1f, 5.0f);
                ImGui::SliderFloat("Rotation Radius", &lightRotationRadius, 0.5f, 5.0f);
            }
            
            ImGui::ColorEdit3("Light Color", &lightColor.x);
            ImGui::Separator();
            
            ImGui::Text("Light Properties:");
            ImGui::ColorEdit3("Ambient", &lightAmbient.x);
            ImGui::ColorEdit3("Diffuse", &lightDiffuse.x);
            ImGui::ColorEdit3("Specular", &lightSpecular.x);
        }

        ImGui::Separator();

        // 材质控制
        if (ImGui::CollapsingHeader("Material Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::ColorEdit3("Material Ambient", &materialAmbient.x);
            ImGui::ColorEdit3("Material Diffuse", &materialDiffuse.x);
            ImGui::ColorEdit3("Material Specular", &materialSpecular.x);
            ImGui::SliderFloat("Shininess", &materialShininess, 1.0f, 256.0f);
        }

        ImGui::Separator();

        // 预设按钮
        if (ImGui::CollapsingHeader("Presets")) {
            if (ImGui::Button("Emerald")) {
                materialAmbient = glm::vec3(0.0215f, 0.1745f, 0.0215f);
                materialDiffuse = glm::vec3(0.07568f, 0.61424f, 0.07568f);
                materialSpecular = glm::vec3(0.633f, 0.727811f, 0.633f);
                materialShininess = 76.8f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Gold")) {
                materialAmbient = glm::vec3(0.24725f, 0.1995f, 0.0745f);
                materialDiffuse = glm::vec3(0.75164f, 0.60648f, 0.22648f);
                materialSpecular = glm::vec3(0.628281f, 0.555802f, 0.366065f);
                materialShininess = 51.2f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Ruby")) {
                materialAmbient = glm::vec3(0.1745f, 0.01175f, 0.01175f);
                materialDiffuse = glm::vec3(0.61424f, 0.04136f, 0.04136f);
                materialSpecular = glm::vec3(0.727811f, 0.626959f, 0.626959f);
                materialShininess = 76.8f;
            }
            
            if (ImGui::Button("Cyan Plastic")) {
                materialAmbient = glm::vec3(0.0f, 0.1f, 0.06f);
                materialDiffuse = glm::vec3(0.0f, 0.50980392f, 0.50980392f);
                materialSpecular = glm::vec3(0.50196078f, 0.50196078f, 0.50196078f);
                materialShininess = 32.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Brass")) {
                materialAmbient = glm::vec3(0.329412f, 0.223529f, 0.027451f);
                materialDiffuse = glm::vec3(0.780392f, 0.568627f, 0.113725f);
                materialSpecular = glm::vec3(0.992157f, 0.941176f, 0.807843f);
                materialShininess = 27.8974f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Pearl")) {
                materialAmbient = glm::vec3(0.25f, 0.20725f, 0.20725f);
                materialDiffuse = glm::vec3(1.0f, 0.829f, 0.829f);
                materialSpecular = glm::vec3(0.296648f, 0.296648f, 0.296648f);
                materialShininess = 11.264f;
            }
        }

        ImGui::End();

        imguiLayer->End();
    }

    static void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
        // 先让 ImGui 处理鼠标事件
        ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
        
        OpenGLApp* app = static_cast<OpenGLApp*>(glfwGetWindowUserPointer(window));
        
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return; 
        }
        
        if (app && app->cameraControlEnabled) {
            if (g_firstMouse) {
                g_lastX = static_cast<float>(xpos);
                g_lastY = static_cast<float>(ypos);
                g_firstMouse = false;
            }

            float xoffset = static_cast<float>(xpos) - g_lastX;
            float yoffset = g_lastY - static_cast<float>(ypos);

            g_lastX = static_cast<float>(xpos);
            g_lastY = static_cast<float>(ypos);

            if (g_camera) {
                g_camera->ProcessMouseMovement(xoffset, yoffset);
            }
        }
    }

    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
        ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
        
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }
        
        if (g_camera) {
            g_camera->ProcessMouseScroll(static_cast<float>(yoffset));
        }
    }

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
    }
    // 添加鼠标按钮回调
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
        ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
        
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }
        
    }

    static void char_callback(GLFWwindow* window, unsigned int c) {
        ImGui_ImplGlfw_CharCallback(window, c);
    }

    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
    }

    void cleanup() {
        delete imguiLayer;
        if (VBO) glDeleteBuffers(1, &VBO);
        if (EBO) glDeleteBuffers(1, &EBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (lightVAO) glDeleteVertexArrays(1, &lightVAO);
        if (texture1) glDeleteTextures(1, &texture1);
        if (texture2) glDeleteTextures(1, &texture2);
        if (shader) glDeleteProgram(shader.shader_id);
        if (light_shader) glDeleteProgram(light_shader.shader_id);
        if (lamp_shader) glDeleteProgram(lamp_shader.shader_id);
        if (window) glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    try {
        OpenGLApp app;
        app.run();
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}