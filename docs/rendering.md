# Rendering

The rendering layer (`vengine_rendering`) owns everything Vulkan: the RHI
wrappers, GPU resources, the frame graph, and the individual render passes.

## Module map

```
Rendering/
├── RHI/          Device, SwapChain, Pipeline, Buffer, Descriptors — thin Vulkan wrappers
├── Core/         Renderer, Camera, Frame_Info, Render_Scene       — per-frame plumbing
├── FrameGraph/   Frame_Graph                                      — JSON-driven pass graph
├── Passes/       Depth prepass, Simple (forward), Point light, Ray tracing (stub)
├── Resources/    Model, Texture, TextureManager, Async_Loader
└── Shaders/      GLSL sources + compiled .spv
```

## The frame, end to end

The render loop lives in `FirstApp::run()`
([`Src/Application/Apps/FirstApp/First_App.cpp`](../Src/Application/Apps/FirstApp/First_App.cpp)).
Per frame:

1. **Input & camera.** The `KeyboardMovementController` updates the camera
   entity's transform; `Camera::setView` / `setPerspectiveProjection` rebuild
   the matrices.
2. **Extract.** `Scene::buildRenderScene(renderScene)` walks the ECS into the
   flat `RenderScene` (objects + lights).
3. **`renderer.beginFrame()`** acquires a swapchain image and begins the primary
   command buffer. Returns `nullptr` if the swapchain needs recreating (resize).
4. **Update UBO.** `GlobalUbo` (projection, view, inverse view/proj, ambient,
   point lights, light count) is filled — `PointLightSystem::update` copies the
   lights in — and written to that frame's host-visible uniform buffer.
5. **`renderUI()`** builds the ImGui draw data (dockspace, menu bar, control
   panel, scene hierarchy).
6. **`frameGraph.execute(commandBuffer)`** records the graph: depth pre-pass →
   forward, into an offscreen `scene_colour` image, with image barriers
   auto-inserted from the graph edges.
7. **Composite.** The graph's present output (`scene_colour`, left in
   `TRANSFER_SRC_OPTIMAL`) is `vkCmdBlitImage`-ed into the acquired swapchain
   image (barriered to `TRANSFER_DST` then `COLOR_ATTACHMENT`).
8. **UI pass.** A swapchain render pass draws only ImGui, via a secondary
   command buffer.
9. **`renderer.endFrame()`** ends the primary buffer and submits + presents;
   handles resize / out-of-date.

There are **3 frames in flight** (`SwapChain::MAX_FRAMES_IN_FLIGHT`); UBO
buffers, descriptor sets and secondary command pools are all triple-buffered.

## RHI wrappers

| Class | Responsibility |
|---|---|
| **`Device`** | Instance, physical/logical device, queues (graphics, present, **transfer**), command pools (graphics + transfer), the pipeline cache (`createPipelineCache` / `savePipelineCache`), and buffer/image helpers. Requests RT + descriptor-indexing + buffer-device-address extensions. `graphicsQueueMutex` guards cross-thread graphics submits. |
| **`SwapChain`** | Swapchain images/views, the depth buffer, the swapchain render pass, framebuffers, and the per-frame sync objects (image-available / render-finished semaphores, in-flight fences). Supports recreation from an old swapchain. |
| **`Pipeline`** | A graphics pipeline from a vert/frag SPIR-V pair + a `PipelineConfigInfo`. `defaultPipelineConfigInfo` / `enableAlphaBlending` set up common state. `ComputePipeline` is the compute counterpart. Uses the device pipeline cache. |
| **`Buffer`** | A `VkBuffer` + `VkDeviceMemory` pair with `map` / `writeToBuffer` / `flush` / `descriptorInfo`, plus indexed variants for per-instance sub-allocations. |
| **`Descriptors`** | Builder-pattern `DescriptorSetLayout`, `DescriptorPool`, and `DescriptorWriter`. Layout builder supports per-binding `VkDescriptorBindingFlags` (used for the bindless texture array). |

### `Renderer`

