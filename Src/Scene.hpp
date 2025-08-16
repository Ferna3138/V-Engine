#pragma once

#include <entt.hpp>
#include "Components/Components.hpp"
#include "Renderer/Texture_Manager.hpp"

class Scene {
public:
    Scene();

    entt::entity createEntity(const std::string& name = "Entity");
    entt::entity createPointLight(glm::vec4 colour, float radius);
    entt::entity createModelEntity(std::shared_ptr<Model> model);

    void update(float dt);      // Call per-frame for logic
    void render(VkCommandBuffer commandBuffer); // Call per-frame for rendering

    entt::registry& getRegistry() { return registry; }

private:
    entt::registry registry;


    // Add helper system methods if needed
    void renderModels(VkCommandBuffer commandBuffer);
    void updateLights(float dt);

    void createTextureImages(const std::vector<std::string> textureNames);


};