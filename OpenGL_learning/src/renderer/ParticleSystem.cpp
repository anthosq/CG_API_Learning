#include "ParticleSystem.h"
#include "utils/GLCommon.h"
#include <vector>
#include <numeric>

namespace GLRenderer {

ParticleSystem::ParticleSystem(int maxParticles, uint32_t seed)
    : m_MaxParticles(maxParticles)
{
    InitBuffers(seed);
}

ParticleSystem::~ParticleSystem()
{
    if (m_ParticleSSBO)   glDeleteBuffers(1, &m_ParticleSSBO);
    if (m_AliveSSBO)      glDeleteBuffers(1, &m_AliveSSBO);
    if (m_DeadSSBO)       glDeleteBuffers(1, &m_DeadSSBO);
    if (m_CounterSSBO)    glDeleteBuffers(1, &m_CounterSSBO);
    if (m_IndirectBuffer) glDeleteBuffers(1, &m_IndirectBuffer);
    if (m_DummyVAO)       glDeleteVertexArrays(1, &m_DummyVAO);
}

void ParticleSystem::InitBuffers(uint32_t /*seed*/)
{
    // 粒子 SSBO（全部初始化为 0，alive=0）
    glGenBuffers(1, &m_ParticleSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ParticleSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 sizeof(GPUParticle) * m_MaxParticles,
                 nullptr, GL_DYNAMIC_DRAW);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    // AliveList SSBO
    glGenBuffers(1, &m_AliveSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_AliveSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 sizeof(uint32_t) * m_MaxParticles,
                 nullptr, GL_DYNAMIC_DRAW);

    // DeadList SSBO：初始化为 0,1,2,...,maxParticles-1（全部空闲）
    {
        std::vector<uint32_t> deadList(m_MaxParticles);
        std::iota(deadList.begin(), deadList.end(), 0u);
        glGenBuffers(1, &m_DeadSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_DeadSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
                     sizeof(uint32_t) * m_MaxParticles,
                     deadList.data(), GL_DYNAMIC_DRAW);
    }

    // Counter SSBO: uint[4] = { aliveCount=0, deadCount=maxParticles, emitCount=0, _pad=0 }
    {
        uint32_t counters[4] = { 0u, (uint32_t)m_MaxParticles, 0u, 0u };
        glGenBuffers(1, &m_CounterSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_CounterSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(counters), counters, GL_DYNAMIC_DRAW);
    }

    // Indirect buffer: DrawArraysIndirectCommand
    {
        DrawArraysIndirectCommand cmd = { 0u, 1u, 0u, 0u };
        glGenBuffers(1, &m_IndirectBuffer);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_IndirectBuffer);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(cmd), &cmd, GL_DYNAMIC_DRAW);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER,  0);

    // Dummy VAO：Core Profile 要求所有 draw call 必须绑定 VAO
    // 粒子渲染不使用顶点属性（gl_VertexID 从 SSBO 拉数据），但仍需一个合法的 VAO
    glGenVertexArrays(1, &m_DummyVAO);
}

void ParticleSystem::Simulate(const EmitParams& params,
                               Ref<ComputePipeline> emitPipeline,
                               Ref<ComputePipeline> updatePipeline,
                               Ref<ComputePipeline> compactPipeline)
{
    // 绑定所有 SSBO
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SSBO_BINDING_PARTICLES,  m_ParticleSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SSBO_BINDING_ALIVE_LIST, m_AliveSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SSBO_BINDING_DEAD_LIST,  m_DeadSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SSBO_BINDING_COUNTERS,   m_CounterSSBO);

    // 每帧只重置 aliveCount=0（Compact 重新统计）
    // deadCount 是跨帧持久状态，不能重置：Emit 消耗（递减），Update 回收（递增）
    {
        uint32_t zero = 0u;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_CounterSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &zero);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    uint32_t groupsAll = ((uint32_t)m_MaxParticles + 63u) / 64u;

    // Pass 1: Emit
    if (params.EmitCount > 0 && emitPipeline && *emitPipeline) {
        emitPipeline->Bind();
        emitPipeline->SetVec4 ("u_EmitPos",     params.Position);
        emitPipeline->SetVec4 ("u_EmitDir",     params.EmitDirection);
        emitPipeline->SetVec4 ("u_ColorBegin",  params.ColorBegin);
        emitPipeline->SetVec4 ("u_ColorEnd",    params.ColorEnd);
        emitPipeline->SetVec4 ("u_Gravity",     params.Gravity);
        emitPipeline->SetFloat("u_EmitSpread",  params.EmitSpread);
        emitPipeline->SetFloat("u_LifetimeMin", params.LifetimeMin);
        emitPipeline->SetFloat("u_LifetimeMax", params.LifetimeMax);
        emitPipeline->SetFloat("u_SpeedMin",    params.SpeedMin);
        emitPipeline->SetFloat("u_SpeedMax",    params.SpeedMax);
        emitPipeline->SetFloat("u_SizeBegin",   params.SizeBegin);
        emitPipeline->SetFloat("u_SizeEnd",     params.SizeEnd);
        emitPipeline->SetInt  ("u_EmitCount",   params.EmitCount);
        emitPipeline->SetInt  ("u_MaxParticles",m_MaxParticles);

        uint32_t emitGroups = ((uint32_t)params.EmitCount + 63u) / 64u;
        emitPipeline->Dispatch(emitGroups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // Pass 2: Update（遍历所有槽，内部判断 alive）
    if (updatePipeline && *updatePipeline) {
        updatePipeline->Bind();
        updatePipeline->SetFloat("u_DeltaTime",   params.DeltaTime);
        updatePipeline->SetInt  ("u_MaxParticles",m_MaxParticles);
        updatePipeline->SetVec4 ("u_Gravity",     params.Gravity);
        updatePipeline->SetVec4 ("u_ColorBegin",  params.ColorBegin);
        updatePipeline->SetVec4 ("u_ColorEnd",    params.ColorEnd);
        updatePipeline->SetFloat("u_SizeBegin",   params.SizeBegin);
        updatePipeline->SetFloat("u_SizeEnd",     params.SizeEnd);

        updatePipeline->Dispatch(groupsAll, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // Pass 3: Compact — 收集存活粒子写入 AliveList
    // IndirectBuffer 由 CPU 在 Compact 后更新（跨 workgroup barrier 无法在 shader 内保证顺序）
    if (compactPipeline && *compactPipeline) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, m_IndirectBuffer);

        compactPipeline->Bind();
        compactPipeline->SetInt("u_MaxParticles", m_MaxParticles);

        compactPipeline->Dispatch(groupsAll, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // CPU 端读回 aliveCount，写入 IndirectBuffer（避免跨 workgroup 竞争问题）
    {
        uint32_t counters[4] = {};
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_CounterSSBO);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(counters), counters);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        uint32_t count = counters[0] * 6u;  // aliveCount * 6 顶点/粒子
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_IndirectBuffer);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(uint32_t), &count);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

        glMemoryBarrier(GL_COMMAND_BARRIER_BIT);
    }
}

void ParticleSystem::BindForRender() const
{
    glBindVertexArray(m_DummyVAO);  // Core Profile 要求 draw call 必须有 VAO
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SSBO_BINDING_PARTICLES,  m_ParticleSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SSBO_BINDING_ALIVE_LIST, m_AliveSSBO);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_IndirectBuffer);
}

int ParticleSystem::GetAliveCount() const
{
    uint32_t counters[4] = {};
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_CounterSSBO);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(counters), counters);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return (int)counters[0];
}

} // namespace GLRenderer
