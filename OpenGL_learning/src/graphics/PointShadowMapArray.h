#pragma once

// PointShadowMapArray - 点光源全向阴影贴图数组
//
// 使用 GL_TEXTURE_CUBE_MAP_ARRAY 将多个点光源的 Cubemap 深度纹理合并到一个对象中，
// PBR shader 只需一个 samplerCubeArray 绑定即可动态索引任意光源的阴影。
//
// 深度值存储为归一化线性距离：depth = length(fragPos - lightPos) / farPlane
// PBR shader 用 samplerCubeArray 手动比较（不使用 samplerCubeShadow）。
//
// 使用流程：
//   auto arr = PointShadowMapArray::Create({ .Resolution=512, .Count=4 });
//   for (int slot = 0; slot < 4; slot++)
//       for (int face = 0; face < 6; face++) {
//           arr->BindFaceForWriting(slot, face);
//           glViewport(0,0,512,512); glClear(GL_DEPTH_BUFFER_BIT);
//           // ... 渲染几何体 ...
//       }
//   arr->BindForReading(11);  // 绑定到纹理槽 11
//   shader.SetInt("u_PointShadowMaps", 11);
//   // shader 中: texture(u_PointShadowMaps, vec4(dir, float(slot)))

#include "core/Ref.h"
#include "utils/GLCommon.h"
#include <vector>
#include <cstdint>

namespace GLRenderer {

class PointShadowMapArray : public RefCounter {
public:
    struct Config {
        uint32_t Resolution = 512;
        uint32_t Count      = 4;   // MAX_SHADOW_POINT_LIGHTS
    };

    static Ref<PointShadowMapArray> Create(const Config& config = Config());
    ~PointShadowMapArray();

    // 绑定第 lightSlot 个 Cubemap 的第 faceIndex（0-5）面用于深度写入
    void BindFaceForWriting(uint32_t lightSlot, uint32_t faceIndex);

    // 绑定 CubemapArray 到指定纹理槽，供 PBR shader 采样
    void BindForReading(uint32_t slot) const;

    void Unbind() const;

    GLuint   GetTextureID()  const { return m_CubemapArray; }
    uint32_t GetResolution() const { return m_Config.Resolution; }
    uint32_t GetCount()      const { return m_Config.Count; }
    bool     IsValid()       const { return m_CubemapArray != 0; }

private:
    explicit PointShadowMapArray(const Config& config);
    void Create_();

    Config              m_Config;
    GLuint              m_CubemapArray = 0;
    std::vector<GLuint> m_FBOs;   // 大小 = Count * 6，索引 = lightSlot * 6 + faceIndex
};

} // namespace GLRenderer
