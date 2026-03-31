// fluid_color.glsl — Phase 12-F：流体颜色累积 Pass（FluidComponent）
//
// Additive 累积 (waterColor × thickness) 到颜色 FBO。
// FluidShadePass 通过 rgba.a 归一化 rgba.rgb，得到加权平均颜色。

#type vertex
#version 430 core

layout(std430, binding = 0) buffer PositionBuffer { vec4 positions[]; };

uniform mat4  u_View;
uniform mat4  u_Proj;
uniform float u_ParticleRadius;
uniform float u_ViewportH;

out vec3 v_ViewPos;

void main() {
    vec3  worldPos = positions[gl_VertexID].xyz;
    vec4  viewPos4 = u_View * vec4(worldPos, 1.0);
    v_ViewPos      = viewPos4.xyz;
    gl_Position    = u_Proj * viewPos4;

    float eyeZ   = -viewPos4.z;
    gl_PointSize = max(u_ParticleRadius * u_Proj[1][1] * u_ViewportH / eyeZ, 1.0);
}

#type fragment
#version 430 core

layout(location = 0) out vec4 out_WeightedColor;

in vec3 v_ViewPos;

uniform float u_ParticleRadius;
uniform vec3  u_WaterColor;

void main() {
    vec2  uv = vec2(gl_PointCoord.x * 2.0 - 1.0,
                    1.0 - gl_PointCoord.y * 2.0);
    float r2 = dot(uv, uv);
    if (r2 > 1.0) discard;

    float dz        = sqrt(1.0 - r2);
    float thickness = dz * 2.0 * u_ParticleRadius;

    out_WeightedColor = vec4(u_WaterColor * thickness, thickness);
}
