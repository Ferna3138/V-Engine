// ImGui_Setup.cpp
#include "Application/Editor/ImGui_Setup.hpp"
#include "Application/Scene/Components.hpp"
#include "Application/Scene/Scene_Serializer.hpp"

#include "Rendering/RHI/Device.hpp"
#include "Rendering/RHI/Swap_Chain.hpp"
#include "Rendering/Core/Renderer.hpp"
#include "Foundation/Platform/Window.hpp"

#include <imgui/imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <utility>

// ============================ EditorUI ============================

EditorUI::EditorUI(Device& device, Window& window, Renderer& renderer,
                   Scene& scene, std::string scenePath, std::string iniPath)
    : device{device}, window{window}, renderer{renderer}, scene{scene},
      scenePath{std::move(scenePath)}, iniPath{std::move(iniPath)}, hierarchy{scene} {
    initBackend();
}

EditorUI::~EditorUI() {
    ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(device.device(), pool, nullptr);
}

void EditorUI::initBackend() {
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;
    vkCreateDescriptorPool(device.device(), &pool_info, nullptr, &pool);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = iniPath.c_str();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 1.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForVulkan(window.getGLFWwindow(), true);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = device.getInstance();
    init_info.PhysicalDevice = device.getPhysicalDevice();
    init_info.Device = device.device();
    init_info.Queue = device.graphicsQueue();
    init_info.DescriptorPool = pool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = SwapChain::MAX_FRAMES_IN_FLIGHT;
    init_info.UseDynamicRendering = false;
    init_info.RenderPass = renderer.getSwapChainRenderPass();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&init_info);
}

bool EditorUI::consumeSceneReloadRequest() {
    bool r = reloadRequested;
    reloadRequested = false;
    return r;
}

void EditorUI::draw() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    drawDockspaceAndMenu();
    drawControlPanel();

    ImGui::Render();
}

void EditorUI::recordDrawData(VkCommandBuffer cb) {
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cb);
}

void EditorUI::drawDockspaceAndMenu() {
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_MenuBar;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    window_flags |= ImGuiWindowFlags_NoBackground;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(2);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
                vengine::saveScene(scenePath, scene);
            if (ImGui::MenuItem("Reload Scene"))
                reloadRequested = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
                glfwSetWindowShouldClose(window.getGLFWwindow(), GLFW_TRUE);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
            ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Visualise Point Lights", nullptr, &visualisePointLights);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
        vengine::saveScene(scenePath, scene);

    ImGuiID dockspace_id = ImGui::GetID("MyDockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
}

void EditorUI::drawControlPanel() {
    ImGui::Begin("Control Panel");

    float fps = ImGui::GetIO().Framerate;
    ImGui::Text("FPS: %.1f (%.3f ms/frame)", fps, 1000.0f / fps);
    ImGui::Separator();
    ImGui::ColorEdit3("Background", &backgroundColour.x);

    hierarchy.draw();
    ImGui::End();
}

// ======================= SceneHierarchyPanel =======================

namespace {
struct SensorPreset { const char* name; float width; float height; };  // mm
constexpr SensorPreset kSensorPresets[] = {
    {"IMAX 65mm",          70.41f, 52.63f},
    {"Medium Format",      43.80f, 32.90f},
    {"Full Frame",         36.00f, 24.00f},
    {"Super 35",           24.89f, 18.66f},
    {"APS-C",              23.60f, 15.70f},
    {"Micro Four Thirds",  17.30f, 13.00f},
};
}


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
        ImGui::DragFloat("Power (lm)", &light->colour.w, 25.0f, 0.0f, 50000.0f, "%.0f",
                         ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Range", &light->range, 0.1f, 0.1f, 200.0f, "%.1f",
                         ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Radius", &light->radius, 0.01f, 0.0f, 5.0f, "%.2f",
                         ImGuiSliderFlags_AlwaysClamp);
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

            {
                auto& cp = camera->cameraParams;
                int cur = -1;
                for (int i = 0; i < IM_ARRAYSIZE(kSensorPresets); ++i)
                    if (std::fabs(cp.sensor_width - kSensorPresets[i].width) < 0.02f &&
                        std::fabs(cp.sensor_height - kSensorPresets[i].height) < 0.02f) { cur = i; break; }
                const char* preview = cur >= 0 ? kSensorPresets[cur].name : "Custom";
                if (ImGui::BeginCombo("Sensor Preset", preview)) {
                    for (int i = 0; i < IM_ARRAYSIZE(kSensorPresets); ++i)
                        if (ImGui::Selectable(kSensorPresets[i].name, i == cur)) {
                            cp.sensor_width  = kSensorPresets[i].width;
                            cp.sensor_height = kSensorPresets[i].height;
                        }
                    ImGui::EndCombo();
                }
            }
            ImGui::DragFloat("Sensor Width", &camera->cameraParams.sensor_width, 0.1f, 1.f, 80.f, "%.2f", clamp);
            ImGui::DragFloat("Sensor Height", &camera->cameraParams.sensor_height, 0.1f, 1.f, 80.f, "%.2f", clamp);
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