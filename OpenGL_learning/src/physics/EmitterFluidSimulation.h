#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "core/Ref.h"
#include "graphics/ComputePipeline.h"

namespace GLRenderer {

// EmitterFluidSimulation — 多发射器共享的 PBF 仿真池。
//
// 架构：
//   所有 FluidEmitterComponent 向同一个 EmitterFluidSimulation 注入粒子，
//   粒子在同一哈希网格中互相产生密度约束（物理交互），支持颜色混合（Phase 12-F）。
//   与 FluidComponent 的 FluidSimulation 实例完全隔离，互不影响。
//
// 粒子生命周期：
//   lifetime > 0  → 活跃，每 step 递减 dt
//   lifetime == 0 → 死亡/空闲，跳过积分，顶点着色器剔除，在 freeList 中等待复用
//
// SSBO 布局（binding 0-13 与 FluidSimulation 相同，额外增加 14-17）：
//   0  positions    vec4[N]   当前位置
//   1  predicted    vec4[N]   预测位置
//   2  velocities   vec4[N]
//   3  cellCount    uint[C]
//   4  cellOffset   uint[C]
//   5  sortedIdx    uint[N]
//   6  blockOffset  uint[B]
//   7  neighborCount uint[N]
//   8  neighborIdx  uint[N*64]
//   9  density      float[N]
//   10 constraint   float[N]
//   11 gradSqSum    float[N]
//   12 lambda       float[N]
//   13 deltaPos     vec4[N]
//   14 lifetime     float[N]  生命期（0=死亡）
//   15 freeList     uint[N]   空闲槽位索引栈
//   16 freeCount    uint[1]   栈顶原子计数
//   17 color        vec4[N]   每粒子颜色（emit 写入，Phase 12-F shade 读取）
class EmitterFluidSimulation : public RefCounter {
public:
    EmitterFluidSimulation(int maxParticles,
                           float particleRadius,
                           glm::vec3 boundaryMin,
                           glm::vec3 boundaryMax);
    ~EmitterFluidSimulation();

    // 推进一个子步（dt 已由调用方除以 substeps）
    void Step(float dt);

    // 回收到期粒子（每步 Step 之后调用）
    void Recycle(float dt);

    // 发射粒子（每步 Step 之前调用）
    void Emit(glm::vec3 emitPos, glm::vec3 emitDir,
              int count, float speed, float lifetime,
              float coneAngleRad, glm::vec4 color);

    // 渲染侧只读接口
    GLuint GetPositionSSBO()  const { return m_Buffers.positionSSBO; }
    GLuint GetLifetimeSSBO()  const { return m_Buffers.lifetimeSSBO; }
    GLuint GetColorSSBO()     const { return m_Buffers.colorSSBO; }
    int    GetMaxParticles()  const { return m_MaxParticles; }
    float  GetParticleRadius() const { return m_ParticleRadius; }
    bool   IsReady()          const { return m_Ready; }

    glm::ivec3 GetGridDims()  const { return m_GridDims; }
    int        GetCellCount() const { return m_CellCount; }

    // 场景碰撞 GBuffer（每帧由 PhysicsWorld 注入，传入上一帧 GBuffer 纹理）
    void SetSceneGBuffer(GLuint depthTex, GLuint normalTex,
                         const glm::mat4& viewProj, const glm::mat4& invViewProj);

private:
    void AllocateBuffers();
    void ReleaseBuffers();
    void InitBuffers();
    void LoadShaders();
    void BindAllSSBOs();

    static constexpr int MAX_NEIGHBORS  = 64;
    static constexpr int WORKGROUP_SIZE = 256;
    static int CeilDiv(int a, int b) { return (a + b - 1) / b; }

    struct Buffers {
        // PBF state（binding 0-13，与 FluidSimulation 相同布局）
        GLuint positionSSBO    = 0;  // 0
        GLuint predictedSSBO   = 0;  // 1
        GLuint velocitySSBO    = 0;  // 2
        GLuint cellCountSSBO   = 0;  // 3
        GLuint cellOffsetSSBO  = 0;  // 4
        GLuint sortedIdxSSBO   = 0;  // 5
        GLuint blockOffsetSSBO = 0;  // 6
        GLuint neighborCountSSBO = 0; // 7
        GLuint neighborIdxSSBO   = 0; // 8
        GLuint densitySSBO     = 0;  // 9
        GLuint constraintSSBO  = 0;  // 10
        GLuint gradSqSumSSBO   = 0;  // 11
        GLuint lambdaSSBO      = 0;  // 12
        GLuint deltaPosSSBO    = 0;  // 13
        // Emitter-specific（binding 14-17）
        GLuint lifetimeSSBO    = 0;  // 14
        GLuint freeListSSBO    = 0;  // 15
        GLuint freeCountSSBO   = 0;  // 16
        GLuint colorSSBO       = 0;  // 17
    } m_Buffers;

    // PBF compute pipelines（复用现有 shader 路径，与 FluidSimulation 相同）
    Ref<ComputePipeline> m_IntegrateCS;
    Ref<ComputePipeline> m_HashClearCS;
    Ref<ComputePipeline> m_HashCountCS;
    Ref<ComputePipeline> m_HashPrefixInnerCS;
    Ref<ComputePipeline> m_HashPrefixBlockCS;
    Ref<ComputePipeline> m_HashPrefixFinalCS;
    Ref<ComputePipeline> m_HashAssignCS;
    Ref<ComputePipeline> m_NeighborSearchCS;
    Ref<ComputePipeline> m_DensityLambdaCS;
    Ref<ComputePipeline> m_DeltaPosCS;
    Ref<ComputePipeline> m_ApplyDeltaCS;
    Ref<ComputePipeline> m_VelocityUpdateCS;
    Ref<ComputePipeline> m_XSPHViscosityCS;
    Ref<ComputePipeline> m_VorticityCS;
    // Emitter-specific
    Ref<ComputePipeline> m_EmitCS;
    Ref<ComputePipeline> m_RecycleCS;
    Ref<ComputePipeline> m_SceneCollisionCS;  // Phase 12-G

    int       m_MaxParticles   = 0;
    int       m_SolverIters    = 3;
    float     m_ParticleRadius = 0.01f;
    float     m_KernelRadius   = 0.04f;
    float     m_RestDensity    = 1000.0f;
    float     m_Mass           = 1.0f;
    float     m_MassInverse    = 1.0f;
    float     m_Viscosity      = 0.02f;
    float     m_VorticityEps   = 0.0f;
    float     m_Restitution    = 0.0f;
    glm::vec3 m_BoundaryMin    = {-2.0f, 0.0f, -2.0f};
    glm::vec3 m_BoundaryMax    = { 2.0f, 4.0f,  2.0f};

    glm::ivec3 m_GridDims  = {1, 1, 1};
    int        m_CellCount = 1;
    int        m_BlockSize = 1;

    uint32_t m_FrameSeed = 0;   // 每次 Emit 调用后递增，保证伪随机种子不重复

    // Scene collision GBuffer（来自上一帧，Phase 12-G）
    GLuint    m_GDepthTex    = 0;
    GLuint    m_GNormalTex   = 0;
    glm::mat4 m_GViewProj    = glm::mat4(1.0f);
    glm::mat4 m_GInvViewProj = glm::mat4(1.0f);

    bool m_Ready = false;
};

} // namespace GLRenderer
