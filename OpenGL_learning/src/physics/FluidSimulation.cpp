#include "FluidSimulation.h"
#include "scene/ecs/Components.h"

#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>

namespace GLRenderer {

FluidSimulation::FluidSimulation(const ECS::FluidComponent& params)
    : m_MaxParticles  (params.MaxParticles)
    , m_SolverIters   (params.SolverIters)
    , m_Substeps      (params.Substeps)
    , m_ParticleRadius(params.ParticleRadius)
    , m_KernelRadius  (params.ParticleRadius * 4.0f)
    , m_RestDensity   (params.RestDensity)
    , m_Viscosity     (params.Viscosity)
    , m_VorticityEps  (params.VorticityEps)
    , m_BoundaryMin      (params.BoundaryMin)
    , m_BoundaryMax      (params.BoundaryMax)
    , m_SceneRestitution (params.SceneRestitution)
{
    // 粒子质量 m = 6.4 * r³ * ρ₀
    const float r3 = m_ParticleRadius * m_ParticleRadius * m_ParticleRadius;
    m_Mass        = 6.4f * r3 * m_RestDensity;
    m_MassInverse = 1.0f / m_Mass;

    // 空间哈希网格尺寸（每格边长 = kernel radius）
    glm::vec3 span  = m_BoundaryMax - m_BoundaryMin;
    m_GridDims.x    = std::max(1, (int)std::ceil(span.x / m_KernelRadius));
    m_GridDims.y    = std::max(1, (int)std::ceil(span.y / m_KernelRadius));
    m_GridDims.z    = std::max(1, (int)std::ceil(span.z / m_KernelRadius));
    m_CellCount  = m_GridDims.x * m_GridDims.y * m_GridDims.z;
    m_BlockSize  = (int)std::ceil(std::sqrt((float)m_CellCount));

    AllocateBuffers();
    LoadShaders();
    InitParticles();

    std::cout << "[FluidSimulation] Ready — N=" << m_ParticleCount
              << " grid=" << m_GridDims.x << "x" << m_GridDims.y << "x" << m_GridDims.z
              << " cells=" << m_CellCount << std::endl;
    m_Ready = true;
}

FluidSimulation::~FluidSimulation() {
    ReleaseBuffers();
    std::cout << "[FluidSimulation] Destroyed" << std::endl;
}

void FluidSimulation::AllocateBuffers() {
    const int N          = m_MaxParticles;
    const int CELLS      = m_CellCount;
    const int NEIGHBORS  = N * MAX_NEIGHBORS;

    // 一次性生成所有 SSBO
    GLuint all[14];
    glGenBuffers(14, all);
    m_Buffers.positionSSBO    = all[0];
    m_Buffers.predictedSSBO   = all[1];
    m_Buffers.velocitySSBO    = all[2];
    m_Buffers.cellCountSSBO   = all[3];
    m_Buffers.cellOffsetSSBO  = all[4];
    m_Buffers.sortedIdxSSBO   = all[5];
    m_Buffers.blockOffsetSSBO = all[6];
    m_Buffers.neighborCountSSBO = all[7];
    m_Buffers.neighborIdxSSBO   = all[8];
    m_Buffers.densitySSBO     = all[9];
    m_Buffers.constraintSSBO  = all[10];
    m_Buffers.gradSqSumSSBO   = all[11];
    m_Buffers.lambdaSSBO      = all[12];
    m_Buffers.deltaPosSSBO    = all[13];

    // 分配 GPU 内存（vec4 = 16 bytes, uint = 4 bytes, float = 4 bytes）
    auto AllocVec4 = [](GLuint id, int count) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(count * 4 * sizeof(float)), nullptr, GL_DYNAMIC_DRAW);
    };
    auto AllocUint = [](GLuint id, int count) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(count * sizeof(uint32_t)), nullptr, GL_DYNAMIC_DRAW);
    };
    auto AllocFloat = [](GLuint id, int count) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(count * sizeof(float)), nullptr, GL_DYNAMIC_DRAW);
    };

    AllocVec4 (m_Buffers.positionSSBO,      N);
    AllocVec4 (m_Buffers.predictedSSBO,     N);
    AllocVec4 (m_Buffers.velocitySSBO,      N);
    AllocUint (m_Buffers.cellCountSSBO,     CELLS);
    AllocUint (m_Buffers.cellOffsetSSBO,    CELLS);
    AllocUint (m_Buffers.sortedIdxSSBO,     N);
    AllocUint (m_Buffers.blockOffsetSSBO,   m_BlockSize);
    AllocUint (m_Buffers.neighborCountSSBO, N);
    AllocUint (m_Buffers.neighborIdxSSBO,   NEIGHBORS);
    AllocFloat(m_Buffers.densitySSBO,       N);
    AllocFloat(m_Buffers.constraintSSBO,    N);
    AllocFloat(m_Buffers.gradSqSumSSBO,     N);
    AllocFloat(m_Buffers.lambdaSSBO,        N);
    AllocVec4 (m_Buffers.deltaPosSSBO,      N);

    // 全局绑定点（persistent binding，shader 无需每次重绑）
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0,  m_Buffers.positionSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1,  m_Buffers.predictedSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2,  m_Buffers.velocitySSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3,  m_Buffers.cellCountSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4,  m_Buffers.cellOffsetSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5,  m_Buffers.sortedIdxSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6,  m_Buffers.blockOffsetSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7,  m_Buffers.neighborCountSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8,  m_Buffers.neighborIdxSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9,  m_Buffers.densitySSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, m_Buffers.constraintSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, m_Buffers.gradSqSumSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, m_Buffers.lambdaSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 13, m_Buffers.deltaPosSSBO);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void FluidSimulation::ReleaseBuffers() {
    GLuint all[] = {
        m_Buffers.positionSSBO,    m_Buffers.predictedSSBO,    m_Buffers.velocitySSBO,
        m_Buffers.cellCountSSBO,   m_Buffers.cellOffsetSSBO,   m_Buffers.sortedIdxSSBO,
        m_Buffers.blockOffsetSSBO, m_Buffers.neighborCountSSBO, m_Buffers.neighborIdxSSBO,
        m_Buffers.densitySSBO,     m_Buffers.constraintSSBO,   m_Buffers.gradSqSumSSBO,
        m_Buffers.lambdaSSBO,      m_Buffers.deltaPosSSBO
    };
    glDeleteBuffers(static_cast<GLsizei>(std::size(all)), all);
}

