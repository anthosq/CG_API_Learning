// SSAO Geometry Prepass
// 渲染视图空间法线到 G-Buffer，供 SSAOPass 使用
// 顶点格式与 PBR shader 相同（position, normal, texcoord, tangent, bitangent）

#type vertex
#version 430 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
// location 2-4 存在于 VAO 但本 shader 不需要

layout(std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_View;
    mat4 u_Projection;
    vec4 u_CameraPos;
};

uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;

out vec3 v_ViewNormal;

void main() {
    // 将法线变换到视图空间
    v_ViewNormal = normalize(mat3(u_View) * u_NormalMatrix * a_Normal);
    gl_Position  = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}

#type fragment
#version 430 core

layout(location = 0) out vec4 o_Normal;

in vec3 v_ViewNormal;

void main() {
    // 编码到 [0, 1] 存入 RGBA16F 纹理（xyz = normal, w = 1）
    o_Normal = vec4(normalize(v_ViewNormal) * 0.5 + 0.5, 1.0);
}
