#version 450

#include "shader_common.glsl"

layout (location = 0) in vec2 fragOffset;
layout (location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    vec4 lightPosition; // Position in world space
    vec4 colour;        // rgb + luminous power (lumens) in .w
    float radius;
} push;

const float PI = 3.14159265359;

void main() {
    float dis = sqrt(dot(fragOffset, fragOffset));
    if (dis >= 1.0) {
        discard;
    }
    float alpha = 0.5 * (cos(dis * PI) + 1.0);   // soft-edged disc

    // Emitter luminance of a uniform sphere of radius r radiating total power Phi:
    //   I = Phi / (4 pi)   (isotropic, matches the lighting shader)
    //   L = I / (pi r^2)   (radiance = intensity / projected area)
    // So the billboard glows in step with the light's power and shrinks/brightens
    // with its radius, and rides the same exposure as the rest of the scene.
    float r = max(push.radius, 1e-3);
    vec3 emission = push.colour.rgb * (push.colour.w / (4.0 * PI * PI * r * r));

    outColor = vec4(emission, alpha);
}
