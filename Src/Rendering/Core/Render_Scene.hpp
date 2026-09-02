#pragma once

// Flat, engine-facing view of everything to be drawn this frame. The Application
// layer walks its ECS once per frame and fills this in (see
// Scene::buildRenderScene); the renderer and its passes consume it and never
// touch entt / the component definitions. This is the seam that keeps
// Rendering independent of Application.

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"

#include <vector>

class Model;

struct RenderObject {
    glm::mat4 modelMatrix{1.f};
    glm::mat4 normalMatrix{1.f};   // mat3 in practice, padded to mat4 for the push block
    Model* model = nullptr;        // non-owning; the Scene keeps the model alive
};

struct RenderLight {
    glm::vec3 position{0.f};
    glm::vec4 colour{1.f};         // rgb + intensity in .w
    float radius = 0.f;
};

struct RenderScene {
    std::vector<RenderObject> objects;
    std::vector<RenderLight>  lights;

    void clear() {
        objects.clear();
        lights.clear();
    }
};
