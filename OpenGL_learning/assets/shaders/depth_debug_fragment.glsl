#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthMap;  // 深度纹理
uniform float near;
uniform float far;
uniform int visualizationMode = 0;  // 0: linear, 1: logarithmic, 2: inverted, 3: heat map

float LinearizeDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    return (2.0 * near * far) / (far + near - z * (far - near));
}

vec3 depthToColor(float depth) {
    // 使用热力图着色
    float r = smoothstep(0.0, 0.5, depth);
    float g = smoothstep(0.25, 0.75, depth);
    float b = smoothstep(0.5, 1.0, depth);
    return vec3(r, g, b);
}

void main() {
    // 从深度纹理中读取深度值
    float depth = texture(depthMap, TexCoords).r;
    float linearDepth = LinearizeDepth(depth) / far;
    
    vec3 color;
    
    if (visualizationMode == 0) {
        // 线性灰度
        color = vec3(linearDepth);
    } else if (visualizationMode == 1) {
        // 对数刻度（更好地显示近处细节）
        float logDepth = log(linearDepth * far + 1.0) / log(far + 1.0);
        color = vec3(logDepth);
    } else if (visualizationMode == 2) {
        // 反转（近处亮，远处暗）
        color = vec3(1.0 - linearDepth);
    } else {
        // 热力图
        color = depthToColor(linearDepth);
    }
    
    FragColor = vec4(color, 1.0);
}