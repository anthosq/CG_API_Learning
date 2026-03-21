// Equirectangular to CubeMap Compute Shader
// Converts HDR equirectangular map into a cubemap using image load/store
#version 430 core

layout(binding = 0, rgba16f) restrict writeonly uniform imageCube o_CubeMap;
layout(binding = 1) uniform sampler2D u_EquirectangularMap;

const float PI = 3.14159265359;

// 将 invocation ID 转换为 cubemap 方向向量
// gl_GlobalInvocationID.z = face index (0..5: +X, -X, +Y, -Y, +Z, -Z)
vec3 GetCubeMapTexCoord(vec2 outputSize) {
    vec2 st = gl_GlobalInvocationID.xy / outputSize;
    vec2 uv = 2.0 * vec2(st.x, 1.0 - st.y) - vec2(1.0);

    vec3 ret;
    if      (gl_GlobalInvocationID.z == 0u) ret = vec3( 1.0, uv.y, -uv.x);  // +X
    else if (gl_GlobalInvocationID.z == 1u) ret = vec3(-1.0, uv.y,  uv.x);  // -X
    else if (gl_GlobalInvocationID.z == 2u) ret = vec3( uv.x, 1.0, -uv.y);  // +Y
    else if (gl_GlobalInvocationID.z == 3u) ret = vec3( uv.x,-1.0,  uv.y);  // -Y
    else if (gl_GlobalInvocationID.z == 4u) ret = vec3( uv.x, uv.y,  1.0);  // +Z
    else                                    ret = vec3(-uv.x, uv.y, -1.0);  // -Z
    return normalize(ret);
}

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
void main() {
    ivec2 outputSize = imageSize(o_CubeMap);
    if (gl_GlobalInvocationID.x >= uint(outputSize.x) ||
        gl_GlobalInvocationID.y >= uint(outputSize.y)) return;

    vec3 cubeTC = GetCubeMapTexCoord(vec2(outputSize));

    // 球面坐标转换为 UV
    float phi   = atan(cubeTC.z, cubeTC.x);     // [-PI, PI]
    float theta = acos(clamp(cubeTC.y, -1.0, 1.0));  // [0, PI]
    vec2 uv = vec2(phi / (2.0 * PI) + 0.5, theta / PI);

    vec4 color = texture(u_EquirectangularMap, uv);
    color = min(color, vec4(500.0));  // 限制亮度防止 fireflies

    imageStore(o_CubeMap, ivec3(gl_GlobalInvocationID), color);
}
