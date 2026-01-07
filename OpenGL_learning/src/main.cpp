#include "gl_common.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <cmath>
#include <algorithm>
#include <map>

#include "Shader.h"
#include "Camera.h"
#include "imgui_layer.h"
#include "stb_image.h"
#include "Mesh.h"
#include "Grid.h"
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
                  camera(glm::vec3(5.0f, 5.0f, 5.0f)),
                  lastFrameTime(0.0f), deltaTime(0.0f),
                  imguiLayer(nullptr),
                  cameraControlEnabled(false)
    { // 默认关闭相机控制
        g_camera = &camera;
        camera.Yaw = -135.0f;
        camera.Pitch = -30.0f;

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
    unsigned int diffuseMap, specularMap;
    Shader shader, light_shader, lamp_shader;
    Shader single_color_shader;
    
    Shader model_shader;
    Model ourModel;
    unsigned int lightVAO, lightVBO;
    Camera camera;
    ImguiLayer* imguiLayer;

    Grid *grid = nullptr;
    Shader grid_shader;
    bool showGrid = true;
    float gridSize = 100.0f;
    float gridCellSize = 1.0f;

    bool debugDepth = false;
    Shader debugDepthShader;
    unsigned int screenQuadVAO = 0;
    unsigned int screenQuadVBO = 0;
    
    // 帧缓冲和深度纹理
    unsigned int depthMapFBO = 0;
    unsigned int depthTexture = 0;

    unsigned int TransparentVAO, TransparentVBO;
    Shader TransparentShader;
    unsigned int TransparentTexture;

    // 帧缓冲
    unsigned int FBO = 0;
    unsigned int FBOTexture = 0;
    unsigned int RBO = 0;
    Shader screen_shader;
    unsigned int screenQuadVAO2 = 0;
    unsigned int screenQuadVBO2 = 0;


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
    
    const int WINDOW_WIDTH = 1280;
    const int WINDOW_HEIGHT = 720;

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

        float vertices[] = {
            // positions          // normals           // texture coords
            -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
            0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
            0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f,
            0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
            -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
            -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f,

            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
            -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,

            -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
            -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
            -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
            -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,

            0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
            0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
            0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
            0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,

            -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,
            0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f,
            0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
            0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,

            -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
            0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
            0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
            -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f
        };

        // 加载着色器
        // shader = Shader(
        //     std::filesystem::path("assets/shaders/test_vertex.glsl"),
        //     std::filesystem::path("assets/shaders/test_fragment.glsl")
        // );

        // 设置 VAO & VBO
        // glGenVertexArrays(1, &VAO);
        // glGenBuffers(1, &VBO);

        // glBindVertexArray(VAO);
        // glBindBuffer(GL_ARRAY_BUFFER, VBO);
        // glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // 配置顶点属性

        // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        // glEnableVertexAttribArray(0);
        // glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        // glEnableVertexAttribArray(1);

        // 加载纹理
        // loadTexture(texture1, "assets/pic/container.jpg", GL_RGB);
        // loadTexture(texture2, "assets/pic/awesomeface.png", GL_RGBA);

        // 设置纹理单元
        // shader.use();
        // shader.setInt("texture1", 0);
        // shader.setInt("texture2", 1);

        // 解绑

        // glBindBuffer(GL_ARRAY_BUFFER, 0);
        // glBindVertexArray(0);

        // 光源着色器和缓冲区设置
        light_shader = Shader(
            std::filesystem::path("assets/shaders/light_vertex.glsl"),
            std::filesystem::path("assets/shaders/light_fragment.glsl")
        );

        glGenVertexArrays(1, &lightVAO);
        glGenBuffers(1, &lightVBO);
        glBindBuffer(GL_ARRAY_BUFFER, lightVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glBindVertexArray(lightVAO);
        glBindBuffer(GL_ARRAY_BUFFER, lightVBO);

        loadTexture(diffuseMap, "assets/pic/container2.png", GL_RGBA);
        loadTexture(specularMap, "assets/pic/container2_specular.png", GL_RGBA);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);
        light_shader.use();

        // 解绑
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        lamp_shader = Shader(
            std::filesystem::path("assets/shaders/lamp_vertex.glsl"),
            std::filesystem::path("assets/shaders/lamp_fragment.glsl")
        );
        lamp_shader.use();

        model_shader = Shader(
            std::filesystem::path("assets/shaders/model_loading_vertex.glsl"),
            std::filesystem::path("assets/shaders/model_loading_frag.glsl")
        );

        model_shader.use();
        // ourModel = Model("assets/model/backpack/backpack.obj");
        ourModel = Model("assets/model/Hana/Hana.fbx");

        grid_shader = Shader(
            std::filesystem::path("assets/shaders/grid_vertex.glsl"),
            std::filesystem::path("assets/shaders/grid_fragment.glsl")
        );

        single_color_shader = Shader(
            std::filesystem::path("assets/shaders/light_vertex.glsl"),
            std::filesystem::path("assets/shaders/shader_single_color.glsl")
        );

        grid = new Grid(gridSize, gridCellSize);

        debugDepthShader = Shader(
            std::filesystem::path("assets/shaders/screen_vertex.glsl"),
            std::filesystem::path("assets/shaders/depth_debug_fragment.glsl")
        );

        float quadVertices[] = {
            // positions
            -1.0f, 1.0f, 0.0f,
            -1.0f, -1.0f, 0.0f,
            1.0f, -1.0f, 0.0f,

            -1.0f, 1.0f, 0.0f,
            1.0f, -1.0f, 0.0f,
            1.0f, 1.0f, 0.0f
        };

        float ScreenQuadVertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
        };

        glGenVertexArrays(1, &screenQuadVAO);
        glGenBuffers(1, &screenQuadVBO);

        glBindVertexArray(screenQuadVAO);

        glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

        glBindVertexArray(0);

        glGenVertexArrays(1, &screenQuadVAO2);
        glGenBuffers(1, &screenQuadVBO2);

        glBindVertexArray(screenQuadVAO2);

        glBindBuffer(GL_ARRAY_BUFFER, screenQuadVBO2);
        glBufferData(GL_ARRAY_BUFFER, sizeof(ScreenQuadVertices), &ScreenQuadVertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));

        glBindVertexArray(0);

        
        TransparentShader = Shader(
            std::filesystem::path("assets/shaders/grass_vertex.glsl"),
            std::filesystem::path("assets/shaders/grass_fragment.glsl")
        );
        
        float grassVertices[] = {
            // positions        // texture Coords
            0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            
            0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
            1.0f, 1.0f, 0.0f, 1.0f, 1.0f
        };

        glGenVertexArrays(1, &TransparentVAO);
        glGenBuffers(1, &TransparentVBO);
        glBindVertexArray(TransparentVAO);
        glBindBuffer(GL_ARRAY_BUFFER, TransparentVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(grassVertices), grassVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

        loadTransparentTexture(TransparentTexture, "assets/pic/blending_transparent_window.png", GL_RGBA);
        glBindVertexArray(0);
        

        setupFrameBuffer(FBO, FBOTexture);
        screen_shader = Shader(
            std::filesystem::path("assets/shaders/framebuffer_vertex.glsl"),
            std::filesystem::path("assets/shaders/framebuffer_frag.glsl")
        );

        // 创建深度纹理用于调试
        setupDepthTexture();
    }

    void setupFrameBuffer(unsigned int &FBO, unsigned int &texture) {
        // 创建帧缓冲
        glGenFramebuffers(1, &FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);

        // texture
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        // rbo
        glGenRenderbuffers(1, &RBO);
        glBindRenderbuffer(GL_RENDERBUFFER, RBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, WINDOW_WIDTH, WINDOW_HEIGHT);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Framebuffer is not complete!" << std::endl;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    void setupDepthTexture() {
        // 创建深度纹理
        glGenTextures(1, &depthTexture);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 
                     WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        // 创建帧缓冲
        glGenFramebuffers(1, &depthMapFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
        
        // 不需要颜色附件
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Depth framebuffer is not complete!" << std::endl;
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        std::cout << "Depth texture created: " << depthTexture << std::endl;
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

    void loadTransparentTexture(unsigned int& textureID, const char* path, GLenum format) {
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        int width, height, nrChannels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(path, &width, &height, &nrChannels, STBI_rgb_alpha);

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
        // 先正常渲染场景（不显示深度调试时也需要渲染）
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        // glEnable(GL_STENCIL_TEST);
        // glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        // glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        // glStencilMask(0x00);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        // glEnable(GL_STENCIL_TEST);
        // glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        // glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        // glStencilMask(0x00);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // glEnable(GL_CULL_FACE);

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

        glm::vec3 pointLightPositions[] = {
            glm::vec3(0.7f, 0.2f, 2.0f),
            glm::vec3(2.3f, -3.3f, -4.0f),
            glm::vec3(-4.0f, 2.0f, -12.0f),
            glm::vec3(0.0f, 0.0f, -3.0f)
        };

        std::vector<glm::vec3> windowsPosition = {
            glm::vec3(-1.5f, 0.0f, -0.48f),
            glm::vec3(1.5f, 0.0f, 0.51f),
            glm::vec3(0.0f, 0.0f, 0.7f),
            glm::vec3(-0.3f, 0.0f, -2.3f),
            glm::vec3(0.5f, 0.0f, -0.6f)
        };

        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom), 
            static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT), 
            0.1f, 
            100.0f
        );

        if (showGrid) {
            grid->Draw(grid_shader, view, projection);
        }

        // 更新光源位置
        // if (autoRotateLight) {
        //     lightPos.x = cos(glfwGetTime() * lightRotationSpeed) * lightRotationRadius;
        //     lightPos.z = sin(glfwGetTime() * lightRotationSpeed) * lightRotationRadius;
        // }

        // lightPos = {0.0f, 0.0f, 0.0f};

        // 绘制光源立方体

        lamp_shader.use();
        lamp_shader.setMat4("view", view);
        lamp_shader.setMat4("projection", projection);
        
        glBindVertexArray(lightVAO);  // 先绑定 VAO
        
        for (int i = 0; i < 4; i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, pointLightPositions[i]);
            model = glm::scale(model, glm::vec3(0.2f)); 
            lamp_shader.setMat4("model", model);
            lamp_shader.setFloat3("lightColor", lightColor.x, lightColor.y, lightColor.z);
            
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        
        glBindVertexArray(0);  // 解绑
        
        // 绘制被照亮的物体

        light_shader.use();
        
        // 先激活并绑定纹理
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseMap);
        light_shader.setInt("material.diffuse", 0);
        
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, specularMap);
        light_shader.setInt("material.specular", 1);
        
        // 设置材质属性
        light_shader.setFloat("material.shininess", materialShininess);
        
        light_shader.setFloat3("dirLight.direction", -0.2f, -1.0f, -0.3f);
        light_shader.setFloat3("dirLight.ambient", lightAmbient.x, lightAmbient.y, lightAmbient.z);
        light_shader.setFloat3("dirLight.diffuse", lightDiffuse.x, lightDiffuse.y, lightDiffuse.z);
        light_shader.setFloat3("dirLight.specular", lightSpecular.x, lightSpecular.y, lightSpecular.z);

        // 设置点光源
        for (unsigned int i = 0; i < 4; i++) {
            std::string number = std::to_string(i);
            light_shader.setFloat3("pointLight[" + number + "].position", 
                pointLightPositions[i].x, pointLightPositions[i].y, pointLightPositions[i].z);
            light_shader.setFloat3("pointLight[" + number + "].ambient", 
                lightAmbient.x, lightAmbient.y, lightAmbient.z);
            light_shader.setFloat3("pointLight[" + number + "].diffuse", 0.8f, 0.8f, 0.8f);
            light_shader.setFloat3("pointLight[" + number + "].specular", 1.0f, 1.0f, 1.0f);
            light_shader.setFloat("pointLight[" + number + "].constant", 1.0f);
            light_shader.setFloat("pointLight[" + number + "].linear", 0.09f);
            light_shader.setFloat("pointLight[" + number + "].quadratic", 0.032f);
        }

        light_shader.setFloat3("spotLight.position", camera.Position.x, camera.Position.y, camera.Position.z);
        light_shader.setFloat3("spotLight.direction", camera.Front.x, camera.Front.y, camera.Front.z);
        light_shader.setFloat3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
        light_shader.setFloat3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
        light_shader.setFloat3("spotLight.specular", 1.0f, 1.0f, 1.0f);
        light_shader.setFloat("spotLight.constant", 1.0f);
        light_shader.setFloat("spotLight.linear", 0.09f);
        light_shader.setFloat("spotLight.quadratic", 0.032f);
        light_shader.setFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
        light_shader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));

        // 设置观察位置
        light_shader.setFloat3("viewPos", camera.Position.x, camera.Position.y, camera.Position.z);
        
        // 设置视图和投影矩阵
        light_shader.setMat4("view", view);
        light_shader.setMat4("projection", projection);

        // 绑定 VAO 并绘制
        glBindVertexArray(lightVAO);
        
        // 绘制第一个立方体

        // 绘制其他立方体
        for (unsigned int i = 1; i < 10; i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePositions[i]);
            float angle = 20.0f * i;
            model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
            
            glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
            light_shader.setMat3("normalMatrix", normalMatrix);
            light_shader.setMat4("model", model);

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        // model_shader.use();
        // model = glm::mat4(1.0f);
        // model = glm::translate(model, glm::vec3(0.0f, 10.0f, 0.0f));
        // model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
        // model_shader.setMat4("model", model);
        // model_shader.setMat4("view", view);
        // model_shader.setMat4("projection", projection);
        // ourModel.Draw(model_shader);


        // 绘制带轮廓的模型

        // 注意, 模型的基准点问题, 这里scaling会导致模型轮廓位置不正确
        // 想要实现正确的结果, 需要通过法线膨胀
        // 单纯地法线缩放也不行, 对正方体需要合并顶点后再进行法线膨胀
        // 1. 启用模板测试并清除模板值
        glBindVertexArray(lightVAO);
        glEnable(GL_STENCIL_TEST);
        // glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        // glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        // glStencilMask(0x00);
        
        glStencilMask(0xFF);
        // 2. 第一遍：绘制模型，向模板缓冲写入1
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilMask(0xFF);
        light_shader.use();
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
        light_shader.setMat3("normalMatrix", normalMatrix);
        light_shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // 3. 第二遍：绘制放大的轮廓，只在模板值不等于1的地方绘制
        glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        glStencilMask(0x00);  // 禁止写入模板缓冲
        glDisable(GL_DEPTH_TEST);  // 禁用深度测试以确保轮廓可见
        
        // single_color_shader.use();
        // model = glm::mat4(1.0f);
        // model = glm::translate(model, glm::vec3(0.0f, 10.0f, 0.0f));
        // model = glm::scale(model, glm::vec3(1.05f, 1.05f, 1.05f));
        // single_color_shader.setMat4("model", model);
        // single_color_shader.setMat4("view", view);
        // single_color_shader.setMat4("projection", projection);
        // ourModel.Draw(single_color_shader);

        single_color_shader.use();
        single_color_shader.setMat4("view", view);
        single_color_shader.setMat4("projection", projection);
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(1.02f, 1.02f, 1.02f));
        normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
        single_color_shader.setMat3("normalMatrix", normalMatrix);
        single_color_shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // 4. 恢复OpenGL状态
        glStencilMask(0xFF);
        glStencilFunc(GL_ALWAYS, 0, 0xFF);
        glEnable(GL_DEPTH_TEST);


        TransparentShader.use();
        model = glm::mat4(1.0f);
        TransparentShader.setMat4("projection", projection);
        TransparentShader.setMat4("view", view);
        glBindVertexArray(TransparentVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, TransparentTexture);
        TransparentShader.setInt("texture1", 0);
        std::map<float, glm::vec3> sorted;

        for (unsigned int i = 0; i < windowsPosition.size(); i++) {
            float distance = glm::length(camera.Position - windowsPosition[i]);
            sorted[distance] = windowsPosition[i];
        }

        for (std::map<float, glm::vec3>::reverse_iterator it = sorted.rbegin(); it != sorted.rend(); ++it) {
            model = glm::mat4(1.0f);
            model = glm::translate(model, it->second);
            TransparentShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        glBindVertexArray(0);


        if (debugDepth)
        {
            visualizeDepthBuffer();
        }

        // framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        screen_shader.use();
        glBindVertexArray(screenQuadVAO2);
        glDisable(GL_DEPTH_TEST);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, FBOTexture);
        screen_shader.setInt("screenTexture", 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glEnable(GL_DEPTH_TEST);

    }
    
    void visualizeDepthBuffer() {
        // 将当前深度缓冲复制到深度纹理
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
        
        // 禁用深度测试和深度写入
        glDisable(GL_DEPTH_TEST);
        
        debugDepthShader.use();
        debugDepthShader.setFloat("near", 0.1f);
        debugDepthShader.setFloat("far", 100.0f);
        debugDepthShader.setInt("visualizationMode", 0);
        debugDepthShader.setInt("depthMap", 0);
        
        // 绑定深度纹理
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        
        // 绘制全屏四边形
        glBindVertexArray(screenQuadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        
        // 恢复状态
        glEnable(GL_DEPTH_TEST);
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

        // 添加网格控制
        if (ImGui::CollapsingHeader("Grid Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Show Grid", &showGrid);
            
            if (ImGui::SliderFloat("Grid Size", &gridSize, 10.0f, 200.0f)) {
                grid->SetGridSize(gridSize);
            }
            
            if (ImGui::SliderFloat("Cell Size", &gridCellSize, 0.1f, 10.0f)) {
                grid->SetGridCellSize(gridCellSize);
            }
        }

        // depth检测
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Depth Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Visualize Depth Buffer", &debugDepth);
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
        if (grid) delete grid;
        
        if (VBO) glDeleteBuffers(1, &VBO);
        if (EBO) glDeleteBuffers(1, &EBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (lightVAO) glDeleteVertexArrays(1, &lightVAO);
        if (lightVBO) glDeleteBuffers(1, &lightVBO);
        if (screenQuadVAO) glDeleteVertexArrays(1, &screenQuadVAO);
        if (screenQuadVBO) glDeleteBuffers(1, &screenQuadVBO);
        if (diffuseMap) glDeleteTextures(1, &diffuseMap);
        if (specularMap) glDeleteTextures(1, &specularMap);
        if (depthTexture) glDeleteTextures(1, &depthTexture);
        if (depthMapFBO) glDeleteFramebuffers(1, &depthMapFBO);
        if (shader) glDeleteProgram(shader.shader_id);
        if (light_shader) glDeleteProgram(light_shader.shader_id);
        if (lamp_shader) glDeleteProgram(lamp_shader.shader_id);
        if (model_shader) glDeleteProgram(model_shader.shader_id);
        if (grid_shader) glDeleteProgram(grid_shader.shader_id);
        if (single_color_shader) glDeleteProgram(single_color_shader.shader_id);
        if (debugDepthShader) glDeleteProgram(debugDepthShader.shader_id);

        if (FBO) glDeleteFramebuffers(1, &FBO);
        if (FBOTexture) glDeleteTextures(1, &FBOTexture);
        if (RBO) glDeleteRenderbuffers(1, &RBO);
        if (screen_shader) glDeleteProgram(screen_shader);
        if (screenQuadVAO2) glDeleteVertexArrays(1, &screenQuadVAO2);
        if (screenQuadVBO2) glDeleteBuffers(1, &screenQuadVBO2);

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