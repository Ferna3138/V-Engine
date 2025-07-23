#version 450

const vec2 OFFSETS[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, 1.0)
);

layout (location = 0) out vec2 fragOffset;

struct PointLight{
    vec4 position;
    vec4 colour;
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    vec4 ambientLightColor; // w is intensity
    PointLight pointLights[10];
    int numLights;
} ubo;

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