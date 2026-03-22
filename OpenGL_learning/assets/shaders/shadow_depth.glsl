// Shadow Depth Pass Shader
// 将场景几何体从光源视角投影到深度贴图，仅写入深度，无颜色输出。
// 每次 ShadowPass 对应一个 cascade，通过 CPU 切换 FBO + 设置 u_LightSpaceMatrix。

#type vertex
#version 430 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_LightSpaceMatrix;  // cascade 的 view * ortho
uniform mat4 u_Transform;         // 物体世界矩阵

void main() {
    gl_Position = u_LightSpaceMatrix * u_Transform * vec4(a_Position, 1.0);
}

#type fragment
#version 430 core

// 空 Fragment Shader：OpenGL 自动写入 gl_FragDepth。
// 不需要任何颜色输出，draw buffer 已设置为 GL_NONE。

void main() {}
