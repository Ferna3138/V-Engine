// ImGui_Setup.cpp
#include "ImGui_Setup.hpp"
#include "Components/Components.hpp"
#include <imgui/imgui.h>

void SceneHierarchyPanel::draw() {
    ImGui::Begin("Outliner");

    auto& registry = scene.getRegistry();
    auto view = registry.view<TransformComponent>();

    for (auto entity : view) {
        ImGui::PushID(static_cast<int>(entt::to_integral(entity)));
        
        std::string name = "Entity";
        if (auto* tag = registry.try_get<TagComponent>(entity)){
            name = tag->name;
        }

        std::string combinedLabel = name;
        if (registry.all_of<PointLightComponent>(entity)) {
            combinedLabel += " (Point Light)";
        } else if (registry.all_of<SpotLightComponent>(entity)) {
            combinedLabel += " (Spot Light)";
        } else if (registry.all_of<DirectionalLightComponent>(entity)) {
            combinedLabel += " (Directional Light)";
        } else if (registry.all_of<CameraComponent>(entity)) {
            combinedLabel += " (Camera)";
        } else if (registry.all_of<ModelComponent>(entity)) {
            combinedLabel += " (Mesh)";
        }

        bool isSelected = (selectedEntity == entity);
        if(ImGui::Selectable(combinedLabel.c_str(), isSelected))
            selectedEntity = entity;

        ImGui::PopID();
    }

    ImGui::End();
    
    drawInspector();

    if (entityToDelete != entt::null) {
        scene.getRegistry().destroy(entityToDelete);
        entityToDelete = entt::null;
    }
}

void SceneHierarchyPanel::drawInspector(){
    ImGui::Begin("Inspector");
    if (selectedEntity == entt::null || !scene.getRegistry().valid(selectedEntity)) {
        ImGui::Text("No entity selected.");
        ImGui::End();
        return;
    }

    auto& registry = scene.getRegistry();
    auto& transform = registry.get<TransformComponent>(selectedEntity);
    if (cachedRotationEntity != selectedEntity) {
        editedEulerDegrees = glm::degrees(glm::eulerAngles(transform.rotation));
        cachedRotationEntity = selectedEntity;
    }
    
    if (auto* tag = registry.try_get<TagComponent>(selectedEntity)) 
        ImGui::Text("%s", tag->name.c_str());
    
    ImGui::SliderFloat3("Position", &transform.translation.x, -10.0f, 10.0f);
    
    if (ImGui::DragFloat3("Rotation", &editedEulerDegrees.x, 0.5f, -180.f, 180.f))
        transform.rotation = glm::quat(glm::radians(editedEulerDegrees));    

    if (auto* light = registry.try_get<PointLightComponent>(selectedEntity)) {
        ImGui::Separator();
        ImGui::ColorEdit3("Color", &light->colour.x);
        ImGui::SliderFloat("Intensity", &light->colour.w, 0.0f, 50.0f);
        ImGui::DragFloat("Radius", &light->radius, 0.01f, 0.0f, 5.0f);
    }
    
    if (auto* camera = registry.try_get<CameraComponent>(selectedEntity)){
        ImGui::Separator();
    }
    
    if (auto* model = registry.try_get<ModelComponent>(selectedEntity)){
        ImGui::SliderFloat3("Scale", &transform.scale.x, 0.0f, 10.0f);
        ImGui::Separator();
    }

    if (ImGui::Button("Delete")) {
        entityToDelete = selectedEntity;
    }

    ImGui::End();

}