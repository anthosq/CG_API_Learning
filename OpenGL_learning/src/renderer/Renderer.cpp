#include "Renderer.h"

namespace GLRenderer {

struct RendererData {
    Ref<Texture2D> WhiteTexture;
    Ref<Texture2D> BlackTexture;
    Ref<Texture2D> NormalTexture;
    Ref<TextureCube> BlackCubeTexture;

    Ref<VertexArray> FullscreenQuadVAO;
};

static RendererData s_Data;

SceneData Renderer::s_SceneData;
RendererStats Renderer::s_Stats;
std::unique_ptr<VertexArray> Renderer::s_FullscreenQuadVAO;
bool Renderer::s_Initialized = false;

void Renderer::Init() {
    if (s_Initialized) return;

    RenderCommand::Init();

    // 创建 1x1 白色纹理
    uint32_t whiteData = 0xFFFFFFFF;
    TextureSpec defaultSpec;
    defaultSpec.GenerateMipmaps = false;
    defaultSpec.MinFilter = GL_NEAREST;
    defaultSpec.MagFilter = GL_NEAREST;
    s_Data.WhiteTexture = Texture2D::Create(&whiteData, 1, 1, GL_RGBA, GL_RGBA8, defaultSpec);

    // 创建 1x1 黑色纹理
    uint32_t blackData = 0xFF000000;
    s_Data.BlackTexture = Texture2D::Create(&blackData, 1, 1, GL_RGBA, GL_RGBA8, defaultSpec);

    // 创建 1x1 法线纹理 (128, 128, 255, 255) -> 朝向 +Z 的默认法线
    uint32_t normalData = 0xFFFF8080;
    s_Data.NormalTexture = Texture2D::Create(&normalData, 1, 1, GL_RGBA, GL_RGBA8, defaultSpec);

    CreateFullscreenQuad();

    s_Initialized = true;
    std::cout << "[Renderer] 初始化完成" << std::endl;
}

void Renderer::Shutdown() {
    s_Data.WhiteTexture = nullptr;
    s_Data.BlackTexture = nullptr;
    s_Data.NormalTexture = nullptr;
    s_Data.BlackCubeTexture = nullptr;
    s_Data.FullscreenQuadVAO = nullptr;

    s_FullscreenQuadVAO.reset();
    s_Initialized = false;
    std::cout << "[Renderer] 已关闭" << std::endl;
}

Ref<Texture2D> Renderer::GetWhiteTexture() {
    return s_Data.WhiteTexture;
}

Ref<Texture2D> Renderer::GetBlackTexture() {
    return s_Data.BlackTexture;
}

Ref<Texture2D> Renderer::GetNormalTexture() {
    return s_Data.NormalTexture;
}

Ref<TextureCube> Renderer::GetBlackCubeTexture() {
    return s_Data.BlackCubeTexture;
}

void Renderer::BeginFrame(const Camera& camera, float aspectRatio) {
    s_Stats.Reset();

    s_SceneData.ViewMatrix = camera.GetViewMatrix();
    s_SceneData.ProjectionMatrix = camera.GetProjectionMatrix(aspectRatio);
    s_SceneData.ViewProjectionMatrix = s_SceneData.ProjectionMatrix * s_SceneData.ViewMatrix;
    s_SceneData.CameraPosition = camera.GetPosition();
    s_SceneData.NearPlane = camera.GetNearPlane();
    s_SceneData.FarPlane = camera.GetFarPlane();
}

void Renderer::EndFrame() {
}

void Renderer::SetupShaderMatrices(Shader& shader) {
    shader.SetMat4("view", s_SceneData.ViewMatrix);
    shader.SetMat4("projection", s_SceneData.ProjectionMatrix);
    shader.SetVec3("viewPos", s_SceneData.CameraPosition);
}

void Renderer::SetupShaderMatrices(Shader& shader, const glm::mat4& model) {
    SetupShaderMatrices(shader);
    shader.SetMat4("model", model);

    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
    shader.SetMat3("normalMatrix", normalMatrix);
}

void Renderer::SetupDirectionalLight(Shader& shader, const DirectionalLight& light) {
    shader.SetVec3("dirLight.direction", light.Direction);
    shader.SetVec3("dirLight.ambient", light.Ambient);
    shader.SetVec3("dirLight.diffuse", light.Diffuse);
    shader.SetVec3("dirLight.specular", light.Specular);
}

void Renderer::SetupPointLights(Shader& shader, const PointLight* lights, int count) {
    for (int i = 0; i < count; ++i) {
        std::string base = "pointLight[" + std::to_string(i) + "]";
        shader.SetVec3(base + ".position", lights[i].Position);
        shader.SetVec3(base + ".ambient", lights[i].Ambient);
        shader.SetVec3(base + ".diffuse", lights[i].Diffuse);
        shader.SetVec3(base + ".specular", lights[i].Specular);
        shader.SetFloat(base + ".constant", lights[i].Constant);
        shader.SetFloat(base + ".linear", lights[i].Linear);
        shader.SetFloat(base + ".quadratic", lights[i].Quadratic);
    }
}

void Renderer::SetupSpotLight(Shader& shader, const SpotLight& light) {
    shader.SetVec3("spotLight.position", light.Position);
    shader.SetVec3("spotLight.direction", light.Direction);
    shader.SetVec3("spotLight.ambient", light.Ambient);
    shader.SetVec3("spotLight.diffuse", light.Diffuse);
    shader.SetVec3("spotLight.specular", light.Specular);
    shader.SetFloat("spotLight.constant", light.Constant);
    shader.SetFloat("spotLight.linear", light.Linear);
    shader.SetFloat("spotLight.quadratic", light.Quadratic);
    shader.SetFloat("spotLight.cutOff", glm::cos(glm::radians(light.InnerCutOff)));
    shader.SetFloat("spotLight.outerCutOff", glm::cos(glm::radians(light.OuterCutOff)));
}

void Renderer::DisableSpotLight(Shader& shader) {
    shader.SetVec3("spotLight.position", glm::vec3(0.0f));
    shader.SetVec3("spotLight.direction", glm::vec3(0.0f, -1.0f, 0.0f));
    shader.SetVec3("spotLight.ambient", glm::vec3(0.0f));
    shader.SetVec3("spotLight.diffuse", glm::vec3(0.0f));
    shader.SetVec3("spotLight.specular", glm::vec3(0.0f));
    shader.SetFloat("spotLight.constant", 1.0f);
    shader.SetFloat("spotLight.linear", 0.0f);
    shader.SetFloat("spotLight.quadratic", 0.0f);
    shader.SetFloat("spotLight.cutOff", glm::cos(glm::radians(10.0f)));
    shader.SetFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));
}