// InitParticles — dam-break：在物理有效域内填充 90% 高度
//
// 关键修复：fillMin/fillMax 的边距必须与 apply_delta / velocity_update 一致，
// 即 padding = 2.5 * KernelRadius，而非 diameter。
//
// 原先用 diameter(=0.02m) 作边距，但物理边界是 2.5*h(=0.10m)，
// 导致初始化的粒子约 40% 落在物理无效区，第一帧被强制堆压到边界，
// 产生极端高密度层，4次迭代无法收敛 → 水位远低于理论值。
//
// 修复后填充有效物理域的 90%，平衡水位 ≈ 72% 有效域高。

void FluidSimulation::InitParticles() {
    const float diameter = 2.0f * m_ParticleRadius;
    const float padding  = m_KernelRadius * 2.5f;   // 与 apply_delta 保持一致
    const glm::vec3 span = m_BoundaryMax - m_BoundaryMin;
    const float validH   = span.y - 2.0f * padding; // 物理有效高度

    glm::vec3 fillMin = m_BoundaryMin + glm::vec3(padding);
    glm::vec3 fillMax = glm::vec3(
        m_BoundaryMax.x - padding,
        fillMin.y + validH * 0.75f,
        m_BoundaryMax.z - padding
    );

    std::vector<glm::vec4> positions;
    positions.reserve(m_MaxParticles);

    for (float x = fillMin.x; x <= fillMax.x && (int)positions.size() < m_MaxParticles; x += diameter) {
        for (float y = fillMin.y; y <= fillMax.y && (int)positions.size() < m_MaxParticles; y += diameter) {
            for (float z = fillMin.z; z <= fillMax.z && (int)positions.size() < m_MaxParticles; z += diameter) {
                positions.push_back(glm::vec4(x, y, z, 0.0f));
            }
        }
    }

    m_ParticleCount = static_cast<int>(positions.size());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffers.positionSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    (GLsizeiptr)(m_ParticleCount * sizeof(glm::vec4)),
                    positions.data());

    // 速度初始化为零（glBufferData 时已清零，但显式写一遍更清晰）
    std::vector<glm::vec4> zeros(m_ParticleCount, glm::vec4(0.0f));
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffers.velocitySSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    (GLsizeiptr)(m_ParticleCount * sizeof(glm::vec4)),
                    zeros.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::cout << "[FluidSimulation] InitParticles: " << m_ParticleCount << " particles" << std::endl;
}

