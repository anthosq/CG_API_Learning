// point_shadow.glsl — 点光源 Omnidirectional Shadow Depth Pass
//
// 每次渲染 Cubemap 的一个面（6 次 DrawCall）。
// Fragment shader 将世界空间距离归一化后写入 gl_FragDepth：
//   depth = length(fragPos - lightPos) / farPlane
// PBR shader 用 samplerCube 手动比较（不用 samplerCubeShadow）。

#type vertex
#version 430 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_Model;
uniform mat4 u_LightSpaceMatrix; // 当前面的 Projection * View

out VS_OUT {
    vec3 FragPos;
} vs_out;

void main() {
    vec4 worldPos    = u_Model * vec4(a_Position, 1.0);
    vs_out.FragPos   = worldPos.xyz;
    gl_Position      = u_LightSpaceMatrix * worldPos;
}

#type fragment
#version 430 core

in VS_OUT {
    vec3 FragPos;
} fs_in;

uniform vec3  u_LightPos;
uniform float u_FarPlane;

void main() {
    float dist   = length(fs_in.FragPos - u_LightPos);
    gl_FragDepth = dist / u_FarPlane;  // 归一化线性距离 [0, 1]
}
