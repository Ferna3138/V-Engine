#version 450

#include "shader_common.glsl"

layout(location = 0) in vec3 fragColour;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;

layout(location = 0) out vec4 outColour;

layout(push_constant) uniform Push {
    mat4 modelMatrix; // Projection * View * Model
    mat4 normalMatrix; 
} push;



float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main() {
    vec3 surfaceNormal = normalize(fragNormalWorld);
    vec3 viewDir = normalize(ubo.invView[3].xyz - fragPosWorld);

    vec3 albedo = fragColour;
    float roughness = 0.5; // You could pass this per-material
    float metallic = 0.2;  // Also optional per-material

    vec3 F0 = mix(vec3(0.04), albedo, metallic); // Base reflectivity

    vec3 Lo = vec3(0.0);

    for (int i = 0; i < ubo.numLights; ++i) {
        PointLight light = ubo.pointLights[i];
        vec3 lightDir = light.position.xyz - fragPosWorld;
        float distance = length(lightDir);
        lightDir = normalize(lightDir);

        vec3 H = normalize(viewDir + lightDir);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = light.colour.xyz * light.colour.w * attenuation;

        // Cook-Torrance BRDF terms
        float NDF = DistributionGGX(surfaceNormal, H, roughness);
        float G = GeometrySmith(surfaceNormal, viewDir, lightDir, roughness);
        vec3 F = FresnelSchlick(max(dot(H, viewDir), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(surfaceNormal, viewDir), 0.0) * max(dot(surfaceNormal, lightDir), 0.0) + 0.001;
        vec3 specular = numerator / denominator;

        float NdotL = max(dot(surfaceNormal, lightDir), 0.0);
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // Ambient
    vec3 ambient = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w * albedo;

    vec3 color = ambient + Lo;
    outColour = vec4(color, 1.0);
}