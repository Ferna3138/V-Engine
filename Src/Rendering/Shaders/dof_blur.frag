#version 450

layout(location=0) in vec2 uv;
layout(location=0) out vec4 outFar;

layout(set=0,binding=0) uniform sampler2D dofFar;   // rgb + CoC(px) in .a

layout(push_constant) uniform Push {
    vec4 params;   // x = blades, y = bladeRot, z = sampleCount, w = unused
} p;

const float PI = 3.14159265;

float ngon(float theta, float blades, float rot) {
    float m = PI / blades;
    return cos(m) / cos(mod(theta + rot, 2.0 * m) - m);
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(dofFar, 0));
    vec4 center = texture(dofFar, uv);
    float R = center.a;                        // half-res px
    if (R < 0.5) { outFar = vec4(center.rgb, R); return; }

    int   N     = int(p.params.z);
    float blades = p.params.x, rot = p.params.y;
    vec3  accum = center.rgb; float weight = 1.0;

    for (int i = 0; i < N; ++i) {
        float t  = float(i) / float(N);
        float ang = t * PI * 2.0 * 5.0;                    // 5 turns -> spiral
        float rad = sqrt(t) * ngon(ang, blades, rot);      // polygonal disc, density-corrected
        vec2  dir = vec2(cos(ang), sin(ang)) * rad;
        vec2  suv = uv + dir * R * texel;
        vec4  s   = texture(dofFar, suv);
        float dist = length(dir) * R;
        float w = clamp(s.a - dist + 1.0, 0.0, 1.0);       // s' blur circle must reach us
        accum  += s.rgb * w;                               // s.rgb is linear HDR, never pre-averaged
        weight += w;
    }
    outFar = vec4(accum / max(weight, 1e-4), R);
}
