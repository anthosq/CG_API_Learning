#version 430 core

// hash_prefix_block.glsl — 空间哈希步骤 4/6：对块总和做前缀和
//
// 单线程串行扫描 blockSum[]，将其变为各块的起始偏移。
// 块数 = u_BlockSize，串行工作量 O(sqrt(CellCount))，可接受。

layout(local_size_x = 1) in;

layout(std430, binding = 6) buffer BlockSum { uint blockSum[]; };

uniform uint u_BlockSize;

void main() {
    // blockSum[i] 变为前 i 块的粒子总数（exclusive prefix sum）
    for (uint i = 1u; i < u_BlockSize; i++) {
        blockSum[i] += blockSum[i - 1u];
    }
}
