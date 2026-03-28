#version 430 core

// hash_prefix_final.glsl — 空间哈希步骤 5/6：将块偏移加回各格子偏移
//
// cellOffset[c] += blockSum[blockId - 1]（blockId > 0 时）
// 完成后 cellOffset[c] = 格子 c 在 sortedIdx[] 中的起始位置（全局 exclusive scan 结果）

layout(local_size_x = 256) in;

layout(std430, binding = 4) buffer CellOffset { uint cellOffset[]; };
layout(std430, binding = 6) buffer BlockSum   { uint blockSum[]; };

uniform uint u_BlockSize;
uniform uint u_CellCount;

void main() {
    uint blockId = gl_GlobalInvocationID.x;
    if (blockId >= u_BlockSize || blockId == 0u) return;

    uint start = blockId * u_BlockSize;
    uint last  = min(start + u_BlockSize, u_CellCount);
    uint base  = blockSum[blockId - 1u];

    for (uint i = start; i < last; i++) {
        cellOffset[i] += base;
    }
}
