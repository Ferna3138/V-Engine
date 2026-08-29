#pragma once

#include "Renderer/Model.hpp"
#include <memory>
#include <string>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <Systems/Camera.hpp>

struct TagComponent {
    std::string name = "Entity";
};

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

// Lighting
struct SpotLightComponent {
    glm::vec4 colour;
    float radius;
    float innerCutoff;
    float outerCutoff;
};

struct DirectionalLightComponent {
    glm::vec4 colour;
    glm::vec3 direction;
};

struct PointLightComponent {
    glm::vec4 colour;
    float radius;
};

// Camera
struct CameraComponent {
    Camera camera;
};