void FluidSimulation::LoadShaders() {
    auto Load = [](const char* path) -> Ref<ComputePipeline> {
        auto cs = ComputePipeline::Create(path);
        if (!cs || !cs->IsValid())
            std::cerr << "[FluidSimulation] Failed to load: " << path << std::endl;
        return cs;
    };

    m_IntegrateCS        = Load("assets/shaders/physics/integrate.glsl");
    m_HashClearCS        = Load("assets/shaders/physics/hash_clear.glsl");
    m_HashCountCS        = Load("assets/shaders/physics/hash_count.glsl");
    m_HashPrefixInnerCS  = Load("assets/shaders/physics/hash_prefix_inner.glsl");
    m_HashPrefixBlockCS  = Load("assets/shaders/physics/hash_prefix_block.glsl");
    m_HashPrefixFinalCS  = Load("assets/shaders/physics/hash_prefix_final.glsl");
    m_HashAssignCS       = Load("assets/shaders/physics/hash_assign.glsl");
    m_NeighborSearchCS   = Load("assets/shaders/physics/neighbor_search.glsl");
    m_DensityLambdaCS    = Load("assets/shaders/physics/density_lambda.glsl");
    m_DeltaPosCS         = Load("assets/shaders/physics/delta_pos.glsl");
    m_ApplyDeltaCS       = Load("assets/shaders/physics/apply_delta.glsl");
    m_VelocityUpdateCS   = Load("assets/shaders/physics/velocity_update.glsl");
    m_XSPHViscosityCS    = Load("assets/shaders/physics/xsph_viscosity.glsl");
    m_VorticityCS        = Load("assets/shaders/physics/vorticity.glsl");
    m_TranslateCS        = Load("assets/shaders/physics/translate.glsl");
    m_SceneCollisionCS   = Load("assets/shaders/physics/scene_collision.glsl");
}

