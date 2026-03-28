#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "core/Ref.h"
#include "graphics/ComputePipeline.h"

namespace GLRenderer {

namespace ECS { struct FluidComponent; }

// FluidSimulation：基于 PBF（Position-Based Fluids）的 GPU 流体求解器。
//
// Phase 11-B：SSBO 分配 + integrate.glsl（外力积分 + 预测位置）
// Phase 11-C：空间哈希 7-Dispatch + neighbor_search.glsl
// Phase 11-D：密度约束迭代（lambda / delta_pos）
// Phase 11-E：XSPH 粘性 + 涡旋约束 + ECS 跨类型耦合
class FluidSimulation : public RefCounter {
public:
    explicit FluidSimulation(const ECS::FluidComponent& params);
    ~FluidSimulation();

    // 推进一个子步，dt 已由调用方除以 substeps
    void Step(float dt);

    // 平移仿真域和所有粒子，不重置模拟（纯平移时调用，保留速度场）
    void TranslateDomain(glm::vec3 delta);

    // 渲染侧只读接口（Phase 12 零拷贝渲染用）
    GLuint GetPositionSSBO()  const { return m_Buffers.positionSSBO; }
    GLuint GetVelocitySSBO()  const { return m_Buffers.velocitySSBO; }
    int    GetParticleCount() const { return m_ParticleCount; }
    bool   IsReady()          const { return m_Ready; }

    // 空间哈希网格参数（供 Phase 11-C shader 使用）
    glm::ivec3 GetGridDims()  const { return m_GridDims; }
    int        GetCellCount() const { return m_CellCount; }

private:
    void AllocateBuffers();
    void ReleaseBuffers();
    void InitParticles();       // dam-break：在边界左下角填充粒子格子
    void LoadShaders();

    // h=0.04m、r=0.01m 球形邻域立方堆积理论最大邻居 ≈ 33，
    // 64 是安全上界（参考实现 GUI 可调，典型值 64）
    static constexpr int MAX_NEIGHBORS    = 64;
    static constexpr int WORKGROUP_SIZE   = 256;

    // 网格辅助
    static int CeilDiv(int a, int b) { return (a + b - 1) / b; }

    // ─── SSBOs ───────────────────────────────────────────────────────────
    struct Buffers {
        // Particle state（Phase 11-B）
        GLuint positionSSBO   = 0;  // binding 0  vec4[N]  当前位置
        GLuint predictedSSBO  = 0;  // binding 1  vec4[N]  预测位置
        GLuint velocitySSBO   = 0;  // binding 2  vec4[N]

        // Spatial hash（Phase 11-C）
        GLuint cellCountSSBO  = 0;  // binding 3  uint[CELLS]   每格粒子数
        GLuint cellOffsetSSBO = 0;  // binding 4  uint[CELLS]   前缀和偏移
        GLuint sortedIdxSSBO  = 0;  // binding 5  uint[N]       排序后粒子索引
        GLuint blockOffsetSSBO= 0;  // binding 6  uint[BLOCKS]  分层前缀和块偏移

        // Neighbor list（Phase 11-C）
        GLuint neighborCountSSBO = 0;  // binding 7  uint[N]
        GLuint neighborIdxSSBO   = 0;  // binding 8  uint[N × MAX_NEIGHBORS]

        // Constraint solving（Phase 11-D）
        GLuint densitySSBO    = 0;  // binding 9  float[N]
        GLuint constraintSSBO = 0;  // binding 10 float[N]
        GLuint gradSqSumSSBO  = 0;  // binding 11 float[N]
        GLuint lambdaSSBO     = 0;  // binding 12 float[N]
        GLuint deltaPosSSBO   = 0;  // binding 13 vec4[N]
    } m_Buffers;

    // ─── Compute Pipelines ───────────────────────────────────────────────
    Ref<ComputePipeline> m_IntegrateCS;         // Phase 11-B

    // Phase 11-C：空间哈希
    Ref<ComputePipeline> m_HashClearCS;         // 1. 清零 cellCount
    Ref<ComputePipeline> m_HashCountCS;         // 2. 统计每格粒子数
    Ref<ComputePipeline> m_HashPrefixInnerCS;   // 3. 组内前缀和 + blockSum
    Ref<ComputePipeline> m_HashPrefixBlockCS;   // 4. blockSum 前缀和（单线程）
    Ref<ComputePipeline> m_HashPrefixFinalCS;   // 5. 加回块偏移
    Ref<ComputePipeline> m_HashAssignCS;        // 6. 粒子写入 sortedIdx
    Ref<ComputePipeline> m_NeighborSearchCS;    // 7. 27 格邻居搜索

    // Phase 11-D：密度约束
    Ref<ComputePipeline> m_DensityLambdaCS;     // 密度 + λ
    Ref<ComputePipeline> m_DeltaPosCS;          // 计算 Δp
    Ref<ComputePipeline> m_ApplyDeltaCS;        // 应用 Δp + 边界限位
    Ref<ComputePipeline> m_VelocityUpdateCS;    // 速度更新 + 位置确认

    // Phase 11-E：粘性 + 涡旋
    Ref<ComputePipeline> m_XSPHViscosityCS;     // XSPH 速度平滑
    Ref<ComputePipeline> m_VorticityCS;         // 涡旋约束

    // 平移辅助
    Ref<ComputePipeline> m_TranslateCS;         // translate.glsl：平移 positions/predicted

    // ─── Simulation parameters ───────────────────────────────────────────
    int       m_ParticleCount  = 0;
    int       m_MaxParticles   = 0;
    int       m_SolverIters    = 4;
    int       m_Substeps       = 1;
    float     m_ParticleRadius = 0.01f;
    float     m_KernelRadius   = 0.04f;     // = particle_radius * 4
    float     m_RestDensity    = 1000.0f;
    float     m_Mass           = 1.0f;      // 6.4 * r³ * ρ₀
    float     m_MassInverse    = 1.0f;      // 1 / mass
    float     m_Viscosity      = 0.01f;
    float     m_VorticityEps   = 0.0f;
    float     m_Restitution    = 1.0f;      // 边界碰撞弹性系数（1.0 = 完全弹性）
    glm::vec3 m_BoundaryMin    = {-0.5f, 0.0f, -0.5f};
    glm::vec3 m_BoundaryMax    = { 0.5f, 2.0f,  0.5f};

    // ─── Spatial hash grid ───────────────────────────────────────────────
    glm::ivec3 m_GridDims   = {1, 1, 1};  // cells per axis
    int        m_CellCount  = 1;           // = Dx*Dy*Dz
    int        m_BlockSize  = 1;           // = ceil(sqrt(CellCount))，前缀和块大小

    bool m_Ready = false;
};

} // namespace GLRenderer
