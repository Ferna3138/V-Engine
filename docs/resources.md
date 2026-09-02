# Resources

`Src/Rendering/Resources/` — models, textures, the bindless texture manager, and
the asynchronous upload pipeline.

## Models

`Model` (`Model.hpp`) is an indexed mesh: one vertex buffer, one index buffer,
both `DEVICE_LOCAL`, uploaded through a staging buffer.

### Vertex layout

```cpp
struct Vertex {
    glm::vec3 position;
    glm::vec3 colour;
    glm::vec3 normal;
    glm::vec2 uv;
    int   textureIndex  = -1;   // bindless slot for albedo
    int   specularIndex = -1;   // bindless slot for specular
    int   normalIndex   = -1;   // bindless slot for normal map
    glm::vec3 tangent   = 0;
};
```

The texture indices are stored **per vertex** — a whole model is one draw call
regardless of how many textures its materials use, because the fragment shader
indexes the bindless array with the interpolated (flat) slot number.

`MaterialObj` carries the classic OBJ material fields (ambient/diffuse/specular,
shininess, IOR, dissolve, illum) plus the three texture IDs.

### Loading

```cpp
auto model = Model::createModelFromFile(device, textureManager, "Models/GLTF/Sponza/glTF/Sponza.gltf");
```

- Path is resolved against the project root. A **missing** file logs and returns
  `nullptr` (not fatal); a **malformed** file throws.
- `Builder::loadModel` dispatches on extension to `loadObj` (tinyobjloader) or
  `loadGltf` (tinygltf v3). Each material texture referenced by the file is
  registered with the `TextureManager`, and the returned slot is baked into the
  affected vertices.

`bind()` / `draw()` record `vkCmdBindVertexBuffers` + `vkCmdBindIndexBuffer` and
`vkCmdDrawIndexed`.

## Bindless textures — `TextureManager`

One descriptor set, one binding: a **1024-slot** `COMBINED_IMAGE_SAMPLER` array
(`textures[]` in `simple_shader.frag`).

```cpp
setLayout = DescriptorSetLayout::Builder(device)
    .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT, MAX_BINDLESS_TEXTURES,
                  VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT)
    .build();
```

### Reserved slots

| Slot | Contents |
|---|---|
| 0 | 1×1 black |
| 1 | 1×1 white (also the streaming placeholder) |
| 2 | 1×1 flat normal `(128,128,255)` |
| 3… | Streamed textures; `nextIndex` starts at 3 |

`pathToIndex` deduplicates — asking for the same file twice returns the same
slot.

### `addTexture(path, format)` (main thread)

1. Reserve a slot, record `pathToIndex[path] = slot`.
2. Under `mutex`: write the **white placeholder** into the slot and register
   `pendingTextures[slot] = tex`.
3. Spawn a `TextureDecodeTask` (an `enki::ITaskSet`) on the scheduler. Decode
   tasks are kept alive in `decodeTasks` until shutdown.

### `update()` (I/O thread)

Called in the async-loader thread's loop:

1. `loader.update()` advances the transfer/acquire state machine.
2. For every slot from `loader.pollFinishedUploads()`: pop it from
   `pendingTextures`, call `texture->onUploadFinished()` (free pixels, set
   layout), and — under `mutex` — `writeTextureToSlot()` so the real texture
   replaces the placeholder.

`mutex` guards `pendingTextures` and every host write to the shared bindless
descriptor set (`addTexture` runs on the main thread, `update` on the I/O
thread).

## Async upload pipeline — `AsyncLoader`

Streams texture pixels to the GPU on the **dedicated transfer queue**, then hands
the image to the graphics queue via a queue-family ownership transfer. Almost
everything here runs on **one pinned I/O thread**; only `addUploadRequest()`
(any thread) and `wakeForShutdown()` are external entry points.

### Per-texture state machine (one stage deep each)

```
queued ──(transfer submit; signals transferCompleteSemaphore)──► uploading
uploading ──(transferFence)──► ──(acquire submit; waits semaphore)──► finalizing
finalizing ──(finalizeFence, polled non-blocking)──► finished  (id → pollFinishedUploads)
```

- **`submitTransfer`** — copies from the staging buffer on the transfer queue,
  signals `transferFence` (staging reuse) and the binary
  `transferCompleteSemaphore` (the cross-queue dependency).
- **`submitAcquire`** — on the graphics queue under `Device::graphicsQueueMutex`,
  waits `transferCompleteSemaphore`, signals `finalizeFence`. **No CPU wait.**
- **`update()`** polls `finalizeFence` non-blocking, then pushes the slot id to
  `finishedUploads`.

### Threading & buffers

- The I/O thread is `AsyncLoadTask` (an `enki::IPinnedTask`, pinned to
  `GetNumTaskThreads() - 1`). It loops on `loader.waitForWork()` — a condition
  variable when fully idle, a bounded fence wait while a stage is in flight —
  then calls `textureManager.update()`.
- A single **64 MB persistently-mapped** staging buffer; **one transfer + one
  acquire in flight at a time**.
- Dedicated graphics-family command pool/fence for the acquire, so the I/O
  thread never touches the `Device` shared pool.

### Decode itself is a task

`Texture::asyncLoad(id)` runs on a worker thread: `stbi_load` +
`vkCreateImage` / view / sampler + `loader.addUploadRequest`. So file I/O and
decode are parallel across the pool, and the pinned thread only serializes the
GPU transfer/acquire submissions.

### Known deferred upgrade

The transfer queue is the throughput throttle during a burst of texture loads.
The planned fix is a staging ring with batched copies rather than one-in-flight.

## Two `Texture` constructors

| Constructor | Use |
|---|---|
| `Texture(device, loader, filepath, format)` | Streamed: created empty/`ready=false`, filled by `asyncLoad` on a worker |
| `Texture(device, w, h, data, format, usage)` | Immediate: small in-memory images (the black/white/normal fallbacks) |
