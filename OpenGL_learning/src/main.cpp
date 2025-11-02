#include "gl_common.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <cmath>

#include "Shader.h"


class OpenGLApp {
public:
    OpenGLApp() : window(nullptr), VAO(0), VBO(0), EBO(0) {}
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
    Shader shader;

    void initGLFW() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }

    void createWindow() {
        window = glfwCreateWindow(800, 600, "OpenGL App", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }
        glfwMakeContextCurrent(window);
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
    }

    void setupBuffers() {
        float vertices[] = {
            // positions         // colors
             0.5f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // top right
             0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // bottom right
            -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f,  // bottom left
            -0.5f,  0.5f, 0.0f,  1.0f, 1.0f, 0.0f   // top left 
        };

        // shaders--------------
        shader = Shader(
            std::filesystem::path("asssets/shaders/test_vertex.glsl"),
            std::filesystem::path("asssets/shaders/test_fragment.glsl")
        );


        // VAO & VBO
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // EBO
        unsigned int indices[] = {
            0, 1, 3,
            1, 2, 3
        };
        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // 配置顶点属性, 对应layout(location = 0) 和 layout(location = 1)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);


        // 解绑
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            processInput();
            render();
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    void processInput() {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }
    }

    void render() {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 设置uniform
        shader.use();
        // float timeValue = static_cast<float>(glfwGetTime());
        // float greenValue = (sin(timeValue) / 2.0f) + 0.5f;
        // int vertexColorLocation = glGetUniformLocation(shaderProgram, "ourColor");
        // glUniform3f(vertexColorLocation, 0.0f, greenValue, 0.0f);


        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }



    void cleanup() {
        if (VBO) glDeleteBuffers(1, &VBO);
        if (EBO) glDeleteBuffers(1, &EBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (shader) glDeleteProgram(shader.shader_id);
        if (window) glfwDestroyWindow(window);
        glfwTerminate();
    }

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
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