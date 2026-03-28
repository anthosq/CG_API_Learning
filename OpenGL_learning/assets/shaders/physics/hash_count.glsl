#version 430 core

// hash_count.glsl — 空间哈希步骤 2/6：统计每格粒子数（atomic）
//
// 位置 → 格子索引映射（与参考实现的区别：使用任意 BoundaryMin 而非对称边界）：
//   localPos = predictedPos - BoundaryMin
//   cellIdx  = ivec3(floor(localPos / h))
//   flatIdx  = cellIdx.x * (Dy*Dz) + cellIdx.y * Dz + cellIdx.z

layout(local_size_x = 256) in;

layout(std430, binding = 1) buffer PositionPredict { vec4 predicted[]; };
layout(std430, binding = 3) buffer CellCount       { uint cellCount[]; };

uniform vec3  u_BoundaryMin;
uniform float u_KernelRadius;
uniform ivec3 u_GridDims;      // (Dx, Dy, Dz)
uniform uint  u_ParticleCount;

int flatCellIndex(vec3 pos) {
    vec3  local   = pos - u_BoundaryMin;
    ivec3 cell    = clamp(ivec3(floor(local / u_KernelRadius)),
                          ivec3(0), u_GridDims - 1);
    return cell.x * (u_GridDims.y * u_GridDims.z)
         + cell.y * u_GridDims.z
         + cell.z;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_ParticleCount) return;

    int cell = flatCellIndex(predicted[idx].xyz);
    atomicAdd(cellCount[cell], 1u);
}
