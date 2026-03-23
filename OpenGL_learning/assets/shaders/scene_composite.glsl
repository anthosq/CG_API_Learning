// Scene Composite - Tone Mapping + Gamma Correction
// 参考 Hazel Engine SceneComposite.glsl
// 流程: HDR linear → Exposure → ACES Tonemap → Gamma 2.2

#type vertex
#version 430 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position.xy, 0.0, 1.0);
}

#type fragment
#version 430 core

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int  o_EntityID;

in vec2 v_TexCoord;

uniform sampler2D  u_Texture;      // HDR color attachment
uniform isampler2D u_EntityIDTex;  // HDR EntityID attachment（原样复制到 viewport FBO）
uniform float      u_Exposure;

// Bloom（intensity=0 表示禁用，无需额外 bool）
uniform sampler2D  u_BloomTex;
uniform float      u_BloomIntensity;

uniform int   u_SelectedEntityID;  // -1 = 无选中
uniform vec4  u_OutlineColor;      // rgba，alpha 控制混合强度
uniform float u_OutlineWidth;      // 像素宽度

// ACES Filmic Tonemapper
// 来源: http://www.oscars.org/science-technology/sci-tech-projects/aces
// 与 Hazel SceneComposite.glsl 完全一致
vec3 ACESTonemap(vec3 color) {
    mat3 m1 = mat3(
        0.59719, 0.07600, 0.02840,
        0.35458, 0.90834, 0.13383,
        0.04823, 0.01566, 0.83777
    );
    mat3 m2 = mat3(
        1.60475, -0.10208, -0.00327,
       -0.53108,  1.10813, -0.07276,
       -0.07367, -0.00605,  1.07602
    );
    vec3 v = m1 * color;
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return clamp(m2 * (a / b), 0.0, 1.0);
}

void main() {
    vec3 color = texture(u_Texture, v_TexCoord).rgb;

    // 曝光调整
    color *= u_Exposure;

    // Bloom 叠加（在 Tonemap 之前；intensity=0 时无贡献）
    color += texture(u_BloomTex, v_TexCoord).rgb * u_BloomIntensity;

    // ACES Tone Mapping: HDR linear → [0, 1]
    color = ACESTonemap(color);

    // Gamma 校正: linear → sRGB（显示器期望的编码）
    color = pow(color, vec3(1.0 / 2.2));

    o_Color = vec4(color, 1.0);

    // 将几何体 EntityID 从 HDR FBO 复制到 viewport FBO attachment 1
    // billboard 后续渲染会在其像素位置覆盖写入，最终 viewport FBO 包含所有 EntityID
    ivec2 texelCoord = ivec2(v_TexCoord * vec2(textureSize(u_EntityIDTex, 0)));
    int   thisID     = texelFetch(u_EntityIDTex, texelCoord, 0).r;
    o_EntityID = thisID;

    // 描边：对选中实体做 EntityID 边缘检测
    if (u_SelectedEntityID >= 0) {
        ivec2 texSize = textureSize(u_EntityIDTex, 0) - ivec2(1);
        int   w = int(u_OutlineWidth);
        bool  isEdge  = false;
        for (int dx = -w; dx <= w && !isEdge; dx++) {
            for (int dy = -w; dy <= w && !isEdge; dy++) {
                if (dx == 0 && dy == 0) continue;
                ivec2 nc = clamp(texelCoord + ivec2(dx, dy), ivec2(0), texSize);
                if (texelFetch(u_EntityIDTex, nc, 0).r == u_SelectedEntityID
                    && thisID != u_SelectedEntityID)
                    isEdge = true;
            }
        }
        if (isEdge)
            o_Color.rgb = mix(o_Color.rgb, u_OutlineColor.rgb, u_OutlineColor.a);
    }
}
