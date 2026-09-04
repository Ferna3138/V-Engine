#pragma once

#include <entt.hpp>
#include "Application/Scene/Components.hpp"
#include "Rendering/Core/Render_Scene.hpp"

#include <string>

class Scene {
public:
    Scene();

    entt::entity createEntity(const std::string& name = "Entity");

    // Lights
    entt::entity createSpotLight(glm::vec4 colour, float radius, float innerCutoff, float outerCutoff);
    entt::entity createAreaLight(glm::vec4 colour, float width, float height);
    entt::entity createDirectionalLight(glm::vec4 colour, glm::vec3 direction);
    entt::entity createPointLight(glm::vec4 colour, float radius, float range = 20.f);

    entt::entity createModelEntity(std::shared_ptr<Model> model, std::string sourcePath = {});

    void update(float dt);      // Call per-frame for logic

    // Walks the ECS and produces the flat, renderer-facing view of the frame.
    // This is the only place the component layout is translated for Rendering.
    void buildRenderScene(RenderScene& out);

    entt::registry& getRegistry() { return registry; }
    const entt::registry& getRegistry() const { return registry; }

private:
    entt::registry registry;

    void updateLights(float dt);
};
