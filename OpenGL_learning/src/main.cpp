#include "gl_common.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <cmath>

#include "Shader.h"
#include "Camera.h"
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
                  lastFrameTime(0.0f), deltaTime(0.0f) {
        g_camera = &camera;
    }
    ~OpenGLApp() { cleanup(); }

    void run() {
        initGLFW();
        createWindow();
        initGLAD();
        printGLInfo();
        setupCallbacks();
        setupBuffers();
        mainLoop();
    }

private:
    GLFWwindow* window;
    unsigned int VAO, VBO, EBO;
    unsigned int texture1, texture2;
    Shader shader;
    Camera camera;
    
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
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        
        // 设置初始鼠标位置
        g_lastX = WINDOW_WIDTH / 2.0f;
        g_lastY = WINDOW_HEIGHT / 2.0f;
    }

    void initGLAD() {
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            throw std::runtime_error("Failed to initialize GLAD");
        }
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
    }

    void setupBuffers() {
        glEnable(GL_DEPTH_TEST);

        float vertices[] = {
            -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
            0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
            0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
            0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
            -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
            -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,

            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
            0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 1.0f,
            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,

            -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
            -0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
            -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
            -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
            -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

            0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
            0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
            0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
            0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
            0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
            0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

            -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
            0.5f, -0.5f, -0.5f, 1.0f, 1.0f,
            0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,

            -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
            0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
            0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 0.0f,
            -0.5f, 0.5f, -0.5f, 0.0f, 1.0f
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
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // 加载纹理
        loadTexture(texture1, "assets/pic/container.jpg", GL_RGB);
        loadTexture(texture2, "assets/pic/awesomeface.png", GL_RGBA);

        // 设置纹理单元
        shader.use();
        shader.setInt("texture1", 0);
        shader.setInt("texture2", 1);

        // 解绑
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
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

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.ProcessKeyboard(FORWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.ProcessKeyboard(RIGHT, deltaTime);
    }

    void render() {
        glm::vec3 cubePositions[] = {
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(2.0f, 5.0f, -15.0f),
            glm::vec3(-1.5f, -2.2f, -2.5f),
            glm::vec3(-3.8f, -2.0f, -12.3f),
            glm::vec3(2.4f, -0.4f, -3.5f),
            glm::vec3(-1.7f, 3.0f, -7.5f),
            glm::vec3(1.3f, -2.0f, -2.5f),
            glm::vec3(1.5f, 2.0f, -2.5f),
            glm::vec3(1.5f, 0.2f, -1.5f),
            glm::vec3(-1.3f, 1.0f, -1.5f)
        };

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        // 激活纹理
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture2);

        // 设置变换矩阵
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom), 
            static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT), 
            0.1f, 
            100.0f
        );

        shader.setMat4("view", view);
        shader.setMat4("projection", projection);

        // 绘制立方体
        glBindVertexArray(VAO);
        for (int i = 0; i < 10; i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePositions[i]);
            float angle = 20.0f * i;
            model = glm::rotate(
                model, 
                static_cast<float>(glfwGetTime()) * glm::radians(50.0f + angle), 
                glm::vec3(1.0f, 0.3f, 0.5f)
            );
            shader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        glBindVertexArray(0);
    }

    static void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
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

    static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
        if (g_camera) {
            g_camera->ProcessMouseScroll(static_cast<float>(yoffset));
        }
    }

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
    }

    void cleanup() {
        if (VBO) glDeleteBuffers(1, &VBO);
        if (EBO) glDeleteBuffers(1, &EBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (texture1) glDeleteTextures(1, &texture1);
        if (texture2) glDeleteTextures(1, &texture2);
        if (shader) glDeleteProgram(shader.shader_id);
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