`Renderer` (`Core/Renderer.hpp`) owns the swapchain and the command buffers:

- One **primary** command buffer per frame in flight.
- A grid of **secondary** command pools/buffers: `MAX_FRAMES_IN_FLIGHT ×
  kNumRecorders` where `kNumRecorders = 3` (mesh, point light, ImGui). Each
  recorder gets its own pool so it can be recorded independently.
  `getSecondaryCommandBuffer(recorderIndex)` indexes into the current frame's
  row.
- `beginFrame` / `endFrame` / `beginSwapChainRenderPass` /
  `endSwapChainRenderPass` bracket the frame and hide swapchain recreation.

## Frame graph

`FrameGraph` (`FrameGraph/Frame_Graph.hpp`) turns a small JSON file into a
sorted list of render passes with automatic barriers.

### Lifecycle

```cpp
frameGraph.parse("…/FrameGraphs/forward.json");   // JSON -> nodes + resources
frameGraph.compile();                             // computeEdges() + topologicalSort()
frameGraph.createResources(device);               // VkImage/View/Memory per owned attachment
frameGraph.createRenderPasses(device);            // VkRenderPass + VkFramebuffer per node
frameGraph.setNodeUsesSecondaryCommandBuffers("forward", true);
frameGraph.setPresentOutput("scene_colour");      // resource that leaves the graph
// per frame:
frameGraph.registerRenderPass("forward", [&](VkCommandBuffer cb){ … });  // once, actually
frameGraph.execute(commandBuffer);
```

### The JSON

[`forward.json`](../Src/Application/Apps/FirstApp/FrameGraphs/forward.json):

```json
[
  { "name": "depth_prepass",
    "outputs": [ { "type": "attachment", "name": "depth",
                   "format": "VK_FORMAT_D32_SFLOAT", "resolution": [1200, 800],
                   "op": "VK_ATTACHMENT_LOAD_OP_CLEAR" } ] },
  { "name": "forward",
    "inputs":  [ { "type": "attachment", "name": "depth" } ],
    "outputs": [ { "type": "attachment", "name": "scene_colour",
                   "format": "VK_FORMAT_B8G8R8A8_SRGB", "resolution": [1200, 800],
                   "op": "VK_ATTACHMENT_LOAD_OP_CLEAR" } ] }
]
```

Each node lists `inputs` and `outputs`. A resource `type` is one of
`attachment`, `texture`, `buffer`, `reference`. An output names its `format`
(a small allow-list of `VkFormat` strings), `resolution`, and load `op`. An
input references a resource by name — `computeEdges()` links it to the node that
produced it and adds a graph edge.

### Compilation & execution

- **`computeEdges()`** resolves each input name to a producing output, records
  the producer node, and appends the consumer to the producer's `edges`.
- **`topologicalSort()`** iterative DFS → `sortedNodeOrder`.
- **`createResources()`** allocates one `VkImage` + view + memory per *owned*
  attachment. Colour attachments get `SAMPLED` + `TRANSFER_SRC` usage (any later
  node may sample them; the present output gets blitted). Input entries just
  alias their producer's handles.
- **`createRenderPasses()`** builds a single-subpass `VkRenderPass` +
  `VkFramebuffer` per node from its attachment set (inputs = `LOAD`, outputs =
  the JSON `op`). Depth vs colour is inferred from the format.
- **`execute()`** resets every resource's tracked layout to `UNDEFINED`, then
  walks `sortedNodeOrder`:
  - For each incoming edge, `insertInputBarrier()` transitions the producer's
    image from its tracked layout to what the consumer needs (sampled →
    `SHADER_READ_ONLY`, attachment → colour/depth-attachment-optimal).
  - Begins the render pass (clear values by format), sets viewport/scissor for
    inline nodes, invokes the registered callback, ends the pass.
  - Records the post-pass layout so downstream edges barrier from the right
    source.
  - Finally transitions `presentOutputResource` to `TRANSFER_SRC_OPTIMAL` for
    the blit into the swapchain.

### Registered callbacks

