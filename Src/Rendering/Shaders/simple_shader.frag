#version 450
#extension GL_EXT_nonuniform_qualifier : enable

#include "shader_common.glsl"
#include "light_common.glsl"


layout(location = 0) in vec3 fragColour;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 3) in vec2 fragUV;
layout(location = 4) flat in int fragMaterialIndex;
layout(location = 7) in vec3 fragTangentWorld;


layout(location = 0) out vec4 outColour;


layout(set = 1, binding = 0) uniform sampler2D textures[];

struct Material {
    vec4  baseColour;
    int   albedoTexIndex;
    int   mrTexIndex;
    int   normalTexIndex;
    int   useAlbedoTexture;
    float metallic;
    float roughness;
    int   useMRTexture;
    int   useNormalTexture;

    vec2  albedoOffset;
    vec2  albedoScale;
    float albedoRotation;
    int   albedoWrapMode;

    vec2  mrOffset;
    vec2  mrScale;
    float mrRotation;
    int   mrWrapMode;

    vec2  normalOffset;
    vec2  normalScale;
    float normalRotation;
    int   normalWrapMode;
};
layout(std430, set = 2, binding = 0) readonly buffer MaterialsBuffer { Material materials[]; };


layout(push_constant) uniform Push {
    mat4 modelMatrix; // Projection * View * Model
    mat4 normalMatrix;
} push;

// Wrap modes, matching TextureWrapMode in MaterialManager.hpp: 0 = Repeat,
// 1 = Mirror, 2 = Stretch (clamp to edge). Done here in UV space (rather than
// via the VkSampler's own address mode) so one bindless texture slot can be
// shared by materials that each want a different wrap behaviour.
vec2 wrapUV(vec2 uv, int mode) {
    if (mode == 1) {
        vec2 t = fract(uv * 0.5) * 2.0;
        return 1.0 - abs(t - 1.0);
    }
    if (mode == 2) return clamp(uv, 0.0, 1.0);
    return fract(uv);
}

// Per-texture-slot position/scale/rotation, applied before sampling. Rotation
// is around the UV centre (0.5, 0.5); scale is tiling (higher = more repeats).
vec2 transformUV(vec2 uv, vec2 offset, vec2 scale, float rotationDegrees, int wrapMode) {
    vec2 centred = (uv - 0.5) * scale;
    float rad = radians(rotationDegrees);
    float c = cos(rad), s = sin(rad);
    centred = mat2(c, s, -s, c) * centred;
    return wrapUV(centred + 0.5 + offset, wrapMode);
}


void main() {
    Material mat = materials[fragMaterialIndex];

    vec3 N = normalize(fragNormalWorld);
    vec3 surfaceNormal = N;
    if (mat.useNormalTexture != 0) {
        vec3 T = normalize(fragTangentWorld - N * dot(N, fragTangentWorld));
        vec3 B = cross(N, T);
        mat3 TBN = mat3(T, B, N);

        vec2 normalUV = transformUV(fragUV, mat.normalOffset, mat.normalScale, mat.normalRotation, mat.normalWrapMode);
        vec3 sampledNormal = texture(textures[nonuniformEXT(mat.normalTexIndex)], normalUV).rgb;
        sampledNormal = normalize(sampledNormal * 2.0 - 1.0);
        surfaceNormal = normalize(TBN * sampledNormal);
    }

    vec3 cameraPosWorld = ubo.invView[3].xyz;
    vec3 viewDirection = normalize(cameraPosWorld - fragPosWorld);

    // Metallic-roughness workflow: G = roughness, B = metalness (glTF convention)
    // when a combined MR texture is assigned; otherwise the material's own
    // scalar factors are used directly - either/or, not glTF's "always multiply".
    float roughness, metallic;
    if (mat.useMRTexture != 0) {
        vec2 mrUV = transformUV(fragUV, mat.mrOffset, mat.mrScale, mat.mrRotation, mat.mrWrapMode);
        vec3 mrSample = texture(textures[nonuniformEXT(mat.mrTexIndex)], mrUV).rgb;
        roughness = clamp(mrSample.g, 0.045, 1.0);
        metallic  = mrSample.b;
    } else {
        roughness = clamp(mat.roughness, 0.045, 1.0);
        metallic  = mat.metallic;
    }

    vec3 albedo = mat.baseColour.rgb;
    if (mat.useAlbedoTexture != 0) {
        vec2 albedoUV = transformUV(fragUV, mat.albedoOffset, mat.albedoScale, mat.albedoRotation, mat.albedoWrapMode);
        albedo = texture(textures[nonuniformEXT(mat.albedoTexIndex)], albedoUV).rgb;
    }
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Crude ambient fill (flat, non-directional). A stand-in until the indirect
    // illumination pass lands; also a manual floor when direct light drops to 0.
    vec3 colour = ubo.ambientLightColor.rgb * ubo.ambientLightColor.w * albedo;

    for (int i = 0; i < ubo.numLights; i++) {
        PointLight light = ubo.pointLights[i];

        vec3  toLight = light.position.xyz - fragPosWorld;
        float dist2   = dot(toLight, toLight);
        vec3  L       = toLight * inversesqrt(dist2);
        vec3  H       = normalize(viewDirection + L);
        float NdotL   = max(dot(surfaceNormal, L), 0.0);

        // Physical inverse-square, then a smooth window so the light's influence
        // reaches exactly 0 at its range (a perf/authoring bound, not physics).
        float range  = light.position.w;
        float atten  = 1.0 / max(dist2, 1e-4);
        float w      = clamp(1.0 - (dist2 * dist2) / (range * range * range * range), 0.0, 1.0);
        atten       *= w * w;

        // colour.w is luminous power (lm); isotropic point light -> I = Phi / 4pi.
        vec3 radiance = light.colour.rgb * (light.colour.w / (4.0 * PI)) * atten;

        // Cook–Torrance specular
        float NDF = DistributionGGX(surfaceNormal, H, roughness);
        float G   = GeometrySmith(surfaceNormal, viewDirection, L, roughness);
        vec3  F   = FresnelSchlick(max(dot(H, viewDirection), 0.0), F0);
        vec3 specular = (NDF * G * F) /
                        (4.0 * max(dot(surfaceNormal, viewDirection), 0.0) * NdotL + 1e-4);

        // Lambertian diffuse, energy-split against specular, no diffuse for metals.
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        vec3 diffuse = kD * albedo / PI;

        colour += (diffuse + specular) * radiance * NdotL;
    }

    outColour = vec4(colour, 1.0);
}