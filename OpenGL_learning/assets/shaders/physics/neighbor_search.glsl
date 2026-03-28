#version 430 core

// neighbor_search.glsl — 邻居搜索：遍历 27 个相邻格子
//
// 对每个粒子：
//   1. 计算所在格子
//   2. 遍历 3×3×3 = 27 个相邻格子（含自身）
//   3. 边界检查排除越界格子
//   4. 对格子内每个粒子做距离测试（< kernelRadius）
//   5. 写入 neighborIdx[] 邻居列表，上限 MAX_NEIGHBORS
//
// 邻居读取方式（配合 hash_assign 的副作用）：
//   格子 c 的粒子在 sortedIdx[cellOffset[c] - count .. cellOffset[c] - 1]
//   即倒序遍历：sortedIdx[cellOffset[c] - 1 - j], j=0..count-1

layout(local_size_x = 256) in;

layout(std430, binding = 1) buffer PositionPredict   { vec4 predicted[]; };
layout(std430, binding = 3) buffer CellCount         { uint cellCount[]; };
layout(std430, binding = 4) buffer CellOffset        { uint cellOffset[]; };
layout(std430, binding = 5) buffer SortedIdx         { uint sortedIdx[]; };
layout(std430, binding = 7) buffer NeighborCount     { uint neighborCount[]; };
layout(std430, binding = 8) buffer NeighborIdx       { uint neighborIdx[]; };

uniform vec3  u_BoundaryMin;
uniform float u_KernelRadius;
uniform ivec3 u_GridDims;
uniform uint  u_ParticleCount;
uniform uint  u_MaxNeighbors;   // typically 64 (h=4r, ~33 neighbors in cubic packing)

ivec3 cellOf(vec3 pos) {
    vec3 local = pos - u_BoundaryMin;
    return clamp(ivec3(floor(local / u_KernelRadius)),
                 ivec3(0), u_GridDims - 1);
}

int flatIdx(ivec3 cell) {
    return cell.x * (u_GridDims.y * u_GridDims.z)
         + cell.y * u_GridDims.z
         + cell.z;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_ParticleCount) return;

    vec3  myPos  = predicted[idx].xyz;
    ivec3 myCell = cellOf(myPos);
    uint  count  = 0u;

    for (int dx = -1; dx <= 1; dx++) {
    for (int dy = -1; dy <= 1; dy++) {
    for (int dz = -1; dz <= 1; dz++) {
        ivec3 nc = myCell + ivec3(dx, dy, dz);
        if (any(lessThan(nc, ivec3(0))) || any(greaterThanEqual(nc, u_GridDims)))
            continue;

        int  cFlat    = flatIdx(nc);
        uint cCount   = cellCount[cFlat];
        uint cEnd     = cellOffset[cFlat];   // post-assign: 段末尾（exclusive）

        for (uint j = 0u; j < cCount && count < u_MaxNeighbors; j++) {
            uint nIdx = sortedIdx[cEnd - 1u - j];
            if (nIdx == idx) continue;

            float dist = length(myPos - predicted[nIdx].xyz);
            if (dist < u_KernelRadius) {
                neighborIdx[idx * u_MaxNeighbors + count] = nIdx;
                count++;
            }
        }
        if (count >= u_MaxNeighbors) break;
    }}}

    neighborCount[idx] = count;
}
