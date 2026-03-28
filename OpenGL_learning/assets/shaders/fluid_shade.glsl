// fluid_shade.glsl — Phase 12-C：水体着色
//
// 读取流体法线 / 厚度 / 深度，叠加折射 + Fresnel 反射 + Beer-Lambert 吸收，
// 合成后写入 HDR FBO（与场景颜色 alpha 混合）。

#type vertex
#version 430 core

out vec2 v_UV;

void main() {
    vec2 uv     = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    v_UV        = uv;
}

#type fragment
#version 430 core

layout(location = 0) out vec4 out_Color;

in vec2 v_UV;

uniform sampler2D u_FluidNormal;        // 视空间法线（来自 FluidNormalPass）
uniform sampler2D u_FluidDepthSmooth;   // 平滑后球面深度
uniform sampler2D u_FluidThickness;     // 厚度（additive 累积）
uniform sampler2D u_SceneColor;         // 场景 HDR 颜色（流体下方背景）
uniform sampler2D u_SceneDepth;         // 场景深度（区分流体 vs 背景）
uniform samplerCube u_EnvCubemap;       // 环境立方体贴图（反射用）

uniform mat4 u_InvProj;                 // 逆投影矩阵（视空间位置重建）
uniform mat4 u_InvView;                 // 逆视图矩阵（视→世界，反射用）

uniform vec3  u_WaterColor;             // 基础水色（参考 rgb(66,132,244)）
uniform vec3  u_Extinction;             // Beer-Lambert 消光系数（= 1 - WaterColor）
uniform float u_RefractStrength;        // 折射 UV 偏移强度（参考：0.025）
uniform float u_ThicknessScale;         // 厚度缩放（参考：0.05，防止吸收过强）
uniform float u_EnvIntensity;           // 环境贴图亮度缩放

const float F0_WATER = 0.02;           // 水的 Fresnel 反射率（n≈1.33）

void main() {
    vec4 normalSample = texture(u_FluidNormal, v_UV);

    // 无流体：透传场景颜色
    if (normalSample.w < 0.5) {
        out_Color = vec4(texture(u_SceneColor, v_UV).rgb, 1.0);
        return;
    }

    // 场景遮挡：流体深度 >= 场景深度（流体在不透明几何体后面），透传场景颜色
    float fluidDepth = texture(u_FluidDepthSmooth, v_UV).r;
    float sceneDepth = texture(u_SceneDepth, v_UV).r;
    if (fluidDepth >= sceneDepth - 0.0001) {
        out_Color = vec4(texture(u_SceneColor, v_UV).rgb, 1.0);
        return;
    }

    vec3  normal    = normalize(normalSample.xyz);
    float thickness = texture(u_FluidThickness, v_UV).r * u_ThicknessScale;

    // 视方向（视空间，朝向摄像机）
    vec4  ndc          = vec4(v_UV * 2.0 - 1.0, fluidDepth * 2.0 - 1.0, 1.0);
    vec4  viewPos4     = u_InvProj * ndc;
    vec3  viewPos      = viewPos4.xyz / viewPos4.w;
    vec3  viewDir      = normalize(-viewPos);            // 视空间，朝向摄像机（+Z）

    // Fresnel（Schlick 近似）
    float cosTheta = max(dot(normal, viewDir), 0.0);
    float fresnel  = F0_WATER + (1.0 - F0_WATER) * pow(1.0 - cosTheta, 5.0);

    // 折射：偏移 UV 采样背景场景颜色
    vec2  refractUV    = v_UV + normal.xy * u_RefractStrength;
    refractUV          = clamp(refractUV, vec2(0.001), vec2(0.999));

    // 诊断 A：条纹来自法线？→ 取消注释查看 refractUV 偏移量分布
    // 平滑无条纹 = 法线正常；有条纹 = normal.xy 存在周期性误差
    // out_Color = vec4(normal.xy * u_RefractStrength * 40.0 + 0.5, 0.5, 1.0); return;

    // 诊断 B：条纹来自场景内容？→ 取消注释直接查看折射采样结果
    // 有条纹 = 场景颜色在 refractUV 处存在周期性变化
    // out_Color = vec4(texture(u_SceneColor, refractUV).rgb, 1.0); return;

    vec3  refractColor = texture(u_SceneColor, refractUV).rgb;

    // Beer-Lambert 吸收：颜色随厚度指数衰减
    // absorbed → 1（薄处接近透明），→ 0（深处强吸收）
    // u_Extinction 编码水色：蓝水吸收 R/G 更多，B 少 → 呈蓝色
    vec3  absorbed     = exp(-u_Extinction * max(thickness, 0.0));

    // 折射颜色 = 场景颜色 × 吸收率（自然透明：thickness→0 时 absorbed→1）
    refractColor *= absorbed;

    // 反射：视空间法线 → 世界空间 → 采样 cubemap
    vec3  reflectViewDir  = reflect(-viewDir, normal);
    vec3  reflectWorldDir = mat3(u_InvView) * reflectViewDir;
    vec3  reflectColor    = texture(u_EnvCubemap, reflectWorldDir).rgb * u_EnvIntensity;

    // Fresnel 混合（物理正确）：折射+反射，不做额外 alpha 混合
    // refractColor 已含场景颜色，直接输出不透明像素
    vec3 color = mix(refractColor, reflectColor, fresnel);

    out_Color = vec4(color, 1.0);
}
