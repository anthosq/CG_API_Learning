#pragma once

#include "utils/GLCommon.h"
#include "graphics/Shader.h"

namespace GLRenderer {

// ============================================================================
// 光源类型
// ============================================================================

// 方向光
struct DirectionalLight {
    glm::vec3 Direction = glm::vec3(-0.2f, -1.0f, -0.3f);
    glm::vec3 Ambient = glm::vec3(0.05f);
    glm::vec3 Diffuse = glm::vec3(0.4f);
    glm::vec3 Specular = glm::vec3(0.5f);
};

// 点光源
struct PointLight {
    glm::vec3 Position = glm::vec3(0.0f);
    glm::vec3 Ambient = glm::vec3(0.05f);
    glm::vec3 Diffuse = glm::vec3(0.8f);
    glm::vec3 Specular = glm::vec3(1.0f);

    // 衰减参数
    float Constant = 1.0f;
    float Linear = 0.09f;
    float Quadratic = 0.032f;
};

// 聚光灯
struct SpotLight {
    glm::vec3 Position = glm::vec3(0.0f);
    glm::vec3 Direction = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 Ambient = glm::vec3(0.0f);
    glm::vec3 Diffuse = glm::vec3(1.0f);
    glm::vec3 Specular = glm::vec3(1.0f);

    // 衰减参数
    float Constant = 1.0f;
    float Linear = 0.09f;
    float Quadratic = 0.032f;

    // 聚光角度（度数）
    float InnerCutOff = 12.5f;
    float OuterCutOff = 15.0f;
};

// ============================================================================
// 光源辅助函数
// ============================================================================

namespace LightUtils {

// 设置方向光 uniform
inline void SetDirectionalLight(Shader& shader, const DirectionalLight& light,
                                 const std::string& prefix = "dirLight") {
    shader.SetVec3(prefix + ".direction", light.Direction);
    shader.SetVec3(prefix + ".ambient", light.Ambient);
    shader.SetVec3(prefix + ".diffuse", light.Diffuse);
    shader.SetVec3(prefix + ".specular", light.Specular);
}

// 设置点光源 uniform
inline void SetPointLight(Shader& shader, const PointLight& light, int index,
                          const std::string& prefix = "pointLight") {
    std::string base = prefix + "[" + std::to_string(index) + "]";
    shader.SetVec3(base + ".position", light.Position);
    shader.SetVec3(base + ".ambient", light.Ambient);
    shader.SetVec3(base + ".diffuse", light.Diffuse);
    shader.SetVec3(base + ".specular", light.Specular);
    shader.SetFloat(base + ".constant", light.Constant);
    shader.SetFloat(base + ".linear", light.Linear);
    shader.SetFloat(base + ".quadratic", light.Quadratic);
}

// 设置聚光灯 uniform
inline void SetSpotLight(Shader& shader, const SpotLight& light,
                         const std::string& prefix = "spotLight") {
    shader.SetVec3(prefix + ".position", light.Position);
    shader.SetVec3(prefix + ".direction", light.Direction);
    shader.SetVec3(prefix + ".ambient", light.Ambient);
    shader.SetVec3(prefix + ".diffuse", light.Diffuse);
    shader.SetVec3(prefix + ".specular", light.Specular);
    shader.SetFloat(prefix + ".constant", light.Constant);
    shader.SetFloat(prefix + ".linear", light.Linear);
    shader.SetFloat(prefix + ".quadratic", light.Quadratic);
    shader.SetFloat(prefix + ".innerCutOff", glm::cos(glm::radians(light.InnerCutOff)));
    shader.SetFloat(prefix + ".outerCutOff", glm::cos(glm::radians(light.OuterCutOff)));
}

} // namespace LightUtils

} // namespace GLRenderer

using GLRenderer::DirectionalLight;
using GLRenderer::PointLight;
using GLRenderer::SpotLight;
