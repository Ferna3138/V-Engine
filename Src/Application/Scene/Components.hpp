#pragma once

#include "Rendering/Resources/Model.hpp"
#include <memory>
#include <string>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/quaternion.hpp>
//#include <glm/gtx/quaternion.hpp>

#include "Rendering/Core/Camera.hpp"

struct TagComponent {
    std::string name = "Entity";
};

struct TransformComponent{
        glm::vec3 translation{};
        glm::vec3 scale{1.f, 1.f, 1.f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // in radians
};

inline glm::mat4 toMat4(const TransformComponent &transform) {
    glm::mat4 translation = glm::translate(glm::mat4{1.0f}, transform.translation);
    glm::mat4 rotation = glm::mat4_cast(transform.rotation);
    glm::mat4 scale = glm::scale(glm::mat4{1.0f}, transform.scale);

    return translation * rotation * scale;
}

inline glm::mat3 toNormalMatrix(const TransformComponent &transform) {
    glm::mat4 model = toMat4(transform);
    return glm::mat3(glm::transpose(glm::inverse(model)));
}

struct ModelComponent {
    std::shared_ptr<Model> model{};
    std::string sourcePath{};
    bool visible = true;   // toggled from the Outliner; hidden models are skipped when building the render scene
};

// Lighting
struct SpotLightComponent {
    glm::vec4 colour;
    float radius;
    float innerCutoff;
    float outerCutoff;
    bool visible = true;
};

struct AreaLightComponent {
    glm::vec4 colour;
    float width;
    float height;
    bool visible = true;
};

struct DirectionalLightComponent {
    glm::vec4 colour;
    glm::vec3 direction;
    bool visible = true;
};

struct PointLightComponent {
    glm::vec4 colour;      // rgb + luminous power (lumens) in .w
    float radius;          // emitter/billboard size
    float range = 20.f;    // influence cutoff distance (authoring/perf bound, not physical)
    bool visible = true;
};

// Camera
struct CameraComponent {
    Camera camera;
    CamParameters cameraParams;
    CameraModel cameraModel = CameraModel::Pinhole;
};