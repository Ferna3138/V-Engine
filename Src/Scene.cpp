#include "Scene.hpp"

// You can add more systems here
Scene::Scene() { }

entt::entity Scene::createEntity(const std::string& name) {
    auto entity = registry.create();
    registry.emplace<TransformComponent>(entity);
    // Optional: store the name in a NameComponent if you want to implement that
    registry.emplace<TagComponent>(entity, TagComponent{name});
    return entity;
}


// Lights
entt::entity Scene::createAreaLight(glm::vec4 colour, float width, float height) {
    auto entity = createEntity("AreaLight");
    registry.emplace<AreaLightComponent>(entity, AreaLightComponent{colour, width, height});
    return entity;
}

entt::entity Scene::createSpotLight(glm::vec4 colour, float radius, float innerCutoff, float outerCutoff) {
    auto entity = createEntity("SpotLight");
    registry.emplace<SpotLightComponent>(entity, SpotLightComponent{colour, radius, innerCutoff, outerCutoff});
    return entity;
}

entt::entity Scene::createDirectionalLight(glm::vec4 colour, glm::vec3 direction) {
    auto entity = createEntity("DirectionalLight");
    registry.emplace<DirectionalLightComponent>(entity, DirectionalLightComponent{colour, direction});
    return entity;
}

entt::entity Scene::createPointLight(glm::vec4 colour, float radius) {
    auto entity = createEntity("PointLight");
    registry.emplace<PointLightComponent>(entity, PointLightComponent{colour, radius});
    return entity;
}


entt::entity Scene::createModelEntity(std::shared_ptr<Model> model) {
    auto entity = createEntity("Model");
    registry.emplace<ModelComponent>(entity, model);
    
    return entity;
}

void Scene::update(float dt) {
    updateLights(dt);
    // Add more systems (animations, scripts, physics)
}

void Scene::render(VkCommandBuffer commandBuffer) {
    renderModels(commandBuffer);
}

void Scene::renderModels(VkCommandBuffer commandBuffer) {
    auto view = registry.view<TransformComponent, ModelComponent>();
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& modelComp = view.get<ModelComponent>(entity);

        modelComp.model->bind(commandBuffer);
        modelComp.model->draw(commandBuffer);
    }
}

void Scene::updateLights(float dt) {
    auto view = registry.view<TransformComponent, PointLightComponent>();
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& light = view.get<PointLightComponent>(entity);
        // Light animation or update uniforms
    }
}