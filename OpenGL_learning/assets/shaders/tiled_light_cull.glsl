// Tiled Forward+ — 光源剔除计算着色器（2.5D Culling）
//
// 将屏幕划分为 TILE_SIZE×TILE_SIZE 像素的 tile，每个 workgroup 处理一个 tile。
// 先采样深度缓冲求 tile 的 [minDepth, maxDepth]，再对每个 tile 构建紧致视锥，
// 测试所有点光源的球体与视锥的相交，将可见光源索引写入 SSBO，供 PBR GeometryPass 读取。
//
// 2.5D Culling 相比基础版，tile 的 z 范围从 [cameraNear, cameraFar]
// 收紧为 [tileMinZ, tileMaxZ]，大量遮挡场景下剔除率显著提升。

#version 430 core

#define TILE_SIZE           16
#define MAX_POINT_LIGHTS    256
#define MAX_LIGHTS_PER_TILE 128

layout(local_size_x = TILE_SIZE, local_size_y = TILE_SIZE, local_size_z = 1) in;

// ── 输入：点光源数据 ────────────────────────────────────────────
struct PointLightData {
    vec4 PosRadius;       // xyz = 世界空间位置, w = 影响半径
    vec4 ColorIntensity;  // xyz = 颜色, w = 强度（剔除时不需要，但保持布局一致）
    vec4 Params;          // x = falloff
};
layout(std430, binding = 0) readonly buffer PointLightSSBO {
    PointLightData lights[];
};

// ── 输出：每 tile 的可见光源列表 ───────────────────────────────
layout(std430, binding = 1) writeonly buffer TileLightCounts {
    int tileCounts[];
};
layout(std430, binding = 2) writeonly buffer TileLightIndices {
    int tileIndices[];
};

// ── Camera UBO（binding = 0，与 VS/FS 共享）───────────────────
layout(std140, binding = 0) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_View;
    mat4 u_Projection;
    vec4 u_CameraPos;
};

// ── Uniforms ────────────────────────────────────────────────────
uniform sampler2D u_DepthTex;     // 深度缓冲（Depth Pre-Pass 输出）
uniform ivec2 u_ScreenSize;       // 屏幕分辨率（像素）
uniform int   u_PointLightCount;  // 当前帧点光源数量
uniform float u_NearPlane;
uniform float u_FarPlane;

// ── Shared memory：当前 tile 的可见光源 + 深度范围 ────────────────
shared int  s_LightCount;
shared int  s_LightIndices[MAX_LIGHTS_PER_TILE];
shared uint s_MinDepthUint;  // tileMinDepth 的 floatBitsToUint 表示
shared uint s_MaxDepthUint;  // tileMaxDepth 的 floatBitsToUint 表示

// ── Tile 视锥（4 个侧平面，view space，法线朝内）─────────────────
struct TileFrustum {
    vec4 planes[4]; // Left, Right, Bottom, Top（xyz = 法线, w = 0）
};

// 从投影矩阵和 tile NDC 范围构建 view space 侧平面
// 各平面过原点（摄像机），法线由 tile 角点光线叉积得到
TileFrustum BuildTileFrustum(ivec2 tileID) {
    // tile NDC 边界
    float ndcMinX = (float(tileID.x * TILE_SIZE)       / float(u_ScreenSize.x)) * 2.0 - 1.0;
    float ndcMaxX = (float((tileID.x + 1) * TILE_SIZE) / float(u_ScreenSize.x)) * 2.0 - 1.0;
    float ndcMinY = (float(tileID.y * TILE_SIZE)       / float(u_ScreenSize.y)) * 2.0 - 1.0;
    float ndcMaxY = (float((tileID.y + 1) * TILE_SIZE) / float(u_ScreenSize.y)) * 2.0 - 1.0;

    // view space 角点（z = -1，near plane 上）
    // view.x = ndc.x / P[0][0]，view.y = ndc.y / P[1][1]
    float px = u_Projection[0][0];
    float py = u_Projection[1][1];
    vec3 tl = vec3(ndcMinX / px, ndcMaxY / py, -1.0); // top-left
    vec3 tr = vec3(ndcMaxX / px, ndcMaxY / py, -1.0); // top-right
    vec3 bl = vec3(ndcMinX / px, ndcMinY / py, -1.0); // bottom-left
    vec3 br = vec3(ndcMaxX / px, ndcMinY / py, -1.0); // bottom-right

    TileFrustum f;
    f.planes[0] = vec4(normalize(cross(tl, bl)), 0.0); // Left:   tl × bl
    f.planes[1] = vec4(normalize(cross(br, tr)), 0.0); // Right:  br × tr
    f.planes[2] = vec4(normalize(cross(bl, br)), 0.0); // Bottom: bl × br
    f.planes[3] = vec4(normalize(cross(tr, tl)), 0.0); // Top:    tr × tl
    return f;
}

