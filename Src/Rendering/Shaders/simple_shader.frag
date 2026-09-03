#version 450
#extension GL_EXT_nonuniform_qualifier : enable

#include "shader_common.glsl"
#include "light_common.glsl"


layout(location = 0) in vec3 fragColour;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 3) in vec2 fragUV;
layout(location = 4) flat in int fragTexIndex;
layout(location = 5) flat in int fragMrIndex;
layout(location = 6) flat in int fragNormalIndex;
layout(location = 7) in vec3 fragTangentWorld;


layout(location = 0) out vec4 outColour;


layout(set = 1, binding = 0) uniform sampler2D textures[];


layout(push_constant) uniform Push {
    mat4 modelMatrix; // Projection * View * Model
    mat4 normalMatrix; 
} push;


void main() {
    vec3 diffuseLight = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
    vec3 specularLight = vec3(0.0);

    vec3 N = normalize(fragNormalWorld);
    vec3 T = normalize(fragTangentWorld - N * dot(N, fragTangentWorld));
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);

    vec3 sampledNormal = texture(textures[nonuniformEXT(fragNormalIndex)], fragUV).rgb;
    sampledNormal = normalize(sampledNormal * 2.0 - 1.0);
    vec3 surfaceNormal = normalize(TBN * sampledNormal);

    vec3 cameraPosWorld = ubo.invView[3].xyz;
    vec3 viewDirection = normalize(cameraPosWorld - fragPosWorld);

    // Metallic-roughness workflow: G = roughness, B = metalness (glTF convention).
    // Loaders bake OBJ shininess / glTF scalar factors into this texture, so every
    // surface samples the same way.
    vec3 mrSample = texture(textures[nonuniformEXT(fragMrIndex)], fragUV).rgb;
    float roughness = clamp(mrSample.g, 0.045, 1.0);
    float metallic  = mrSample.b;
    vec3 albedo = texture(textures[nonuniformEXT(fragTexIndex)], fragUV).rgb;
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    for (int i = 0; i < ubo.numLights; i++) {
        PointLight light = ubo.pointLights[i];
        vec3 L = light.position.xyz - fragPosWorld;
        float attenuation = 1.0 / dot(L, L);
        L = normalize(L);

        vec3 H = normalize(viewDirection + L);

        float NdotL = max(dot(surfaceNormal, L), 0.0);

        vec3 radiance = light.colour.xyz * light.colour.w * attenuation * 5.0;

        // Cook–Torrance BRDF with GGX
        float NDF = DistributionGGX(surfaceNormal, H, roughness);
        float G   = GeometrySmith(surfaceNormal, viewDirection, L, roughness);
        vec3  F   = FresnelSchlick(max(dot(H, viewDirection), 0.0), F0);

        vec3 numerator   = NDF * G * F;
        float denominator = 4.0 * max(dot(surfaceNormal, viewDirection), 0.0) * NdotL + 0.0001;
        vec3 specular = numerator / denominator;

        // Energy conservation
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        // Accumulate lighting
        diffuseLight  += kD * radiance * NdotL;
        specularLight += specular * radiance * NdotL;
    }
    
    outColour = vec4(diffuseLight * albedo + specularLight, 1.0);
}