void Renderer::DrawFullscreenQuad() {
    if (!s_FullscreenQuadVAO) return;

    RenderCommand::DisableDepthTest();
    s_FullscreenQuadVAO->Bind();
    RenderCommand::DrawArrays(GL_TRIANGLES, 0, 6);
    s_FullscreenQuadVAO->Unbind();
    RenderCommand::EnableDepthTest();

    RecordDrawCall(6);
}

void Renderer::RecordDrawCall(uint32_t vertexCount, uint32_t indexCount) {
    s_Stats.DrawCalls++;
    s_Stats.Vertices += vertexCount;
    if (indexCount > 0) {
        s_Stats.Triangles += indexCount / 3;
    } else {
        s_Stats.Triangles += vertexCount / 3;
    }
}

void Renderer::SubmitMesh(Mesh& mesh, Shader& shader, const glm::mat4& transform, int entityID) {
    shader.Bind();
    SetupShaderMatrices(shader, transform);

    shader.SetInt("u_EntityID", entityID);

    mesh.Draw(shader);

    RecordDrawCall(static_cast<uint32_t>(mesh.vertices.size()),
                   static_cast<uint32_t>(mesh.indices.size()));
}

void Renderer::SubmitModel(Model& model, Shader& shader, const glm::mat4& transform, int entityID) {
    shader.Bind();
    SetupShaderMatrices(shader, transform);

    shader.SetInt("u_EntityID", entityID);

    model.Draw(shader);

    s_Stats.DrawCalls++;
}

void Renderer::CreateFullscreenQuad() {
    float quadVertices[] = {
        -1.0f,  1.0f,    0.0f, 1.0f,
        -1.0f, -1.0f,    0.0f, 0.0f,
         1.0f, -1.0f,    1.0f, 0.0f,

        -1.0f,  1.0f,    0.0f, 1.0f,
         1.0f, -1.0f,    1.0f, 0.0f,
         1.0f,  1.0f,    1.0f, 1.0f
    };

    s_FullscreenQuadVAO = std::make_unique<VertexArray>();
    auto vbo = VertexBuffer::Create(quadVertices, sizeof(quadVertices));

    std::vector<VertexAttribute> layout = {
        VertexAttribute::Float(0, 2, 4 * sizeof(float), 0),
        VertexAttribute::Float(1, 2, 4 * sizeof(float), 2 * sizeof(float))
    };

    s_FullscreenQuadVAO->AddVertexBuffer(vbo, layout);
}

}
