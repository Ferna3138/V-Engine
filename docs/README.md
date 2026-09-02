# V-Engine Documentation

V-Engine is a Vulkan renderer / game-engine sandbox written in C++20. It is a
personal learning and research project focused on building a renderer that
**scales on three axes at once**: CPU cores, render passes, and draw calls.

The engine currently renders the Sponza scene with a JSON-driven frame graph
(depth pre-pass → forward → blit), a bindless texture array, an asynchronous
texture streaming pipeline on a dedicated transfer queue, an entt-based scene
with save/load, and a small ImGui editor.

## Documentation index

| Document | What it covers |
|---|---|
| [Architecture](architecture.md) | The three-layer design, module map, and the seams that keep the layers decoupled |
| [Building & Running](building.md) | Dependencies, CMake targets, shader compilation, how to launch |
| [Rendering](rendering.md) | The frame walkthrough, frame graph, render passes, shaders, camera |
| [Resources](resources.md) | Models, the bindless texture manager, and the async loader |
| [Scene & Editor](scene.md) | ECS components, the render-scene seam, JSON serialization, input, ImGui |
| [Foundation](foundation.md) | Window, job system, asset-path resolution, small utilities |

## Roadmap

The project roadmap lives in the [top-level README](../README.md#roadmap) and in
the engineering notes; it is not duplicated here.

## Quick orientation

```
Src/
├── Foundation/    vengine_foundation  — platform, jobs, utilities (no rendering knowledge)
├── Rendering/     vengine_rendering   — Vulkan RHI, resources, frame graph, passes
└── Application/   V-Engine (exe)      — scene/ECS, input, editor, the app entry point
```

Dependency direction is **Foundation ← Rendering ← Application**, enforced by the
linker (each layer is its own static library).
