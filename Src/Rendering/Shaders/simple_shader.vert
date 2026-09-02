#version 450

#include "shader_common.glsl"

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 colour;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;
layout(location = 4) in int textureIndex;
layout(location = 5) in int specularIndex;
layout(location = 6) in int normalIndex;
layout(location = 7) in vec3 tangent;



layout(location = 0) out vec3 fragColour;
layout(location = 1) out vec3 fragPosWorld;
layout(location = 2) out vec3 fragNormalWorld;
layout(location = 3) out vec2 fragUV;
layout(location = 4) flat out int fragTexIndex;
layout(location = 5) flat out int fragSpecIndex;
layout(location = 6) flat out int fragNormalIndex;
layout(location = 7) out vec3 fragTangentWorld;


layout(push_constant) uniform Push {
    mat4 modelMatrix; // Projection * View * Model
    mat4 normalMatrix; 
} push;


void main() {
    vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
    gl_Position = ubo.projection * ubo.view * positionWorld;

    fragNormalWorld = normalize(mat3(push.normalMatrix) * normal);
    fragPosWorld = positionWorld.xyz;
    fragColour = colour;
    fragUV = uv;
    fragTangentWorld = normalize(mat3(push.normalMatrix) * tangent);
    fragTexIndex = textureIndex;
    fragSpecIndex = specularIndex;
    fragNormalIndex = normalIndex;
    
}