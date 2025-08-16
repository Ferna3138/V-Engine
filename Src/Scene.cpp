#include "Scene.hpp"

// You can add more systems here
Scene::Scene() { }

entt::entity Scene::createEntity(const std::string& name) {
    auto entity = registry.create();
    registry.emplace<TransformComponent>(entity);
    // Optional: store the name in a NameComponent if you want to implement that
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