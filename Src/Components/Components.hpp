#pragma once

#include "Renderer/Model.hpp"
#include <memory>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <Systems/Camera.hpp>

struct TransformComponent{
        glm::vec3 translation{};
        glm::vec3 scale{1.f, 1.f, 1.f};
        glm::vec3 rotation{0.f}; // in radians
};

inline glm::mat4 toMat4(const TransformComponent &transform) {
    glm::mat4 translation = glm::translate(glm::mat4{1.0f}, transform.translation);
    glm::mat4 rotation = glm::eulerAngleYXZ(transform.rotation.y, transform.rotation.x, transform.rotation.z);
    glm::mat4 scale = glm::scale(glm::mat4{1.0f}, transform.scale);

    return translation * rotation * scale;
}

inline glm::mat3 toNormalMatrix(const TransformComponent &transform) {
    glm::mat4 model = toMat4(transform);
    return glm::mat3(glm::transpose(glm::inverse(model)));
}

struct ModelComponent {
    std::shared_ptr<Model> model{};
};


struct MaterialComponent {
    glm::vec3 baseColor{1.0f};     // fallback color if no texture
    float metallic{0.0f};          // PBR support
    float roughness{1.0f};         // PBR support
    int textureIndex{-1};          // index into TextureManager (diffuse/albedo)
    int normalMapIndex{-1};        // normal map (optional)
    int roughnessMapIndex{-1};     // roughness map (optional)
};

// Optional: only if you want textures directly on entities
struct TextureComponent {
    int textureID{-1}; // index in TextureManager
};

// Lighting
struct PointLightComponent {
    glm::vec4 colour;
    float radius;
};

struct CameraComponent {
    Camera camera;
};