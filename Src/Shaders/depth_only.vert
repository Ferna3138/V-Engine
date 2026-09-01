#version 450

#include "shader_common.glsl"

layout(location = 0) in vec3 position;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

void main() {
    gl_Position = ubo.projection * ubo.view * push.modelMatrix * vec4(position, 1.0);
}
