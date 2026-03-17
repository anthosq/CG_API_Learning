#version 330 core

// 多渲染目标输出
layout(location = 0) out vec4 FragColor;
layout(location = 1) out int EntityID;

in vec2 TexCoords;

uniform sampler2D texture_diffuse1;

// Entity ID for mouse picking
uniform int u_EntityID;

float near = 0.1;
float far = 100.0;

float linearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // back to NDC
    return (2.0 * near * far) / (far + near - z * (far - near));
}


void main()
{
    // float depth = linearizeDepth(gl_FragCoord.z) / far;
    // FragColor = vec4(vec3(depth), 1.0);
    FragColor = texture(texture_diffuse1, TexCoords);
    EntityID = u_EntityID;
}
