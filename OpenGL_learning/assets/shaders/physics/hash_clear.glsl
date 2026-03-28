#version 430 core

// hash_clear.glsl — 空间哈希步骤 1/6：清零每格粒子计数

layout(local_size_x = 256) in;

layout(std430, binding = 3) buffer CellCount { uint cellCount[]; };

uniform uint u_CellCount;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_CellCount) return;
    cellCount[idx] = 0u;
}
