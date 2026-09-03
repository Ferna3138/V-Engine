#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColour;
layout(set = 0, binding = 0) uniform sampler2D sceneColour;  // HDR
layout(set = 0, binding = 1) uniform sampler2D sceneDepth;

layout(push_constant) uniform Push {
    mat4 reprojection;   // prevViewProj * inverse(currViewProj)
    vec4 params;         // x = shutterTime/frameTime, y = maxBlurPx, z = sampleCount, w = enabled
} p;

float hash(vec2 v){ return fract(sin(dot(v, vec2(12.9898, 78.233))) * 43758.5453); }

void main() {
    vec3 here = texture(sceneColour, uv).rgb;
    if (p.params.w < 0.5) { outColour = vec4(here, 1.0); return; }

    float d = texture(sceneDepth, uv).r;
    vec4 prevClip = p.reprojection * vec4(uv * 2.0 - 1.0, d, 1.0);

    if (abs(prevClip.w) < 1e-5) {
        outColour = vec4(here, 1.0); return;
    }
    
    vec2 prevUV   = (prevClip.xy / prevClip.w) * 0.5 + 0.5;

    vec2 texel = 1.0 / vec2(textureSize(sceneColour, 0));
    vec2 vel = (uv - prevUV) * p.params.x;                    // scaled to the shutter-open interval
    float maxLen = p.params.y * length(texel);
    if (length(vel) > maxLen) vel = normalize(vel) * maxLen;

    if (dot(vel, vel) < 1e-8) {
        outColour = vec4(here, 1.0); return;
    }  // early out

    int   N = int(p.params.z);
    float j = hash(uv) - 0.5;                                  // dither to break banding
    vec3 sum = here;
    float w = 1.0;

    for (int i = 1; i < N; ++i) {
        float t = (float(i) + j) / float(N) - 0.5;             // -0.5 .. 0.5
        sum += texture(sceneColour, uv + vel * t).rgb;
        w += 1.0;
    }

    outColour = vec4(sum / w, 1.0);
}