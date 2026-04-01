#include "EmitterFluidSimulation.h"

#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <numeric>   // std::iota

namespace GLRenderer {

EmitterFluidSimulation::EmitterFluidSimulation(int maxParticles,
                                               float particleRadius,
                                               glm::vec3 boundaryMin,
                                               glm::vec3 boundaryMax)
    : m_MaxParticles  (maxParticles)
    , m_ParticleRadius(particleRadius)
    , m_KernelRadius  (particleRadius * 4.0f)
    , m_BoundaryMin   (boundaryMin)
    , m_BoundaryMax   (boundaryMax)
{
    const float r3 = m_ParticleRadius * m_ParticleRadius * m_ParticleRadius;
    m_Mass        = 6.4f * r3 * m_RestDensity;
    m_MassInverse = 1.0f / m_Mass;

    glm::vec3 span  = m_BoundaryMax - m_BoundaryMin;
    m_GridDims.x    = std::max(1, (int)std::ceil(span.x / m_KernelRadius));
    m_GridDims.y    = std::max(1, (int)std::ceil(span.y / m_KernelRadius));
    m_GridDims.z    = std::max(1, (int)std::ceil(span.z / m_KernelRadius));
    m_CellCount  = m_GridDims.x * m_GridDims.y * m_GridDims.z;
    m_BlockSize  = (int)std::ceil(std::sqrt((float)m_CellCount));

    AllocateBuffers();
    LoadShaders();
    InitBuffers();

    std::cout << "[EmitterFluidSimulation] Ready — maxN=" << m_MaxParticles
              << " grid=" << m_GridDims.x << "x" << m_GridDims.y << "x" << m_GridDims.z
              << " cells=" << m_CellCount << std::endl;
    m_Ready = true;
}

EmitterFluidSimulation::~EmitterFluidSimulation() {
    ReleaseBuffers();
    std::cout << "[EmitterFluidSimulation] Destroyed" << std::endl;
}

void EmitterFluidSimulation::AllocateBuffers() {
    const int N         = m_MaxParticles;
    const int CELLS     = m_CellCount;
    const int NEIGHBORS = N * MAX_NEIGHBORS;

    GLuint all[18];
    glGenBuffers(18, all);
    m_Buffers.positionSSBO      = all[0];
    m_Buffers.predictedSSBO     = all[1];
    m_Buffers.velocitySSBO      = all[2];
    m_Buffers.cellCountSSBO     = all[3];
    m_Buffers.cellOffsetSSBO    = all[4];
    m_Buffers.sortedIdxSSBO     = all[5];
    m_Buffers.blockOffsetSSBO   = all[6];
    m_Buffers.neighborCountSSBO = all[7];
    m_Buffers.neighborIdxSSBO   = all[8];
    m_Buffers.densitySSBO       = all[9];
    m_Buffers.constraintSSBO    = all[10];
    m_Buffers.gradSqSumSSBO     = all[11];
    m_Buffers.lambdaSSBO        = all[12];
    m_Buffers.deltaPosSSBO      = all[13];
    m_Buffers.lifetimeSSBO      = all[14];
    m_Buffers.freeListSSBO      = all[15];
    m_Buffers.freeCountSSBO     = all[16];
    m_Buffers.colorSSBO         = all[17];

    auto AllocVec4  = [](GLuint id, int count) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(count * 4 * sizeof(float)), nullptr, GL_DYNAMIC_DRAW);
    };
    auto AllocUint  = [](GLuint id, int count) {
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
    AllocFloat(m_Buffers.lifetimeSSBO,      N);
    AllocUint (m_Buffers.freeListSSBO,      N);
    // freeCountSSBO：单个 uint
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffers.freeCountSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    AllocVec4 (m_Buffers.colorSSBO,         N);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void EmitterFluidSimulation::ReleaseBuffers() {
    GLuint all[] = {
        m_Buffers.positionSSBO,     m_Buffers.predictedSSBO,    m_Buffers.velocitySSBO,
        m_Buffers.cellCountSSBO,    m_Buffers.cellOffsetSSBO,   m_Buffers.sortedIdxSSBO,
        m_Buffers.blockOffsetSSBO,  m_Buffers.neighborCountSSBO, m_Buffers.neighborIdxSSBO,
        m_Buffers.densitySSBO,      m_Buffers.constraintSSBO,   m_Buffers.gradSqSumSSBO,
        m_Buffers.lambdaSSBO,       m_Buffers.deltaPosSSBO,
        m_Buffers.lifetimeSSBO,     m_Buffers.freeListSSBO,
        m_Buffers.freeCountSSBO,    m_Buffers.colorSSBO
    };
    glDeleteBuffers(static_cast<GLsizei>(std::size(all)), all);
}

void EmitterFluidSimulation::InitBuffers() {
    const int N = m_MaxParticles;

    // 全部粒子放到哨兵位置（不参与空间哈希）
    std::vector<glm::vec4> sentinels(N, glm::vec4(-9999.0f, -9999.0f, -9999.0f, 1.0f));
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffers.positionSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(N * sizeof(glm::vec4)), sentinels.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffers.predictedSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(N * sizeof(glm::vec4)), sentinels.data());

    // 速度全零
    std::vector<glm::vec4> zeros(N, glm::vec4(0.0f));
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffers.velocitySSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(N * sizeof(glm::vec4)), zeros.data());

    // lifetime 全零（所有槽位死亡）
    std::vector<float> ltZero(N, 0.0f);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffers.lifetimeSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(N * sizeof(float)), ltZero.data());

    // freeList = [0, 1, 2, ..., N-1]
    std::vector<uint32_t> freeList(N);
    std::iota(freeList.begin(), freeList.end(), 0u);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffers.freeListSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(N * sizeof(uint32_t)), freeList.data());

    // freeCount = N（栈满）
    uint32_t freeCount = (uint32_t)N;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffers.freeCountSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &freeCount);

    // color 全零
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffers.colorSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(N * sizeof(glm::vec4)), zeros.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void EmitterFluidSimulation::LoadShaders() {
    auto Load = [](const char* path) -> Ref<ComputePipeline> {
        auto cs = ComputePipeline::Create(path);
        if (!cs || !cs->IsValid())
            std::cerr << "[EmitterFluidSimulation] Failed to load: " << path << std::endl;
        return cs;
    };

    // PBF pipeline（复用现有 shader，路径与 FluidSimulation 相同）
    m_IntegrateCS        = Load("assets/shaders/physics/emitter_integrate.glsl");
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

    // Emitter-specific
    m_EmitCS          = Load("assets/shaders/physics/emit.glsl");
    m_RecycleCS       = Load("assets/shaders/physics/recycle.glsl");
    m_SceneCollisionCS = Load("assets/shaders/physics/scene_collision.glsl");
}

