# Scene & Editor

`Src/Application/` — the only layer that knows about entt. It owns the ECS, the
JSON scene format, camera input, and the ImGui editor.

## ECS: `Scene`

`Scene` (`Scene/Scene.hpp`) wraps an `entt::registry` and provides factory
helpers:

| Method | Components attached |
|---|---|
| `createEntity(name)` | `TransformComponent`, `TagComponent` |
| `createModelEntity(model, sourcePath)` | + `ModelComponent` |
| `createPointLight(colour, radius)` | + `PointLightComponent` |
| `createSpotLight(...)` | + `SpotLightComponent` |
| `createAreaLight(...)` | + `AreaLightComponent` |
| `createDirectionalLight(...)` | + `DirectionalLightComponent` |

`update(dt)` is the per-frame logic hook (currently just `updateLights`, a
stub).

### Components (`Scene/Components.hpp`)

| Component | Fields |
|---|---|
| `TagComponent` | `std::string name` |
| `TransformComponent` | `vec3 translation`, `vec3 scale`, `quat rotation`. Free functions `toMat4()` and `toNormalMatrix()` derive the matrices. |
| `ModelComponent` | `shared_ptr<Model> model`, `string sourcePath` (root-relative file it was loaded from, kept for serialization) |
| `PointLightComponent` | `vec4 colour` (`.w` = intensity), `float radius` |
| `SpotLightComponent` | `colour`, `radius`, `innerCutoff`, `outerCutoff` |
| `AreaLightComponent` | `colour`, `width`, `height` |
| `DirectionalLightComponent` | `colour`, `direction` |
| `CameraComponent` | `Camera camera`, `CamParameters cameraParams`, `CameraModel cameraModel` |

Only `PointLightComponent`, `ModelComponent`, and `CameraComponent` are wired
through the renderer and serializer today; the other light types are defined but
not yet consumed.

## The render-scene seam

`Scene::buildRenderScene(RenderScene& out)` is called once per frame, before
`renderer.beginFrame()`. It:

- clears `out`;
- iterates `view<TransformComponent, ModelComponent>` → one `RenderObject`
  (model matrix, normal matrix, raw `Model*`) per entity with a valid model;
- iterates `view<TransformComponent, PointLightComponent>` → one `RenderLight`.

This flat struct is the entire contract the renderer sees — see
[Architecture § The seams](architecture.md).

## JSON scenes

`vengine::loadScene` / `vengine::saveScene`
([`Scene/Scene_Serializer.cpp`](../Src/Application/Scene/Scene_Serializer.cpp),
nlohmann/json). The active scene is `Scenes/sponza.json`.

### Format

```json
{
  "version": 1,
  "camera": {
    "model": "Pinhole",
    "transform": { "translation": [0,1.5,0.5], "rotationEuler": [0,0,0] },
    "params": { "fov": 50.0 }
  },
  "entities": [
    { "name": "Sponza", "model": "Models/GLTF/Sponza/glTF/Sponza.gltf",
      "transform": { "translation": [0,0,0], "rotationEuler": [0,0,0], "scale": [1,1,1] } },

    { "name": "Light Red", "pointLight": { "colour": [0.956,0.262,0.211,1.0], "radius": 0.2 },
      "transform": { "translation": [-1.0,1.5,-1.0] } }
  ]
}
```

- **Rotations are Euler degrees in the file**, converted to/from `glm::quat` on
  load/save.
- An entity is a **model** if it has a `model` key, a **point light** if it has
  a `pointLight` key, otherwise a plain transform node.
- Missing model files are logged and the entity is skipped (load continues).
- On save, a `ModelComponent` with an empty `sourcePath` (procedurally built) is
  skipped — nothing to reference.
- `camParams` round-trips the full physically based camera description even
  though only `fov` is used for the projection today.

### The camera entity

The serializer **creates** the camera entity. `FirstApp::resolveOrCreateCamera()`
finds it via `view<CameraComponent>()` and falls back to a default camera if the
file had none. It is held by pointer so it survives a scene reload.

## Input: `KeyboardMovementController`

`Input/Keyboard_Movement_Controller.hpp`. Operates directly on an entity's
`TransformComponent`:

- `moveInPlaneXZ(window, dt, registry, entity)` — WASD + E/Q, yaw-relative.
- `mouseMove(window, dt, registry, entity)` — right-mouse-drag look; accumulates
  `yaw` / `pitch` and writes back a quaternion.
- `bindScrollCallback(window)` — static scroll callback.

Tunables: `moveSpeed = 3`, `lookSpeed = 1.5`, remappable `KeyMappings`.

## Editor (ImGui)

`ImGui` is docking-enabled and lives in the Application layer only.

### Setup (`FirstApp::setUpImgui`)

Own descriptor pool, GLFW + Vulkan backends, `RenderPass =
renderer.getSwapChainRenderPass()` (ImGui draws in the UI-only swapchain pass
after the frame-graph blit). The layout file is
`Src/Application/Apps/FirstApp/imgui.ini`, resolved against the project root.

### Windows

| Window | Source | Contents |
|---|---|---|
| **DockSpace** host | `FirstApp::renderUI` | Full-viewport, borderless, `MenuBar` flag; hosts the main menu bar + dockspace |
| **Main menu bar** | `FirstApp::drawMainMenuBar` | **File**: Save Scene (`Ctrl+S`, also a global shortcut), Reload Scene, Exit. **Edit**: Undo/Redo (disabled stubs). **View**: Visualise Point Lights toggle |
| **Control Panel** | `FirstApp::renderUI` | FPS / ms-per-frame readout, then the scene hierarchy panel |
| **Scene Hierarchy** | `Editor/ImGui_Setup` — `SceneHierarchyPanel` | Lists entities, selection, delete, and an inspector that edits the selected entity's transform (Euler degrees, cached per entity to avoid quaternion drift while dragging) |

### Scene reload

**File ▸ Reload Scene** sets `sceneReloadRequested`. The run loop handles it at
the top of the next iteration: `vkDeviceWaitIdle` → `registry.clear()` →
`loadGameObjects()` → rebind the camera pointer → reset the controller.
