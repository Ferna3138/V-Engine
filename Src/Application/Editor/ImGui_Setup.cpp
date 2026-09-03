// ImGui_Setup.cpp
#include "Application/Editor/ImGui_Setup.hpp"
#include "Application/Scene/Components.hpp"
#include <imgui/imgui.h>
#include <algorithm>

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
    // Refresh the edited euler from the live transform whenever we're not mid-drag,
    // so flying the camera keeps the field current; hold it steady while dragging
    // to avoid eulerAngles() flipping under the user.
    if (cachedRotationEntity != selectedEntity || !ImGui::IsAnyItemActive()) {
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
        ImGui::DragFloat("Intensity", &light->colour.w, 1.0f, 0.0f, 50.0f);
        ImGui::DragFloat("Radius", &light->radius, 0.01f, 0.0f, 5.0f);
    }
    
    if (auto* camera = registry.try_get<CameraComponent>(selectedEntity)){
        ImGui::Separator();
        bool physical = (camera->cameraModel == CameraModel::Physical);
        if (ImGui::Checkbox("Physically Based", &physical))
            camera->cameraModel = physical ? CameraModel::Physical : CameraModel::Pinhole;

        constexpr ImGuiSliderFlags clamp = ImGuiSliderFlags_AlwaysClamp;

        if (camera->cameraModel == CameraModel::Physical) {
            ImGui::Text("FOV: %.1f deg", camera->cameraParams.fov);
            ImGui::DragFloat("Camera FPS", &camera->cameraParams.fps, 0.1f, 24.f, 1000.f, "%.1f", clamp);
            ImGui::DragFloat("Sensor Width", &camera->cameraParams.sensor_width, 0.1f, 1.f, 70.f, "%.1f", clamp);
            ImGui::DragFloat("Sensor Height", &camera->cameraParams.sensor_height, 0.1f, 1.f, 70.f, "%.1f", clamp);
            ImGui::DragFloat("Focal Length", &camera->cameraParams.focal_length, 0.1f, 8.f, 500.f, "%.1f", clamp);
            ImGui::DragFloat("Aperture", &camera->cameraParams.aperture, 0.1f, 1.f, 32.f, "%.2f", clamp);
            ImGui::DragFloat("Focus Distance", &camera->cameraParams.focus_distance, 0.1f, 0.1f, 150.f, "%.2f", clamp);
            ImGui::DragFloat("Shutter Angle", &camera->cameraParams.shutter_angle, 0.1f, 1.f, 360.f, "%.1f", clamp);
            ImGui::DragFloat("EV Comp", &camera->cameraParams.exposure, 0.1f, -5.f, 5.f, "%.2f", clamp);
            ImGui::DragInt("White Balance", &camera->cameraParams.white_balance, 5, 2500, 10000, "%d", clamp);
            ImGui::Text("ISO: %d (auto)", camera->cameraParams.iso);

            ImGui::SeparatorText("Depth of Field");
            ImGui::Checkbox("Enable DoF", &camera->cameraParams.dof_enabled);
            if (camera->cameraParams.dof_enabled) {
                ImGui::DragInt  ("Aperture Blades", &camera->cameraParams.aperture_blades, 1, 4, 9, "%d", clamp);
                ImGui::DragFloat("Blade Rotation",  &camera->cameraParams.blade_rotation, 1.0f, 0.f, 90.f, "%.1f", clamp);
                ImGui::DragInt  ("Bokeh Samples",   &camera->cameraParams.dof_samples, 1, 8, 64, "%d", clamp);
                ImGui::DragFloat("Max Blur (px)",   &camera->cameraParams.max_coc, 1.0f, 2.f, 60.f, "%.1f", clamp);
            }
            ImGui::SeparatorText("Motion Blur");
            ImGui::Checkbox("Enable Motion Blur", &camera->cameraParams.motion_blur_enabled);
            if (camera->cameraParams.motion_blur_enabled) {
                ImGui::DragInt  ("MB Samples", &camera->cameraParams.mb_samples, 1, 4, 32, "%d", clamp);
                ImGui::DragFloat("MB Max (px)", &camera->cameraParams.mb_max_px, 1.0f, 4.f, 128.f, "%.1f", clamp);
            }
        } else {
            ImGui::SliderFloat("FOV", &camera->cameraParams.fov, 10.f, 120.f, "%.1f", clamp);
        }
        ImGui::DragFloat("Near Plane", &camera->cameraParams.near_plane, 1.0f, 0.05f, 100.f, "%.2f", clamp);
        ImGui::DragFloat("Far Plane", &camera->cameraParams.far_plane, 1.0f, 0.05f, 1000.f, "%.1f", clamp);

        camera->cameraParams.near_plane = std::min(camera->cameraParams.near_plane, camera->cameraParams.far_plane - 0.01f);
        camera->cameraParams.near_plane = std::max(camera->cameraParams.near_plane, 0.01f);
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