#pragma once

// GBuffer - Deferred Shading 几何缓冲区
//
// 布局设计（带宽优先）：
//
//   Attachment 0  RGBA8         BaseColor.rgb + AO.a
//   Attachment 1  RGBA8         Oct(Normal).rg + Roughness.b + Metallic.a
//   Attachment 2  RGBA8         Emissive.rgb + ShadingModelID.a
//   Attachment 3  R32I          EntityID（editor picking）
//   Depth         DEPTH24S8     几何深度（供 TiledLightCull、SkyboxPass 使用）
//
// 关键决策：
// - Normal 使用 Octahedral Encoding 压入 2×8bit（精度 ~0.4°，足够 PBR 使用）
// - 无专用 Position buffer，从 Depth + InvViewProjection 重建世界坐标
// - ShadingModelID 预留多材质扩展（次表面、发丝、布料等）
//
// 使用流程：
//   gbuffer->Bind();
//   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//   gbuffer->ClearEntityID();
//   // ... 渲染几何体（gbuffer.glsl）...
//   gbuffer->Unbind();
//
//   // Deferred Lighting Pass 绑定各 attachment 为纹理：
//   glBindTextureUnit(0, gbuffer->GetBaseColorAO());
//   glBindTextureUnit(1, gbuffer->GetNormalRoughMetal());
//   glBindTextureUnit(2, gbuffer->GetEmissiveShadingID());
//   glBindTextureUnit(3, gbuffer->GetDepth());

#include "core/Ref.h"
#include "utils/GLCommon.h"
#include <cstdint>

namespace GLRenderer {

struct GBufferSpec {
    uint32_t Width  = 1920;
    uint32_t Height = 1080;
};

class GBuffer : public RefCounter {
public:
    static Ref<GBuffer> Create(const GBufferSpec& spec);
    ~GBuffer();

    // 绑定 FBO 用于写入（GBufferPass）
    void Bind() const;
    void Unbind() const;

    // 清除 EntityID attachment 为 -1（需在 Bind() 之后调用）
    void ClearEntityID(int value = -1) const;

    // 调整大小（重建所有纹理和 FBO）
    void Resize(uint32_t width, uint32_t height);

    // Attachment 0: RGBA8 — BaseColor.rgb + AO.a
    GLuint GetBaseColorAO()       const { return m_TexBaseColorAO; }

    // Attachment 1: RGBA8 — Oct(Normal).rg + Roughness.b + Metallic.a
    GLuint GetNormalRoughMetal()  const { return m_TexNormalRoughMetal; }

    // Attachment 2: RGBA8 — Emissive.rgb + ShadingModelID.a
    GLuint GetEmissiveShadingID() const { return m_TexEmissiveShadingID; }

    // Attachment 3: R32I — EntityID（editor picking）
    GLuint GetEntityID()          const { return m_TexEntityID; }

    // Depth: DEPTH24_STENCIL8 texture（供 TiledLightCull、SkyboxPass blit）
    GLuint GetDepth()             const { return m_TexDepth; }

    GLuint   GetFBO()    const { return m_FBO; }
    uint32_t GetWidth()  const { return m_Spec.Width; }
    uint32_t GetHeight() const { return m_Spec.Height; }
    bool     IsValid()   const { return m_FBO != 0; }

    // 读取指定像素的 EntityID（用于鼠标拾取）
    int ReadEntityID(int x, int y) const;

private:
    explicit GBuffer(const GBufferSpec& spec);

    void Create_();
    void Destroy();

    GBufferSpec m_Spec;

    GLuint m_FBO                 = 0;
    GLuint m_TexBaseColorAO      = 0;   // GL_RGBA8
    GLuint m_TexNormalRoughMetal = 0;   // GL_RGBA8
    GLuint m_TexEmissiveShadingID = 0;  // GL_RGBA8
    GLuint m_TexEntityID         = 0;   // GL_R32I
    GLuint m_TexDepth            = 0;   // GL_DEPTH24_STENCIL8
};

} // namespace GLRenderer