void EmitterFluidSimulation::BindAllSSBOs() {
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
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 14, m_Buffers.lifetimeSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 15, m_Buffers.freeListSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 16, m_Buffers.freeCountSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 17, m_Buffers.colorSSBO);
}

void EmitterFluidSimulation::Emit(glm::vec3 emitPos, glm::vec3 emitDir,
                                   int count, float speed, float lifetime,
                                   float coneAngleRad, glm::vec4 color) {
    if (!m_Ready || count <= 0) return;

    BindAllSSBOs();

    constexpr int EMIT_WG = 64;
    const uint32_t groups = (uint32_t)CeilDiv(count, EMIT_WG);

    m_EmitCS->Bind();
    m_EmitCS->SetUint ("u_EmitCount",  (uint32_t)count);
    m_EmitCS->SetVec3 ("u_EmitPos",    emitPos);
    m_EmitCS->SetVec3 ("u_EmitDir",    glm::normalize(emitDir));
    m_EmitCS->SetFloat("u_EmitSpeed",  speed);
    m_EmitCS->SetFloat("u_Lifetime",   lifetime);
    m_EmitCS->SetFloat("u_ConeAngle",  coneAngleRad);
    m_EmitCS->SetVec4 ("u_EmitColor",  color);
    m_EmitCS->SetUint ("u_FrameSeed",  m_FrameSeed++);
    m_EmitCS->Dispatch(groups, 1, 1);
    ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);
}

