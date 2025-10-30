#include "gl_common.h"
#include <iostream>
#include <stdexcept>




class OpenGLApp {
public:
    void run() {
        initOpenGL();
        initWindow();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;
    void initWindow() {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(800, 600, "OpenGL App", NULL, NULL);
        if (!window) {
            throw std::runtime_error("Failed to create GLFW window");
        }
        glfwMakeContextCurrent(window);
    }

    void initOpenGL() {
        if(!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    void cleanup() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};


int main() {
    OpenGLApp app;
    try {
        app.run();
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}