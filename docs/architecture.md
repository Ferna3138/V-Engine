# Architecture

## Three layers, three libraries

`Src/` is split into three layers. Each is compiled as its own CMake **static
library**, so the dependency direction is enforced by the linker, not just by
convention.

```
┌─────────────────────────────────────────────┐
│ Application        (V-Engine executable)     │  Src/Application/
│   Scene / ECS · Input · Editor · main()      │  — the only layer that sees entt
└───────────────────────┬─────────────────────┘
                        │ links
┌───────────────────────▼─────────────────────┐
│ Rendering          (vengine_rendering)       │  Src/Rendering/
│   RHI · Resources · FrameGraph · Passes      │  — Vulkan, no ECS / app knowledge
└───────────────────────┬─────────────────────┘
                        │ links
┌───────────────────────▼─────────────────────┐
│ Foundation         (vengine_foundation)      │  Src/Foundation/
│   Platform · Jobs · Core utils               │  — no rendering knowledge
└─────────────────────────────────────────────┘
```

| Library | Directory | Contents | Links (PUBLIC) |
|---|---|---|---|
| `vengine_foundation` | `Src/Foundation/` | `Core/` (utils), `Platform/` (window, asset paths), `Jobs/` (job system) | Vulkan, enkiTS, GLFW |
| `vengine_rendering` | `Src/Rendering/` | `RHI/`, `Resources/`, `FrameGraph/`, `Passes/`, `Core/`, `Shaders/` | `vengine_foundation` |
| `V-Engine` (exe) | `Src/Application/` | `Scene/`, `Input/`, `Editor/`, `Apps/` | `vengine_rendering`, `imgui` |

### Include convention

All internal includes are full paths from `Src/`:

```cpp
#include "Rendering/RHI/Device.hpp"
#include "Foundation/Jobs/JobSystem.hpp"
```

### Where things live on disk

- **Content** is at the repo root: `Models/`, `Scenes/`, `Environment/`.
- **Shaders** stay in the source tree: `Src/Rendering/Shaders/` (compiled by the
  `CompileShaders` target).
- **Frame-graph JSON** is app configuration, so it sits with the app:
  `Src/Application/Apps/FirstApp/FrameGraphs/forward.json`.
- Each app keeps its own `imgui.ini` next to its code.

## The seams

Two deliberate boundaries keep the layers from bleeding into each other.

### 1. `RenderScene` — Rendering does not know about entt

`Rendering` must not depend on `Application`, so render passes never take an
`entt::registry&`. Instead:

- `Rendering/Core/Render_Scene.hpp` defines a flat, engine-facing snapshot:
  ```cpp
  struct RenderObject { glm::mat4 modelMatrix, normalMatrix; Model* model; };
  struct RenderLight  { glm::vec3 position; glm::vec4 colour; float radius; };
  struct RenderScene  { std::vector<RenderObject> objects; std::vector<RenderLight> lights; };
  ```
- `Scene::buildRenderScene(RenderScene&)` (Application) walks the ECS **once per
  frame** and fills that struct in. This is the only place component layout is
  translated for the renderer.
- `FrameInfo` carries a `const RenderScene&`; passes iterate it and nothing else.

### 2. `AssetPath` — asset loading is independent of the working directory

`vengine::projectRoot()` discovers the repo root once by walking up from the
executable (and, as a fallback, the CWD) until it finds a directory containing
both `Src` and `Models`. `vengine::resolveAssetPath("Models/...")` turns a
root-relative path into an absolute one. This is why the engine runs the same
whether launched from `V-Engine/`, `V-Engine/build/`, or a debugger.

## Job system ownership

`enki::TaskScheduler` is owned by `vengine::JobSystem` (Foundation). Task
submission still goes through `jobSystem.scheduler()` directly with
`enki::TaskSet` / `enki::IPinnedTask`; the wrapper only centralizes
setup/teardown and the "extra threads" policy (see [Foundation](foundation.md)).

## Design notes

Longer-form engineering notes exist as design docs:

- **Frame Anatomy** — the current threading and resource flow, plus the
  frame-time regression introduced when geometry moved into frame-graph pass
  callbacks.
- **The Parallel Frame** — the target architecture: a frame as a DAG of tasks
  over data produced exactly once upstream (extract → cull → build/sort →
  record → assemble → submit).

See the [Rendering](rendering.md) doc for the current state and the top-level
README for the roadmap.
