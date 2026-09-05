#pragma once

#include <entt.hpp>
#include "Application/Scene/Scene.hpp"
#include "Rendering/Resources/MaterialManager.hpp"
#include "Rendering/Resources/ModelImporter.hpp"
#include "Rendering/Resources/TextureManager.hpp"

#include <imgui/imgui.h>
#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <vector>

class Device;
class Window;
class Renderer;

class SceneHierarchyPanel {
public:
    SceneHierarchyPanel(Scene& scene, TextureManager& textureManager, MaterialManager& materialManager)
        : scene{scene}, textureManager{textureManager}, materialManager{materialManager} {}
    void draw();

    // Which texture slot a drag-and-drop image should land on, hit-tested
    // against the rects drawn during the most recent draw() call. Returns
    // true and performs the swap if the drop landed on a slot.
    bool trySwapTextureAtCursor(const std::string& imagePath, ImVec2 cursorPos);

    // Selects an entity (e.g. one just created by a background model import)
    // as if the user had clicked it in the Outliner.
    void selectEntity(entt::entity entity) { selectedEntity = entity; }

    // True once after the user clicks Delete on the selected entity; consuming
    // clears it. Destruction itself is the caller's job - it must happen
    // outside of draw() (before this frame's render list is built, and after a
    // device.waitIdle() so any in-flight command buffer referencing the
    // entity's GPU resources has finished) rather than immediately here, or
    // the just-submitted frame renders a dangling Model pointer into buffers
    // that got destroyed out from under it.
    entt::entity consumeEntityToDelete() {
        entt::entity e = entityToDelete;
        entityToDelete = entt::null;
        return e;
    }

    // Releases every cached ImGui texture handle (see thumbnailCache). Must be
    // called by EditorUI's destructor BEFORE ImGui_ImplVulkan_Shutdown() - the
    // handles are meaningless (and unsafe to free) once the backend is torn
    // down, and SceneHierarchyPanel's own destructor runs too late for that
    // (members are destroyed after EditorUI's destructor body already ran).
    void releaseThumbnails();

private:
    Scene& scene;
    TextureManager& textureManager;
    MaterialManager& materialManager;

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

    enum class MaterialSlotChannel { Albedo, MetallicRoughness, Normal };
    struct TextureSlotRect {
        uint32_t globalMaterialIndex;
        MaterialSlotChannel channel;
        ImVec2 rectMin, rectMax;
    };
    // Repopulated every drawInspector() call; used by trySwapTextureAtCursor()
    // to figure out which texture slot (if any) a drag-and-drop image landed
    // on, since GLFW's drop callback only reports a window-level cursor
    // position, not which ImGui widget was under it.
    std::vector<TextureSlotRect> lastTextureSlotRects;

    // Bindless texture slot -> ImGui texture handle (a VkDescriptorSet, per
    // the Vulkan backend's ImTextureID convention), lazily created the first
    // time a slot's thumbnail is drawn and kept for the panel's lifetime -
    // hot-swapping a texture always allocates a fresh slot (see TextureManager)
    // rather than overwriting an existing one, so a cached entry never goes
    // stale pointing at the wrong image.
    std::unordered_map<uint32_t, VkDescriptorSet> thumbnailCache;
    VkDescriptorSet getOrCreateThumbnail(uint32_t slot);

    void drawMaterialSection(Material& mat, uint32_t globalIndex);
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
             Scene& scene, ModelImporter& modelImporter, TextureManager& textureManager,
             MaterialManager& materialManager, std::string scenePath, std::string iniPath);
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

    // Forwarded to the Outliner's pending delete (see SceneHierarchyPanel::consumeEntityToDelete).
    entt::entity consumeEntityToDelete() { return hierarchy.consumeEntityToDelete(); }

private:
    void initBackend();
    void drawDockspaceAndMenu();
    void drawControlPanel();

    Device&   device;
    Window&   window;
    Renderer& renderer;
    Scene&    scene;
    ModelImporter& modelImporter;
    TextureManager& textureManager;
    MaterialManager& materialManager;
    std::string scenePath;

    std::string iniPath;                 // backs ImGuiIO::IniFilename, must outlive the context
    VkDescriptorPool pool = VK_NULL_HANDLE;
    bool reloadRequested = false;

    SceneHierarchyPanel hierarchy;
};
