#version 330 core
in vec3 fragColor;

out vec4 FragColor;

// uniform vec3 ourColor;

void main() {
    // FragColor = vec4(fragColor * ourColor, 1.0);
    FragColor = vec4(fragColor, 1.0);
}