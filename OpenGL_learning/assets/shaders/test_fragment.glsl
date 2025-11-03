#version 330 core
in vec2 fragTexCoord;

out vec4 FragColor;

uniform sampler2D texture1;
uniform sampler2D texture2;

void main() {
    FragColor = mix(texture(texture1, fragTexCoord), texture(texture2, fragTexCoord), 0.2);
}