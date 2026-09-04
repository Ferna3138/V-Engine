#pragma once

#include <entt.hpp>
#include "Application/Scene/Scene.hpp"
#include "Rendering/Resources/ModelImporter.hpp"

#include <vulkan/vulkan.h>
#include <string>

class Device;
class Window;
class Renderer;

class SceneHierarchyPanel {
public:
    explicit SceneHierarchyPanel(Scene& scene) : scene{scene} {}
    void draw();

    // Selects an entity (e.g. one just created by a background model import)
    // as if the user had clicked it in the Outliner.
    void selectEntity(entt::entity entity) { selectedEntity = entity; }

private:
    Scene& scene;
    entt::entity selectedEntity{entt::null};
    entt::entity entityToDelete{entt::null};

    entt::entity renamingEntity{entt::null};
    std::string renameBuffer;
    bool renameFocusPending = false;

    entt::entity cachedRotationEntity{entt::null};
    glm::vec3 editedEulerDegrees{0.f};

    // When true, dragging one axis of the Scale slider scales all three
    // together (proportionally to the axis being dragged).
    bool uniformScaleLocked = false;
    void drawInspector();
};

// Owns the whole ImGui integration: Vulkan/GLFW backend init + teardown, the
// dockspace, main menu bar and editor panels. An app constructs one, then per
// frame calls draw() (builds the UI, finalises draw data) and later, inside its
// swapchain render pass, recordDrawData(cb). Everything ImGui lives here so the
// app stays free of it.
class EditorUI {
public:
    // scenePath: file used by the Save/Reload menu items.
    // iniPath:   where ImGui persists its layout (pass an absolute/resolved path).
    EditorUI(Device& device, Window& window, Renderer& renderer,
             Scene& scene, ModelImporter& modelImporter, std::string scenePath, std::string iniPath);
    ~EditorUI();

    EditorUI(const EditorUI&) = delete;
    EditorUI& operator=(const EditorUI&) = delete;

    void draw();                              // NewFrame -> panels -> ImGui::Render
    void recordDrawData(VkCommandBuffer cb);  // must run inside an active render pass

    // App-observable UI state.
    bool      visualisePointLights = true;
    glm::vec3 backgroundColour{0.02f, 0.02f, 0.02f};
    bool      consumeSceneReloadRequest();    // true once after the user asks to reload

    // Forwarded to the Outliner so a freshly-imported entity shows up selected.
    void selectEntity(entt::entity entity) { hierarchy.selectEntity(entity); }

private:
    void initBackend();
    void drawDockspaceAndMenu();
    void drawControlPanel();

    Device&   device;
    Window&   window;
    Renderer& renderer;
    Scene&    scene;
    ModelImporter& modelImporter;
    std::string scenePath;

    std::string iniPath;                 // backs ImGuiIO::IniFilename, must outlive the context
    VkDescriptorPool pool = VK_NULL_HANDLE;
    bool reloadRequested = false;

    SceneHierarchyPanel hierarchy;
};
