#version 450

layout(location = 0) in vec3 fragColour;
layout(location = 0) out vec4 outColour;

layout(push_constant) uniform Push {
    mat4 transform;
    vec3 colour;
} push;

void main() {
    outColour = vec4(fragColour, 1.0);
}