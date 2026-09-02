# Foundation

`Src/Foundation/` (`vengine_foundation`) — the base layer. No rendering-domain
knowledge; links Vulkan, enkiTS and GLFW publicly so the layers above inherit
them.

```
Foundation/
├── Core/       Utils.hpp        — small header-only helpers
├── Platform/   Window, AssetPath — OS window + project-root resolution
└── Jobs/       JobSystem        — owns the enkiTS scheduler
```

## `Platform/Window`

A thin GLFW wrapper (`GLFW_INCLUDE_VULKAN`).

- Creates the window, tracks size and a `framebufferResized` flag via
  `framebufferResizeCallback`.
- `getExtent()`, `shouldClose()`, `wasWindowResized()` /
  `resetWindowResizedFlag()`, `getGLFWwindow()`.
- `createWindowSurface(instance, *surface)` — hands the `VkSurfaceKHR` to
  `Device`.

Non-copyable.

## `Platform/AssetPath`

Makes asset loading independent of the working directory.

```cpp
const std::filesystem::path& vengine::projectRoot();
std::string vengine::resolveAssetPath(const std::string& path);
```

- `projectRoot()` — discovered **once** at first use by walking up from the
  executable location (fallback: the CWD) until it finds a directory containing
  both `Src` and `Models`. So the engine runs identically from `V-Engine/`,
  `V-Engine/build/`, `V-Engine/build/Debug/`, or a debugger.
- `resolveAssetPath()` — turns a root-relative path into an absolute one;
  absolute inputs pass through unchanged. Does **not** check existence.

Used by the model loader, texture loader, frame-graph JSON parser, scene
serializer, and the ImGui ini path.

## `Jobs/JobSystem`

Owns the engine-wide `enki::TaskScheduler` — construction, configuration,
shutdown. **Task submission is not wrapped**: callers use
`jobSystem.scheduler()` directly with `enki::TaskSet` / `enki::IPinnedTask`.

```cpp
void initialize(uint32_t extraThreads = 0);
void shutdown();                         // WaitforAllAndShutdown(); idempotent, also in dtor
enki::TaskScheduler& scheduler();
uint32_t threadCount();
```

### The `extraThreads` policy

enkiTS defaults to `GetNumHardwareThreads()` worker threads (thread 0 being the
calling thread). Any `extraThreads` requested sit **on top** of that, so a
permanently-blocked pinned task — like the async texture loader's I/O thread,
pinned to `GetNumTaskThreads() - 1` — can be parked without shrinking the usable
pool.

### Consumers

| User | Task type | Thread |
|---|---|---|
| `AsyncLoadTask` (texture streaming I/O) | `enki::IPinnedTask` | pinned to the last worker, runs for the app lifetime, blocks in `loader.waitForWork()` |
| `TextureManager::TextureDecodeTask` | `enki::ITaskSet` | any worker — file decode + GPU resource creation |
| `forward` frame-graph node | two `enki::TaskSet`s | mesh + point-light secondary command buffers, recorded in parallel |

## `Core/Utils.hpp`

Just `hashCombine(seed, values...)` — a variadic fold used for hashing vertices
during model de-duplication.

## Notes

- An unused vendored `Math/Quaternion.hpp` was removed — the engine uses
  `glm::quat` throughout.
- The old `Src/Dependencies/` (a committed GLFW build tree) was deleted and
  gitignored; real dependencies live at the repo-root `Dependencies/`.
