# V-Engine

A Vulkan renderer and game-engine sandbox in C++20 — a personal project for
learning and researching a renderer that **scales on cores, render passes, and
draw calls** at the same time.

![status](https://img.shields.io/badge/status-active%20development-blue)

## What's in it today

- **Three-layer architecture** — Foundation ← Rendering ← Application, each a
  separate static library so the dependency direction is linker-enforced.
- **JSON-driven frame graph** — `depth_prepass → forward → blit`, with render
  passes, framebuffers and image barriers derived automatically from the graph.
- **Forward PBR shading** — Cook–Torrance GGX with normal mapping, over point
  lights.
- **Bindless textures** — a single 1024-slot sampler array; a model is one draw
  call regardless of texture count.
- **Async texture streaming** — decode on worker threads, upload on a dedicated
  transfer queue, queue-family ownership transfer to graphics, no CPU stalls.
- **entt scene** with JSON save/load and a small ImGui editor (docking,
  hierarchy panel, transform inspector).
- **enkiTS job system**, triple-buffered frames, Vulkan pipeline cache.
- Ray-tracing device extensions enabled; RT pipeline is next up.

## Build

```sh
git clone --recursive <repo-url>
cd V-Engine
cmake -S . -B build
cmake --build build
```

Needs the Vulkan SDK and CMake ≥ 3.28. See
[docs/building.md](docs/building.md) for dependencies, shader compilation, and
controls.

## Documentation

Full docs live in [`docs/`](docs/README.md):

| Doc | |
|---|---|
| [Architecture](docs/architecture.md) | Layers, module map, the decoupling seams |
| [Building & Running](docs/building.md) | Dependencies, targets, controls |
| [Rendering](docs/rendering.md) | Frame walkthrough, frame graph, passes, shaders |
| [Resources](docs/resources.md) | Models, bindless textures, the async loader |
| [Scene & Editor](docs/scene.md) | ECS, components, JSON scenes, input, ImGui |
| [Foundation](docs/foundation.md) | Window, job system, asset paths |

## Roadmap

- Async Compute

- GPU Driven Rendering

- View Culling

- Volumetric Fog

- Light source optimisation

- Ray Tracing Pipeline (some things were already implemented)

- TLAS & BLAS

- Shader Binding Table

- Ray Traced Shadow Tracing

- DDGI (?)

- Ray Traced Reflections
