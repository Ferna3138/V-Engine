#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColour;
layout(set = 0, binding = 0) uniform sampler2D sceneColour;
layout(set = 0, binding = 1) uniform sampler2D dofFarB;

layout(push_constant) uniform Push {
    vec4 exposure; // .rgb = white-balance gain, .a = scale
} push;


vec3 pbrNeutral(vec3 color) {
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;

    float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, newPeak * vec3(1.0), g);
}

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec3  sharp = texture(sceneColour, uv).rgb;
    vec4  far   = texture(dofFarB, uv);               // bilinear upsample from half-res
    float amt   = smoothstep(0.5, 3.0, far.a);        // CoC px -> 0..1 blend
    vec3  c     = mix(sharp, far.rgb, amt);

    c *= push.exposure.a * push.exposure.rgb;
    c  = pbrNeutral(c);
    outColour = vec4(c, 1.0);
}