void EmitterFluidSimulation::Recycle(float dt) {
    if (!m_Ready) return;

    BindAllSSBOs();

    const uint32_t groups = (uint32_t)CeilDiv(m_MaxParticles, WORKGROUP_SIZE);
    m_RecycleCS->Bind();
    m_RecycleCS->SetUint ("u_MaxParticles", (uint32_t)m_MaxParticles);
    m_RecycleCS->SetFloat("u_DeltaTime",    dt);
    m_RecycleCS->Dispatch(groups, 1, 1);
    ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);
}

void EmitterFluidSimulation::Step(float dt) {
    if (!m_Ready) return;

    BindAllSSBOs();

    const uint32_t pGroups = (uint32_t)CeilDiv(m_MaxParticles, WORKGROUP_SIZE);
    const uint32_t cGroups = (uint32_t)CeilDiv(m_CellCount,    WORKGROUP_SIZE);
    const uint32_t bGroups = (uint32_t)CeilDiv(m_BlockSize,    WORKGROUP_SIZE);
    const uint32_t N       = (uint32_t)m_MaxParticles;
    const uint32_t CELLS   = (uint32_t)m_CellCount;
    const uint32_t BSIZE   = (uint32_t)m_BlockSize;

    // ① 外力积分（含 lifetime 跳过）
    {
        m_IntegrateCS->Bind();
        m_IntegrateCS->SetVec3 ("u_Gravity",      glm::vec3(0.0f, -9.8f, 0.0f));
        m_IntegrateCS->SetFloat("u_DeltaTime",     dt);
        m_IntegrateCS->SetUint ("u_ParticleCount", N);
        m_IntegrateCS->Dispatch(pGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // ①-b 场景碰撞（Phase 12-G）：GBuffer 投影，跳过死亡粒子（u_HasLifetime=1）
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
        m_SceneCollisionCS->SetFloat("u_Restitution",    m_Restitution);
        m_SceneCollisionCS->SetInt  ("u_HasLifetime",    1);
        m_SceneCollisionCS->Dispatch(pGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);

        glActiveTexture(GL_TEXTURE0);
    }

    // ② 空间哈希重建
    {
        m_HashClearCS->Bind();
        m_HashClearCS->SetUint("u_CellCount", CELLS);
        m_HashClearCS->Dispatch(cGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);

        m_HashCountCS->Bind();
        m_HashCountCS->SetVec3 ("u_BoundaryMin",  m_BoundaryMin);
        m_HashCountCS->SetFloat("u_KernelRadius", m_KernelRadius);
        glUniform3i(glGetUniformLocation(m_HashCountCS->GetID(), "u_GridDims"),
                    m_GridDims.x, m_GridDims.y, m_GridDims.z);
        m_HashCountCS->SetUint("u_ParticleCount", N);
        m_HashCountCS->Dispatch(pGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);

        m_HashPrefixInnerCS->Bind();
        m_HashPrefixInnerCS->SetUint("u_BlockSize", BSIZE);
        m_HashPrefixInnerCS->SetUint("u_CellCount", CELLS);
        m_HashPrefixInnerCS->Dispatch(bGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);

        m_HashPrefixBlockCS->Bind();
        m_HashPrefixBlockCS->SetUint("u_BlockSize", BSIZE);
        m_HashPrefixBlockCS->Dispatch(1, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);

        m_HashPrefixFinalCS->Bind();
        m_HashPrefixFinalCS->SetUint("u_BlockSize", BSIZE);
        m_HashPrefixFinalCS->SetUint("u_CellCount", CELLS);
        m_HashPrefixFinalCS->Dispatch(bGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);

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
        m_NeighborSearchCS->SetUint("u_ParticleCount", N);
        m_NeighborSearchCS->SetUint("u_MaxNeighbors",  (uint32_t)MAX_NEIGHBORS);
        m_NeighborSearchCS->Dispatch(pGroups, 1, 1);
        ComputePipeline::Wait(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // ④ 密度约束迭代
    for (int iter = 0; iter < m_SolverIters; iter++) {
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

    // ⑥ XSPH 粘性
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

void EmitterFluidSimulation::SetSceneGBuffer(GLuint depthTex, GLuint normalTex,
                                              const glm::mat4& viewProj,
                                              const glm::mat4& invViewProj) {
    m_GDepthTex    = depthTex;
    m_GNormalTex   = normalTex;
    m_GViewProj    = viewProj;
    m_GInvViewProj = invViewProj;
}

} // namespace GLRenderer
