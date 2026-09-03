#version 450

#include "dof_common.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outFar; // rgb = colour, a = far CoC (>=0), in HALF-res px

layout(set = 0, binding = 0) uniform sampler2D sceneColour;
layout(set = 0, binding = 1) uniform sampler2D sceneDepth;

layout(push_constant) uniform Push {
    vec4 dof;
    vec4 extra;
} p;

void main(){
    vec2 texel = 1.0 / vec2(textureSize(sceneColour, 0));
    // 2x2 box in full-res, Karis weight to kill fireflies
    vec3 c = vec3(0.0); float wsum = 0.0; float cocSum = 0.0;

    for (int y = 0; y < 2; ++y)
    for (int x = 0; x < 2; ++x) {
        vec2 o = (vec2(x, y) - 0.5) * texel;
        vec3 s = texture(sceneColour, uv + o).rgb;
        float w = 1.0 / (1.0 + max(max(s.r, s.g), s.b));
        c += s * w; wsum += w;
        float d = texture(sceneDepth, uv + o).r;
        
        float coc = cocPixels(linearDepth(d, p.extra.x, p.extra.z), p.dof, p.extra.xy,
                            float(textureSize(sceneColour, 0).y));

        cocSum += max(coc, 0.0);                 // far field only this increment
    }
    outFar = vec4(c / wsum, (cocSum / 4.0) * 0.5);   // *0.5 -> half-res pixels
}