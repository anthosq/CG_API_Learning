#version 430 core

// hash_prefix_inner.glsl — 空间哈希步骤 3/6：组内前缀和 + 保存块总和
//
// 每个线程（blockId）负责 u_BlockSize 个格子的串行扫描：
//   cubeOffset[start]     = 0（exclusive scan，起点为 0）
//   cubeOffset[start+i]   = cubeOffset[start+i-1] + cellCount[start+i-1]
//   blockSum[blockId]     = 本块所有格子的粒子总数
//
// 注：此实现与参考实现一致——以 sqrt(CellCount) 为块大小，
// 每线程串行扫描。对典型流体规模（< 50K cells）性能足够。

layout(local_size_x = 256) in;

layout(std430, binding = 3) buffer CellCount  { uint cellCount[]; };
layout(std430, binding = 4) buffer CellOffset { uint cellOffset[]; };
layout(std430, binding = 6) buffer BlockSum   { uint blockSum[]; };

uniform uint u_BlockSize;   // = ceil(sqrt(CellCount))
uniform uint u_CellCount;

void main() {
    uint blockId = gl_GlobalInvocationID.x;
    uint start   = blockId * u_BlockSize;

    if (blockId >= u_BlockSize || start >= u_CellCount) return;

    cellOffset[start] = 0u;
    blockSum[blockId] = 0u;

    for (uint i = 1u; i < u_BlockSize && start + i < u_CellCount; i++) {
        cellOffset[start + i] = cellOffset[start + i - 1u] + cellCount[start + i - 1u];
    }

    // 块总和 = 块内所有格子的粒子数之和
    uint last = min(start + u_BlockSize, u_CellCount);
    for (uint i = start; i < last; i++) {
        blockSum[blockId] += cellCount[i];
    }
}
