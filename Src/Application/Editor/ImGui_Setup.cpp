// ImGui_Setup.cpp
#include "Application/Editor/ImGui_Setup.hpp"
#include "Application/Scene/Components.hpp"
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

        // Visibility toggle on the left of the row (models and lights)
        bool* visible = nullptr;
        if (auto* model = registry.try_get<ModelComponent>(entity))                  visible = &model->visible;
        else if (auto* light = registry.try_get<PointLightComponent>(entity))        visible = &light->visible;
        else if (auto* light = registry.try_get<SpotLightComponent>(entity))         visible = &light->visible;
        else if (auto* light = registry.try_get<DirectionalLightComponent>(entity))  visible = &light->visible;
        else if (auto* light = registry.try_get<AreaLightComponent>(entity))         visible = &light->visible;
        if (visible) {
            ImGui::Checkbox("##visible", visible);
            ImGui::SameLine();
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

    // Click in empty space within the Outliner deselects the current entity
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        selectedEntity = entt::null;

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
    
    ImGui::DragFloat3("Position", &transform.translation.x, 0.1f, -50.0f, 50.0f);
    
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
        bool physical = (camera->cameraModel == CameraModel::Physical);
        if (ImGui::Checkbox("Physically Based", &physical))
            camera->cameraModel = physical ? CameraModel::Physical : CameraModel::Pinhole;

        if (camera->cameraModel == CameraModel::Physical) {
            ImGui::SliderFloat("Sensor Width", &camera->cameraParams.sensor_width, 0.f, 70.f);
            ImGui::SliderFloat("Sensor Height", &camera->cameraParams.sensor_height, 0.f, 70.f);
            ImGui::SliderFloat("Focal Length", &camera->cameraParams.focal_length, 0.f, 500.f);
            ImGui::SliderFloat("Aperture", &camera->cameraParams.aperture, 0.f, 32.f);
            ImGui::SliderFloat("Focus Distance", &camera->cameraParams.focus_distance, 0.f, 150.f);
            ImGui::SliderFloat("Shutter Speed", &camera->cameraParams.shutter_speed, 0.f, 360.f);
            ImGui::SliderFloat("Exposure", &camera->cameraParams.exposure, 0.f, 10.f);
            ImGui::SliderInt("White Balance", &camera->cameraParams.white_balance, 1000, 10000);
        } else {
            ImGui::SliderFloat("FOV", &camera->cameraParams.fov, 10.f, 120.f);
        }

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