`registerRenderPass(name, fn)` attaches a `std::function<void(VkCommandBuffer)>`
to a node. `execute()` calls it inside `vkCmdBeginRenderPass` /
`vkCmdEndRenderPass`. In `FirstApp`:

- **`depth_prepass`** → `DepthPrepassSystem::render` on the primary buffer.
- **`forward`** → spawns two enkiTS tasks that record the mesh and point-light
  secondary command buffers in parallel, waits on both, then
  `vkCmdExecuteCommands`. (This is the one place parallel recording still
  happens — see the note at the end.)

## Render passes

| Pass | Shaders | What it does |
|---|---|---|
| **`DepthPrepassSystem`** | `depth_only.vert/.frag` | Draws every `RenderObject` with position only into a depth-only render pass (no colour attachment). Lets the forward pass run `LESS_OR_EQUAL` with depth writes off. |
| **`SimpleRenderSystem`** | `simple_shader.vert/.frag` | The forward pass. Binds the global set (0) + bindless texture set (1), pushes `{modelMatrix, normalMatrix}` per object, and draws each model. `depthCompareOp = LESS_OR_EQUAL`, `depthWriteEnable = FALSE`. |
| **`PointLightSystem`** | `point_light.vert/.frag` | `update()` copies lights into the UBO. `render()` sorts lights back-to-front and draws a 6-vertex alpha-blended billboard per light (no vertex buffer; positions from `gl_VertexIndex`). Toggled by the **View ▸ Visualise Point Lights** menu item. |
| **`RayTracing`** | — | Header stub; ray-tracing device extensions are already enabled on `Device`. Roadmap item. |

### Push constants & descriptor sets

- **Set 0 — `GlobalUbo`** (`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`, all graphics
  stages): projection, view, inverse view, inverse proj, ambient light, up to
  `MAX_LIGHTS` (10) point lights, light count.
- **Set 1 — bindless textures** (`sampler2D textures[]`, 1024-slot
  `COMBINED_IMAGE_SAMPLER` array) — see [Resources](resources.md).
- **Push constant** (`simple_shader`, `depth_only`): `mat4 modelMatrix`,
  `mat4 normalMatrix`.
- **Push constant** (`point_light`): `vec4 position`, `vec4 colour`,
  `float radius`.

## Shaders

`Src/Rendering/Shaders/`, GLSL 450, compiled to `.spv` by `glslc`.

| File | Role |
|---|---|
| `shader_common.glsl` | `PointLight` struct + `GlobalUbo` block (set 0, binding 0) |
| `light_common.glsl` | Cook–Torrance helpers: `DistributionGGX`, `GeometrySmith`, `FresnelSchlick` |
| `simple_shader.vert/.frag` | Forward shading. Vertex passes world pos/normal/tangent + flat texture indices; fragment does normal-mapping via a TBN basis and a Cook–Torrance GGX BRDF loop over the point lights, sampling `albedo` / `specular` / `normal` from the bindless array (`nonuniformEXT`). Material roughness/metallic are currently constants. |
| `depth_only.vert/.frag` | Position-only clip transform, empty fragment |
| `point_light.vert/.frag` | Camera-facing billboard quad from a 6-entry offset table |

## Camera

`Camera` (`Core/Camera.hpp`) holds projection + view (+ their inverses).
`setView(position, quat)` and `setPerspectiveProjection(fovy, aspect, near,
far)` are the ones used each frame. `CamParameters` carries a fuller physically
based description (sensor size, focal length, aperture, focus distance, shutter,
exposure, white balance) and a `CameraModel { Pinhole, Physical }` enum — the
scaffolding for a physically based camera; today only `fov` drives the
projection.

## Parallel recording — current state

Geometry recording used to fan out across worker threads. When the frame graph
took over the render path, most geometry moved into the pass callbacks and is
now recorded largely inline on the main thread; only the `forward` node still
splits mesh vs light recording across two enkiTS tasks. Reclaiming broad
parallel recording (chunked secondary command buffers driven by the graph) is
the highest-leverage item on the renderer roadmap — see the top-level README and
the "The Parallel Frame" design note.
