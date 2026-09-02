# Building & Running

## Prerequisites

| Requirement | Notes |
|---|---|
| **Vulkan SDK** | `find_package(Vulkan REQUIRED)` — the loader plus validation layers (used in debug builds). The engine also requests ray-tracing, acceleration-structure, buffer-device-address and descriptor-indexing device extensions. |
| **CMake ≥ 3.28** | |
| **A C++20 compiler** | Developed with Visual Studio 2022 on Windows; Linux/macOS paths exist in the build files. |
| **Git submodules** | ImGui and GLFW are submodules — clone with `--recursive` or run `git submodule update --init`. |

### Vendored dependencies

Living under `Dependencies/`:

| Dependency | How it's consumed |
|---|---|
| **GLFW** | Prebuilt `.lib` on Windows (`Dependencies/lib-vc2022/`); built from source elsewhere. Submodule. |
| **Dear ImGui** | Compiled into a small `imgui` static lib (docking branch, `IMGUI_ENABLE_DOCKING`). Submodule. Application layer only. |
| **enkiTS** | `add_subdirectory(Dependencies/enkiTS)` — the task scheduler. |
| **EnTT** | Header-only ECS; included only by the `V-Engine` executable target. |
| **tinyobjloader / tinygltf (v3) / stb_image** | Header-only model & image loaders, used by `vengine_rendering`. |
| **glm** | Math. |
| **glslc** | Shader compiler binary at `Dependencies/shader_compile/`. |

## Build

### Unix one-liner

```sh
./unix_build.sh      # configures in build/, runs make, compiles shaders, launches
```

### Manual (any platform)

```sh
cmake -S . -B build
cmake --build build
```

CMake targets:

| Target | Purpose |
|---|---|
| `vengine_foundation`, `vengine_rendering` | The engine static libraries |
| `V-Engine` | The executable (depends on `CompileShaders`) |
| `CompileShaders` | Runs `Src/Rendering/Shaders/compile.{bat,sh}` over every `.vert/.frag/.rgen/.rchit/.rmiss/.comp` with `glslc`, emitting `*.spv` next to the source |

On Windows the CMake `source_group(TREE ...)` call mirrors the on-disk folders
into Visual Studio solution filters.

## Shaders

Shaders are GLSL under `Src/Rendering/Shaders/`, compiled to SPIR-V by the
`CompileShaders` target (or by running `compile.bat` / `compile.sh` directly).
Compiled `.spv` files are loaded at runtime by path, e.g.
`Src/Rendering/Shaders/simple_shader.vert.spv`, resolved against the project
root.

## Running

The executable can be launched from anywhere — asset paths are resolved against
the auto-detected project root (see [Architecture § AssetPath](architecture.md)).

At startup `FirstApp`:

1. Initializes the job system and the pinned async-loader thread.
2. Loads `Scenes/sponza.json` (camera + Sponza model + 6 point lights).
3. Parses and compiles `Src/Application/Apps/FirstApp/FrameGraphs/forward.json`.
4. Opens a 1200×800 window and enters the render loop.

### Controls

| Input | Action |
|---|---|
| `W` `A` `S` `D` | Move in the XZ plane |
| `E` / `Q` | Move up / down |
| Arrow keys | Look |
| Right-mouse drag | Look (mouse) |
| Scroll | (bound; zoom/speed) |
| `Ctrl+S` | Save scene |
| Menu bar | File (Save / Reload / Exit), Edit (stub), View (toggle point-light billboards) |

### Generated files

- `pipeline_cache.bin` — Vulkan pipeline cache, written at the repo root on exit
  and reloaded on startup.
- `Src/Application/Apps/FirstApp/imgui.ini` — ImGui layout (git-tracked via a
  `.gitignore` negation).
