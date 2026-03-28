#version 430 core

// hash_assign.glsl — 空间哈希步骤 6/6：将粒子写入排序后的索引数组
//
// 每个粒子原子地"认领"其格子的一个位置：
//   slot = atomicAdd(cellOffset[cell], 1)
//   sortedIdx[slot] = 粒子索引
//
// 副作用：cellOffset[c] 完成后变为该格子段的末尾（exclusive end）。
// 邻居搜索时倒序读取：sortedIdx[cellOffset[c] - 1 - j]，j=0..count-1。

layout(local_size_x = 256) in;

layout(std430, binding = 1) buffer PositionPredict { vec4 predicted[]; };
layout(std430, binding = 4) buffer CellOffset      { uint cellOffset[]; };
layout(std430, binding = 5) buffer SortedIdx       { uint sortedIdx[]; };

uniform vec3  u_BoundaryMin;
uniform float u_KernelRadius;
uniform ivec3 u_GridDims;
uniform uint  u_ParticleCount;

int flatCellIndex(vec3 pos) {
    vec3  local = pos - u_BoundaryMin;
    ivec3 cell  = clamp(ivec3(floor(local / u_KernelRadius)),
                        ivec3(0), u_GridDims - 1);
    return cell.x * (u_GridDims.y * u_GridDims.z)
         + cell.y * u_GridDims.z
         + cell.z;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_ParticleCount) return;

    int  cell = flatCellIndex(predicted[idx].xyz);
    uint slot = atomicAdd(cellOffset[cell], 1u);
    sortedIdx[slot] = idx;
}
