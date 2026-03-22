#pragma once

// ShadowMap - 级联阴影贴图（CSM）资源封装
//
// 内部持有：
//   - GL_TEXTURE_2D_ARRAY：N 层深度纹理（每层一个 cascade）
//   - N 个 FBO：每个附加纹理数组的对应层
//
// 使用流程：
//   auto sm = ShadowMap::Create({ .Resolution=2048, .CascadeCount=4 });
//
//   // Shadow Pass（CPU 端按 cascade 循环）：
//   sm->BindForWriting(i);
//   glViewport(0,0,2048,2048); glClear(GL_DEPTH_BUFFER_BIT);
//   // ... 渲染几何体 ...
//
//   // Geometry Pass（绑定给 PBR shader）：
//   sm->BindForReading(9);   // slot=9
//   shader.SetInt("u_ShadowMap", 9);

#include "core/Ref.h"
#include "utils/GLCommon.h"
#include <vector>
#include <cstdint>

namespace GLRenderer {

class ShadowMap : public RefCounter {
public:
    struct Config {
        uint32_t Resolution   = 2048;   // 每个 cascade 的宽/高（正方形）
        uint32_t CascadeCount = 4;      // cascade 层数
    };

    static Ref<ShadowMap> Create(const Config& config = Config());
    ~ShadowMap();

    // 绑定第 cascadeIndex 层 FBO 用于深度写入
    // 调用后需自行 glViewport + glClear(GL_DEPTH_BUFFER_BIT)
    void BindForWriting(uint32_t cascadeIndex);

    // 绑定纹理数组到指定纹理槽，供 PBR shader 采样
    void BindForReading(uint32_t slot) const;

    // 解绑 FBO（恢复为 0）
    void Unbind() const;

    uint32_t GetTextureID()    const { return m_TextureArray; }
    uint32_t GetResolution()   const { return m_Config.Resolution; }
    uint32_t GetCascadeCount() const { return m_Config.CascadeCount; }

    bool IsValid() const { return m_TextureArray != 0; }

private:
    explicit ShadowMap(const Config& config);
    void Create_();

    Config              m_Config;
    GLuint              m_TextureArray = 0;
    std::vector<GLuint> m_FBOs;         // m_FBOs[i] 对应第 i 个 cascade
};

} // namespace GLRenderer
