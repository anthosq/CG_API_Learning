#pragma once

#include "graphics/Buffer.h"
#include "graphics/ComputePipeline.h"
#include "renderer/RenderTypes.h"
#include "core/Ref.h"
#include <glm/glm.hpp>
#include <cstdint>

namespace GLRenderer {

// GPU 粒子（std430，64 bytes/粒子）
struct GPUParticle {
    glm::vec4 Position;   // xyz=世界坐标, w=当前生命(秒)
    glm::vec4 Velocity;   // xyz=速度,     w=总生命(秒)
    glm::vec4 Color;      // rgba HDR
    glm::vec4 Params;     // x=大小, y=旋转(rad), z=归一化年龄[0,1], w=alive(0/1)
};

// GL_DRAW_INDIRECT_BUFFER 格式
struct DrawArraysIndirectCommand {
    uint32_t count;        // 活跃粒子数 * 6（每粒子 2 三角形，6 顶点）
    uint32_t primCount;    // = 1
    uint32_t first;        // = 0
    uint32_t baseInstance; // = 0
};

// 每帧发射请求（CPU→GPU）
struct EmitParams {
    glm::vec4 Position;       // xyz=发射位置, w=unused
    glm::vec4 EmitDirection;  // xyz=主方向, w=unused
    glm::vec4 ColorBegin;
    glm::vec4 ColorEnd;
    glm::vec4 Gravity;        // xyz=重力, w=unused
    float     EmitSpread;     // 半锥角(rad)
    float     EmitRate;       // 粒子/秒
    float     LifetimeMin;
    float     LifetimeMax;
    float     SpeedMin;
    float     SpeedMax;
    float     SizeBegin;
    float     SizeEnd;
    float     DeltaTime;
    int       MaxParticles;
    int       EmitCount;      // 本帧要发射的粒子数
    float     _pad0;
    float     _pad1;
};

class ParticleSystem : public RefCounter {
public:
    ParticleSystem(int maxParticles, uint32_t seed = 12345u);
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    void Simulate(const EmitParams& params,
                  Ref<ComputePipeline> emitPipeline,
                  Ref<ComputePipeline> updatePipeline,
                  Ref<ComputePipeline> compactPipeline);

    void BindForRender() const;

    int  GetMaxParticles() const { return m_MaxParticles; }
    int  GetAliveCount()   const;

    uint32_t GetParticleSSBO()  const { return m_ParticleSSBO; }
    uint32_t GetAliveSSBO()     const { return m_AliveSSBO; }
    uint32_t GetDeadSSBO()      const { return m_DeadSSBO; }
    uint32_t GetCounterSSBO()   const { return m_CounterSSBO; }
    uint32_t GetIndirectBuffer()const { return m_IndirectBuffer; }

private:
    void InitBuffers(uint32_t seed);
    void ResetCounters();

    int      m_MaxParticles = 0;

    uint32_t m_ParticleSSBO   = 0;
    uint32_t m_AliveSSBO      = 0;
    uint32_t m_DeadSSBO       = 0;
    uint32_t m_CounterSSBO    = 0;  // uint[4]: aliveCount, deadCount, emitCount, _pad
    uint32_t m_IndirectBuffer = 0;
    uint32_t m_DummyVAO       = 0;  // Core Profile 要求 draw call 必须有 VAO

    float m_EmitAccumulator = 0.0f;
};

} // namespace GLRenderer
