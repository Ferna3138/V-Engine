// ImGui_Setup.cpp
#include "Application/Editor/ImGui_Setup.hpp"
#include "Application/Scene/Components.hpp"
#include "Application/Scene/Scene_Serializer.hpp"

#include "Rendering/RHI/Device.hpp"
#include "Rendering/RHI/Swap_Chain.hpp"
#include "Rendering/Core/Renderer.hpp"
#include "Foundation/Platform/Window.hpp"
#include "Foundation/Platform/FileDialog.hpp"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>

// Editor UI

EditorUI::EditorUI(Device& device, Window& window, Renderer& renderer,
                   Scene& scene, ModelImporter& modelImporter, TextureManager& textureManager,
                   MaterialManager& materialManager, std::string scenePath, std::string iniPath)
    : device{device}, window{window}, renderer{renderer}, scene{scene}, modelImporter{modelImporter},
      textureManager{textureManager}, materialManager{materialManager},
      scenePath{std::move(scenePath)}, iniPath{std::move(iniPath)}, hierarchy{scene, textureManager, materialManager} {
    initBackend();
}

EditorUI::~EditorUI() {
    ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
    hierarchy.releaseThumbnails();  // must run before Shutdown() invalidates the backend
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

    // Drag-and-drop: a single image dropped on a material's texture-slot widget
    // swaps that texture; otherwise, model files are filtered client-side and
    // queued for import (an accidental drop of unrelated files shouldn't spam
    // worker tasks/logs).
    window.setDropCallback([this](const std::vector<std::string>& paths) {
        if (paths.size() == 1) {
            std::string ext = std::filesystem::path(paths[0]).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
                double x, y;
                glfwGetCursorPos(window.getGLFWwindow(), &x, &y);
                if (hierarchy.trySwapTextureAtCursor(paths[0], ImVec2(static_cast<float>(x), static_cast<float>(y))))
                    return;
            }
        }

        for (const auto& path : paths) {
            std::string ext = std::filesystem::path(path).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
            if (ext == ".obj" || ext == ".gltf" || ext == ".glb")
                modelImporter.requestImport(path);
        }
    });

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
                vengine::saveScene(scenePath, scene, materialManager);
            if (ImGui::MenuItem("Reload Scene"))
                reloadRequested = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Import Model...")) {
                if (auto path = vengine::openModelFileDialog())
                    modelImporter.requestImport(*path);
            }
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
        vengine::saveScene(scenePath, scene, materialManager);

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

// Scene Hierarchy Panel

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
        if (renamingEntity == entity) {
            if (renameFocusPending) {
                ImGui::SetKeyboardFocusHere();
                renameFocusPending = false;
            }
            ImGui::SetNextItemWidth(-FLT_MIN);
            bool committed = ImGui::InputText("##rename", &renameBuffer, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            bool cancelled = ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
            if (committed || (ImGui::IsItemDeactivated() && !cancelled)) {
                if (!renameBuffer.empty())
                    registry.get_or_emplace<TagComponent>(entity).name = renameBuffer;
                renamingEntity = entt::null;
            } else if (cancelled) {
                renamingEntity = entt::null;
            }
        } else {
            if (ImGui::Selectable(combinedLabel.c_str(), isSelected))
                selectedEntity = entity;
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                selectedEntity = entity;
                renamingEntity = entity;
                renameBuffer = name;
                renameFocusPending = true;
            }
        }

        ImGui::PopID();
    }

    // Click in empty space within the Outliner deselects the current entity
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        selectedEntity = entt::null;

    ImGui::End();

    drawInspector();
}

