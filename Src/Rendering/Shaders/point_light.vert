#version 450

#include "shader_common.glsl"

const vec2 OFFSETS[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, 1.0)
);

layout (location = 0) out vec2 fragOffset;


layout(push_constant) uniform Push {
    vec4 lightPosition; // Position in world space
    vec4 colour;
    float radius;
} push;


void main() {
    fragOffset = OFFSETS[gl_VertexIndex];

    vec4 lightInCameraSpace = ubo.view * vec4(push.lightPosition);
    vec4 positionInCameraSpace = lightInCameraSpace + push.radius * vec4(fragOffset, 0.0, 0.0);

    gl_Position = ubo.projection * positionInCameraSpace;
}