#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;

out vec3 nearPoint;
out vec3 farPoint;
out mat4 fragView;
out mat4 fragProjection;

vec3 UnprojectPoint(float x, float y, float z,mat4 viewInv, mat4 ProjInv) {
    vec4 unprojectedPoint = viewInv * ProjInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    mat4 viewInverse = inverse(view);
    mat4 projectionInverse = inverse(projection);
    nearPoint = UnprojectPoint(aPos.x, aPos.y, 0.0, viewInverse, projectionInverse);
    farPoint = UnprojectPoint(aPos.x, aPos.y, 1.0, viewInverse, projectionInverse);

    fragView = view;
    fragProjection = projection;

    gl_Position = vec4(aPos, 1.0);
}