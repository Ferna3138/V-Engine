#version 450

#include "shader_common.glsl"

layout (location = 0) in vec2 fragOffset;
layout (location = 0) out vec4 outColor;


layout(push_constant) uniform Push {
    vec4 lightPosition; // Position in world space
    vec4 colour;
    float radius;
} push;

void main() {
    float dis = sqrt(dot(fragOffset, fragOffset));
    if (dis >= 1.0) {
        discard;
    }
    outColor = vec4(push.colour.xyz, 1.0);
}