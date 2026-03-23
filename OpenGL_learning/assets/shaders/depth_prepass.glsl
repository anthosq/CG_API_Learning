// Depth Pre-Pass
//
// 在 GeometryPass 之前仅写入深度，不做任何光照计算。
// GeometryPass 随后切换为 GL_LEQUAL + depthMask=false，
// 确保每个可见片元只执行一次 PBR 着色，消除 overdraw。
//
// 重要：顶点变换必须与 pbr.glsl 完全一致（相同矩阵、相同 a_Position），
// 否则 GL_LEQUAL 深度比较会产生 z-fighting。

#type vertex
#version 430 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_Model;

layout(std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_View;
    mat4 u_Projection;
    vec4 u_CameraPos;
};

void main() {
    gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
}

#type fragment
#version 430 core

void main() {
    // 仅写深度，无颜色输出
    // glColorMask(false) 由 CPU 侧保证，这里无需显式操作
}