void FluidSimulation::Step(float dt) {
    if (!m_Ready || m_ParticleCount == 0) return;

    // 每帧重新绑定：渲染管线（Tiled Forward+、FluidDepthPass 等）会覆盖
    // binding 0/1/2，导致 integrate/hash/density 读到点光源或 tile 索引数据。
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0,  m_Buffers.positionSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1,  m_Buffers.predictedSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2,  m_Buffers.velocitySSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3,  m_Buffers.cellCountSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4,  m_Buffers.cellOffsetSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5,  m_Buffers.sortedIdxSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6,  m_Buffers.blockOffsetSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7,  m_Buffers.neighborCountSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8,  m_Buffers.neighborIdxSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9,  m_Buffers.densitySSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, m_Buffers.constraintSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, m_Buffers.gradSqSumSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, m_Buffers.lambdaSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 13, m_Buffers.deltaPosSSBO);

    const uint32_t pGroups  = (uint32_t)CeilDiv(m_ParticleCount, WORKGROUP_SIZE);
    const uint32_t cGroups  = (uint32_t)CeilDiv(m_CellCount,     WORKGROUP_SIZE);
    const uint32_t bGroups  = (uint32_t)CeilDiv(m_BlockSize,     WORKGROUP_SIZE);
    const uint32_t N        = (uint32_t)m_ParticleCount;
    const uint32_t CELLS    = (uint32_t)m_CellCount;
    const uint32_t BSIZE    = (uint32_t)m_BlockSize;

    // ① 外力积分 + 预测位置
    {
        m_IntegrateCS->Bind();
        m_IntegrateCS->SetVec3 ("u_Gravity",      glm::vec3(0.0f, -9.8f, 0.0f));
        m_IntegrateCS->SetFloat("u_DeltaTime",     dt);
        m_IntegrateCS->SetUint ("u_ParticleCount", N);
        m_IntegrateCS->Dispatch(pGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // ①-b 场景碰撞（Phase 12-G）：GBuffer 深度+法线，推出穿入几何体的粒子
    if (m_SceneCollisionCS && m_GDepthTex != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_GDepthTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_GNormalTex);

        m_SceneCollisionCS->Bind();
        m_SceneCollisionCS->SetMat4 ("u_ViewProj",       m_GViewProj);
        m_SceneCollisionCS->SetMat4 ("u_InvViewProj",    m_GInvViewProj);
        m_SceneCollisionCS->SetInt  ("u_GDepth",         0);
        m_SceneCollisionCS->SetInt  ("u_GNormal",        1);
        m_SceneCollisionCS->SetInt  ("u_HasNormalTex",   m_GNormalTex != 0 ? 1 : 0);
        m_SceneCollisionCS->SetFloat("u_ParticleRadius", m_ParticleRadius);
        m_SceneCollisionCS->SetUint ("u_ParticleCount",  N);
        m_SceneCollisionCS->SetFloat("u_Restitution",    m_SceneRestitution);
        m_SceneCollisionCS->SetInt  ("u_HasLifetime",    0);
        m_SceneCollisionCS->Dispatch(pGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);

        glActiveTexture(GL_TEXTURE0);
    }

    // ② 空间哈希 — 6 dispatch 重建 cellOffset[] + sortedIdx[]
    {
        // 2-1. 清零每格计数
        m_HashClearCS->Bind();
        m_HashClearCS->SetUint("u_CellCount", CELLS);
        m_HashClearCS->Dispatch(cGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);

        // 2-2. 统计每格粒子数（atomic）
        m_HashCountCS->Bind();
        m_HashCountCS->SetVec3 ("u_BoundaryMin",  m_BoundaryMin);
        m_HashCountCS->SetFloat("u_KernelRadius", m_KernelRadius);
        glUniform3i(glGetUniformLocation(m_HashCountCS->GetID(), "u_GridDims"),
                    m_GridDims.x, m_GridDims.y, m_GridDims.z);
        m_HashCountCS->SetUint("u_ParticleCount", N);
        m_HashCountCS->Dispatch(pGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);

        // 2-3. 组内前缀和 + blockSum
        m_HashPrefixInnerCS->Bind();
        m_HashPrefixInnerCS->SetUint("u_BlockSize", BSIZE);
        m_HashPrefixInnerCS->SetUint("u_CellCount", CELLS);
        m_HashPrefixInnerCS->Dispatch(bGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);

        // 2-4. blockSum 前缀和（单线程）
        m_HashPrefixBlockCS->Bind();
        m_HashPrefixBlockCS->SetUint("u_BlockSize", BSIZE);
        m_HashPrefixBlockCS->Dispatch(1, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);

        // 2-5. 加回块偏移
        m_HashPrefixFinalCS->Bind();
        m_HashPrefixFinalCS->SetUint("u_BlockSize", BSIZE);
        m_HashPrefixFinalCS->SetUint("u_CellCount", CELLS);
        m_HashPrefixFinalCS->Dispatch(bGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);

        // 2-6. 粒子写入 sortedIdx（使用 cellOffset，副作用：cellOffset 被递增到段末尾）
        m_HashAssignCS->Bind();
        m_HashAssignCS->SetVec3 ("u_BoundaryMin",  m_BoundaryMin);
        m_HashAssignCS->SetFloat("u_KernelRadius", m_KernelRadius);
        glUniform3i(glGetUniformLocation(m_HashAssignCS->GetID(), "u_GridDims"),
                    m_GridDims.x, m_GridDims.y, m_GridDims.z);
        m_HashAssignCS->SetUint("u_ParticleCount", N);
        m_HashAssignCS->Dispatch(pGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // ③ 邻居搜索
    {
        m_NeighborSearchCS->Bind();
        m_NeighborSearchCS->SetVec3 ("u_BoundaryMin",  m_BoundaryMin);
        m_NeighborSearchCS->SetFloat("u_KernelRadius", m_KernelRadius);
        glUniform3i(glGetUniformLocation(m_NeighborSearchCS->GetID(), "u_GridDims"),
                    m_GridDims.x, m_GridDims.y, m_GridDims.z);
        m_NeighborSearchCS->SetUint("u_ParticleCount",  N);
        m_NeighborSearchCS->SetUint("u_MaxNeighbors",   (uint32_t)MAX_NEIGHBORS);
        m_NeighborSearchCS->Dispatch(pGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // ④ 密度约束迭代
    for (int iter = 0; iter < m_SolverIters; iter++) {
        // ④-a：密度 + λ
        {
            m_DensityLambdaCS->Bind();
            m_DensityLambdaCS->SetFloat("u_KernelRadius",   m_KernelRadius);
            m_DensityLambdaCS->SetFloat("u_RestDensity",    m_RestDensity);
            m_DensityLambdaCS->SetFloat("u_RelaxationEps",  1e-6f);
            m_DensityLambdaCS->SetFloat("u_Mass",           m_Mass);
            m_DensityLambdaCS->SetUint ("u_ParticleCount",  N);
            m_DensityLambdaCS->SetUint ("u_MaxNeighbors",   (uint32_t)MAX_NEIGHBORS);
            m_DensityLambdaCS->Dispatch(pGroups, 1, 1);
            ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);
        }

        // ④-b：Δp 计算
        {
            m_DeltaPosCS->Bind();
            m_DeltaPosCS->SetFloat("u_KernelRadius",   m_KernelRadius);
            m_DeltaPosCS->SetFloat("u_RestDensity",    m_RestDensity);
            m_DeltaPosCS->SetFloat("u_Mass",           m_Mass);
            m_DeltaPosCS->SetFloat("u_ScorrK",         0.0f);
            m_DeltaPosCS->SetFloat("u_ScorrDeltaQ",    0.3f);
            m_DeltaPosCS->SetUint ("u_ParticleCount",  N);
            m_DeltaPosCS->SetUint ("u_MaxNeighbors",   (uint32_t)MAX_NEIGHBORS);
            m_DeltaPosCS->Dispatch(pGroups, 1, 1);
            ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);
        }

        // ④-c：应用 Δp + 边界限位
        // 边界 padding = 2.5 * h，确保边界粒子有完整 kernel 支持
        {
            m_ApplyDeltaCS->Bind();
            m_ApplyDeltaCS->SetVec3 ("u_BoundaryMin",      m_BoundaryMin);
            m_ApplyDeltaCS->SetVec3 ("u_BoundaryMax",      m_BoundaryMax);
            m_ApplyDeltaCS->SetFloat("u_BoundaryPadding",  m_KernelRadius * 2.5f);
            m_ApplyDeltaCS->SetUint ("u_ParticleCount",    N);
            m_ApplyDeltaCS->Dispatch(pGroups, 1, 1);
            ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);
        }
    }

    // ⑤ 速度更新 + 位置确认
    {
        m_VelocityUpdateCS->Bind();
        m_VelocityUpdateCS->SetFloat("u_DeltaTime",       dt);
        m_VelocityUpdateCS->SetUint ("u_ParticleCount",   N);
        m_VelocityUpdateCS->SetVec3 ("u_BoundaryMin",     m_BoundaryMin);
        m_VelocityUpdateCS->SetVec3 ("u_BoundaryMax",     m_BoundaryMax);
        m_VelocityUpdateCS->SetFloat("u_BoundaryPadding", m_KernelRadius * 2.5f);
        m_VelocityUpdateCS->SetFloat("u_Restitution",     m_Restitution);
        m_VelocityUpdateCS->Dispatch(pGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // ⑥ XSPH 粘性（velocity 平滑）
    if (m_Viscosity > 0.0f) {
        m_XSPHViscosityCS->Bind();
        m_XSPHViscosityCS->SetFloat("u_KernelRadius",  m_KernelRadius);
        m_XSPHViscosityCS->SetFloat("u_Viscosity",     m_Viscosity);
        m_XSPHViscosityCS->SetFloat("u_RestDensity",   m_RestDensity);
        m_XSPHViscosityCS->SetUint ("u_ParticleCount", N);
        m_XSPHViscosityCS->SetUint ("u_MaxNeighbors",  (uint32_t)MAX_NEIGHBORS);
        m_XSPHViscosityCS->Dispatch(pGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // ⑦ 涡旋约束
    if (m_VorticityEps > 0.0f) {
        m_VorticityCS->Bind();
        m_VorticityCS->SetFloat("u_KernelRadius",  m_KernelRadius);
        m_VorticityCS->SetFloat("u_VorticityEps",  m_VorticityEps);
        m_VorticityCS->SetFloat("u_DeltaTime",     dt);
        m_VorticityCS->SetUint ("u_ParticleCount", N);
        m_VorticityCS->SetUint ("u_MaxNeighbors",  (uint32_t)MAX_NEIGHBORS);
        m_VorticityCS->Dispatch(pGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);
    }
}

void FluidSimulation::TranslateDomain(glm::vec3 delta, float frameDt) {
    if (!m_Ready || m_ParticleCount == 0 || !m_TranslateCS) return;

    m_BoundaryMin += delta;
    m_BoundaryMax += delta;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_Buffers.positionSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_Buffers.predictedSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_Buffers.velocitySSBO);

    // 惯性冲量：容器以 delta/frameDt 速度运动，流体因惯性获得反向速度 → 晃动
    const glm::vec3 impulse = (frameDt > 1e-6f) ? (-delta / frameDt) : glm::vec3(0.0f);

    m_TranslateCS->Bind();
    m_TranslateCS->SetVec3("u_Delta",           delta);
    m_TranslateCS->SetVec3("u_InertialImpulse", impulse);
    m_TranslateCS->SetUint("u_ParticleCount", static_cast<uint32_t>(m_ParticleCount));
    m_TranslateCS->Dispatch(CeilDiv(m_ParticleCount, WORKGROUP_SIZE), 1, 1);
    ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);
}

void FluidSimulation::SetSceneGBuffer(GLuint depthTex, GLuint normalTex,
                                       const glm::mat4& viewProj,
                                       const glm::mat4& invViewProj) {
    m_GDepthTex    = depthTex;
    m_GNormalTex   = normalTex;
    m_GViewProj    = viewProj;
    m_GInvViewProj = invViewProj;
}

} // namespace GLRenderer