VkDescriptorSet SceneHierarchyPanel::getOrCreateThumbnail(uint32_t slot) {
    if (auto it = thumbnailCache.find(slot); it != thumbnailCache.end())
        return it->second;

    Texture* tex = textureManager.getTexture(slot);
    // Not ready yet (still streaming in): the sampler/view may not exist, or
    // the pixel data/layout may not be valid to sample. Don't cache a miss -
    // retry next frame, since it'll typically become ready within a frame or
    // two of being requested.
    if (!tex || !tex->isReady()) return VK_NULL_HANDLE;

    VkDescriptorSet set = ImGui_ImplVulkan_AddTexture(tex->getSampler(), tex->getImageView(),
                                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    thumbnailCache[slot] = set;
    return set;
}

void SceneHierarchyPanel::releaseThumbnails() {
    for (auto& [slot, set] : thumbnailCache)
        ImGui_ImplVulkan_RemoveTexture(set);
    thumbnailCache.clear();
}

void SceneHierarchyPanel::drawMaterialSection(Material& mat, uint32_t globalIndex) {
    // Shared by all three channels: a thumbnail (once the texture has
    // streamed in), the "Use Texture" toggle, the assigned file's name, a
    // Browse button, and recording this slot's rect for drag-and-drop.
    auto drawTextureSlot = [&](MaterialSlotChannel channel, bool& useTexture, uint32_t& texIndex,
                                std::string& texPath, VkFormat format) {
        ImGui::PushID(static_cast<int>(channel));
        ImGui::Checkbox("Use Texture", &useTexture);
        if (useTexture) {
            constexpr float kThumbnailSize = 48.0f;
            if (VkDescriptorSet thumb = getOrCreateThumbnail(texIndex); thumb != VK_NULL_HANDLE) {
                ImGui::Image((ImTextureID)thumb, ImVec2(kThumbnailSize, kThumbnailSize));

                // Magnifier: hovering the thumbnail shows a zoomed crop of
                // whichever part of the texture is under the cursor, not just
                // a bigger static copy.
                if (ImGui::IsItemHovered()) {
                    ImVec2 imageMin = ImGui::GetItemRectMin();
                    ImVec2 imageMax = ImGui::GetItemRectMax();
                    ImVec2 mousePos = ImGui::GetIO().MousePos;
                    float relX = (mousePos.x - imageMin.x) / (imageMax.x - imageMin.x);
                    float relY = (mousePos.y - imageMin.y) / (imageMax.y - imageMin.y);
                    relX = std::clamp(relX, 0.f, 1.f);
                    relY = std::clamp(relY, 0.f, 1.f);

                    constexpr float kZoomRegion = 1.0f;   // fraction of the texture shown (~2.2x zoom)
                    constexpr float kPreviewSize = 320.0f;
                    float uv0x = std::clamp(relX - kZoomRegion * 0.5f, 0.f, 1.f - kZoomRegion);
                    float uv0y = std::clamp(relY - kZoomRegion * 0.5f, 0.f, 1.f - kZoomRegion);

                    ImGui::BeginTooltip();
                    ImGui::Image((ImTextureID)thumb, ImVec2(kPreviewSize, kPreviewSize),
                                 ImVec2(uv0x, uv0y), ImVec2(uv0x + kZoomRegion, uv0y + kZoomRegion));
                    ImGui::EndTooltip();
                }

                ImGui::SameLine();
            }

            ImGui::BeginGroup();
            std::string label = texPath.empty() ? "(none)" : std::filesystem::path(texPath).filename().string();
            ImGui::TextUnformatted(label.c_str());
            lastTextureSlotRects.push_back({globalIndex, channel, ImGui::GetItemRectMin(), ImGui::GetItemRectMax()});
            if (ImGui::Button("Browse...")) {
                if (auto path = vengine::openImageFileDialog()) {
                    texIndex = textureManager.addTexture(*path, format);
                    texPath = *path;
                    useTexture = true;
                }
            }
            ImGui::EndGroup();
        }
        ImGui::PopID();
    };

    // Closed by default: a model can have many materials, and showing every
    // channel of every one at once gets overwhelming fast.
    if (!ImGui::CollapsingHeader(mat.name.empty() ? "Material" : mat.name.c_str()))
        return;

    ImGui::Text("Albedo");
    ImGui::ColorEdit3("Base Colour", &mat.baseColour.x);
    drawTextureSlot(MaterialSlotChannel::Albedo, mat.useAlbedoTexture, mat.albedoTexIndex, mat.albedoTexturePath,
                     VK_FORMAT_R8G8B8A8_SRGB);

    ImGui::Text("Metallic-Roughness");
    ImGui::SliderFloat("Metallic", &mat.metallic, 0.f, 1.f);
    ImGui::SliderFloat("Roughness", &mat.roughness, 0.f, 1.f);
    drawTextureSlot(MaterialSlotChannel::MetallicRoughness, mat.useMRTexture, mat.mrTexIndex, mat.mrTexturePath,
                     VK_FORMAT_R8G8B8A8_UNORM);

    ImGui::Text("Normal");
    drawTextureSlot(MaterialSlotChannel::Normal, mat.useNormalTexture, mat.normalTexIndex, mat.normalTexturePath,
                     VK_FORMAT_R8G8B8A8_UNORM);
}

bool SceneHierarchyPanel::trySwapTextureAtCursor(const std::string& imagePath, ImVec2 cursorPos) {
    for (const auto& rect : lastTextureSlotRects) {
        if (cursorPos.x < rect.rectMin.x || cursorPos.x > rect.rectMax.x ||
            cursorPos.y < rect.rectMin.y || cursorPos.y > rect.rectMax.y)
            continue;

        Material& mat = materialManager.getMaterial(rect.globalMaterialIndex);
        switch (rect.channel) {
            case MaterialSlotChannel::Albedo:
                mat.albedoTexIndex = textureManager.addTexture(imagePath, VK_FORMAT_R8G8B8A8_SRGB);
                mat.albedoTexturePath = imagePath;
                mat.useAlbedoTexture = true;
                break;
            case MaterialSlotChannel::MetallicRoughness:
                mat.mrTexIndex = textureManager.addTexture(imagePath, VK_FORMAT_R8G8B8A8_UNORM);
                mat.mrTexturePath = imagePath;
                mat.useMRTexture = true;
                break;
            case MaterialSlotChannel::Normal:
                mat.normalTexIndex = textureManager.addTexture(imagePath, VK_FORMAT_R8G8B8A8_UNORM);
                mat.normalTexturePath = imagePath;
                mat.useNormalTexture = true;
                break;
        }
        return true;
    }
    return false;
}

void SceneHierarchyPanel::drawInspector(){
    lastTextureSlotRects.clear();

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
        ImGui::Checkbox("Link Scale Axes", &uniformScaleLocked);
        glm::vec3 previousScale = transform.scale;
        if (ImGui::DragFloat3("Scale", &transform.scale.x, 0.05f, 0.0f, 10.0f) && uniformScaleLocked) {
            // Find the axis the drag actually moved, then carry the same ratio
            // (or, from zero, the same absolute value) over to the other two.
            for (int axis = 0; axis < 3; axis++) {
                if (transform.scale[axis] == previousScale[axis]) continue;
                bool fromZero = previousScale[axis] == 0.0f;
                float ratio = fromZero ? 0.0f : transform.scale[axis] / previousScale[axis];
                for (int other = 0; other < 3; other++) {
                    if (other == axis) continue;
                    transform.scale[other] = fromZero ? transform.scale[axis] : previousScale[other] * ratio;
                }
                break;
            }
        }
        ImGui::Separator();

        ImGui::SeparatorText("Materials");
        for (uint32_t globalIdx : model->model->getMaterialIndices()) {
            ImGui::PushID(static_cast<int>(globalIdx));
            drawMaterialSection(materialManager.getMaterial(globalIdx), globalIdx);
            ImGui::PopID();
        }
    }

    if (ImGui::Button("Delete")) {
        entityToDelete = selectedEntity;
    }

    ImGui::End();

}