// 球体与 tile 视锥相交测试（view space）
// tileNear/tileFar 为 2.5D culling 从深度缓冲中提取的紧致 z 范围
bool SphereInsideFrustum(vec3 center, float radius, TileFrustum f, float tileNear, float tileFar) {
    // 4 个侧平面（过原点，dot(n, p) >= -r 则未被剔除）
    for (int i = 0; i < 4; i++) {
        if (dot(f.planes[i].xyz, center) < -radius)
            return false;
    }
    // 深度感知的近/远裁剪（view space 中摄像机朝 -z，-center.z = 距摄像机深度）
    if (-center.z + radius < tileNear) return false;
    if (-center.z - radius > tileFar)  return false;
    return true;
}

void main() {
    ivec2 tileID     = ivec2(gl_WorkGroupID.xy);
    int   tileCountX = int(gl_NumWorkGroups.x);
    int   tileIndex  = tileID.y * tileCountX + tileID.x;
    int   localIdx   = int(gl_LocalInvocationIndex); // 0..255

    // ── Step 1: 初始化共享变量 ─────────────────────────────────────
    if (localIdx == 0) {
        s_LightCount   = 0;
        s_MinDepthUint = 0xFFFFFFFFu;  // float max
        s_MaxDepthUint = 0u;           // float 0
    }
    barrier();
    memoryBarrierShared();

    // ── Step 2: 2.5D — 每线程采样对应像素的深度，求 tile min/max ──
    ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
    pixelCoord = clamp(pixelCoord, ivec2(0), u_ScreenSize - ivec2(1));
    float ndcZ    = texelFetch(u_DepthTex, pixelCoord, 0).r;
    // NDC 深度 [0,1] → 线性 view-space 深度（正值，单位：世界单位）
    float linearZ = (u_NearPlane * u_FarPlane) / (u_FarPlane + ndcZ * (u_NearPlane - u_FarPlane));

    // IEEE 754 正浮点与 uint 大小顺序一致，可用 atomicMin/Max
    atomicMin(s_MinDepthUint, floatBitsToUint(linearZ));
    atomicMax(s_MaxDepthUint, floatBitsToUint(linearZ));

    barrier();
    memoryBarrierShared();

    float tileNear = uintBitsToFloat(s_MinDepthUint);
    float tileFar  = uintBitsToFloat(s_MaxDepthUint);

    // ── Step 3: 构建此 tile 的 view space 视锥 ────────────────────
    TileFrustum tileFrustum = BuildTileFrustum(tileID);

    // ── Step 4: 256 线程协作测试所有点光源 ───────────────────────
    int numLights = min(u_PointLightCount, MAX_POINT_LIGHTS);
    int threads   = int(gl_WorkGroupSize.x * gl_WorkGroupSize.y); // 256
    for (int i = localIdx; i < numLights; i += threads) {
        vec3  posWorld = lights[i].PosRadius.xyz;
        float radius   = lights[i].PosRadius.w;
        vec3  posView  = (u_View * vec4(posWorld, 1.0)).xyz;
        if (SphereInsideFrustum(posView, radius, tileFrustum, tileNear, tileFar)) {
            int slot = atomicAdd(s_LightCount, 1);
            if (slot < MAX_LIGHTS_PER_TILE)
                s_LightIndices[slot] = i;
        }
    }

    barrier();
    memoryBarrierShared();

    // 主线程写入全局 SSBO
    if (localIdx == 0) {
        int count = min(s_LightCount, MAX_LIGHTS_PER_TILE);
        tileCounts[tileIndex] = count;
        int base = tileIndex * MAX_LIGHTS_PER_TILE;
        for (int i = 0; i < count; i++)
            tileIndices[base + i] = s_LightIndices[i];
    }
}
