#pragma once

#include "Rendering/RHI/Buffer.hpp"
#include "Rendering/RHI/Descriptors.hpp"
#include "Rendering/RHI/Device.hpp"

#define GLM_FORCE_RADIANS
#include "glm/glm.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Matches the shader's manual UV-space wrap (simple_shader.frag) - chosen so
// no per-material VkSampler is needed even though a texture slot can be
// shared by materials that each want a different wrap behaviour.
enum class TextureWrapMode : int32_t { Repeat = 0, Mirror = 1, Stretch = 2 };

// Per-texture-slot UV transform: position/scale/rotation and wrap mode,
// applied to that slot's UV before sampling. Independent per channel (albedo/
// MR/normal can each tile differently), matching how it was asked for.
struct TextureTransform {
    glm::vec2 offset{0.f, 0.f};
    glm::vec2 scale{1.f, 1.f};       // tiling: higher = more repeats
    float rotationDegrees{0.f};      // around the UV centre (0.5, 0.5)
    TextureWrapMode wrapMode{TextureWrapMode::Repeat};

    // Inspector-only convenience (mirrors the mesh Transform's scale lock),
    // but cheap enough to just persist alongside the rest of the transform.
    bool uniformScaleLocked = false;
};

// CPU/editor-only bookkeeping for the Inspector's "move/scale/rotate every
// texture at once" controls - never sent to the GPU. Rather than being a
// separate transform composed at render time, each edit here computes a delta
// (offset/rotation: difference from the previous value; scale: ratio) and
// applies it directly into every channel's own TextureTransform, so this
// struct's value is always exactly "how much has been nudged in so far" -
// which means driving it back to identity (0 offset, 1 scale, 0 rotation)
// exactly undoes the net effect, regardless of how many edits got there. No
// wrap mode of its own: wrap is a discrete per-channel choice, so the
// Inspector's "apply to all" control for it just writes straight into each
// channel's transform instead of being tracked here.
struct GlobalTextureTransform {
    glm::vec2 offset{0.f, 0.f};
    glm::vec2 scale{1.f, 1.f};
    float rotationDegrees{0.f};
    bool uniformScaleLocked = false;
};

// CPU-side, freely editable by the Inspector - this is exactly what ImGui
// mutates directly; MaterialManager::update() pushes it to the GPU every
// frame, so no dirty-flag/notification plumbing is needed for an edit to
// take effect.
struct Material {
    std::string name;
    GlobalTextureTransform globalTransform;

    glm::vec3 baseColour{1.f};
    bool useAlbedoTexture = false;
    uint32_t albedoTexIndex = 1;   // TextureManager fallback slot 1 (white)
    std::string albedoTexturePath;
    TextureTransform albedoTransform;

    float metallic = 0.f;
    float roughness = 1.f;
    bool useMRTexture = false;
    uint32_t mrTexIndex = 0;       // TextureManager fallback slot 0 (MR-neutral)
    std::string mrTexturePath;
    TextureTransform mrTransform;

    bool useNormalTexture = false;
    uint32_t normalTexIndex = 2;   // TextureManager fallback slot 2 (flat normal)
    std::string normalTexturePath;
    TextureTransform normalTransform;
};

// Global, bindless-style growable resource (same shape as TextureManager):
// every Model registers its materials here once at load time and bakes back
// a stable index per vertex. Edits made through getMaterial() are live - the
// per-frame-in-flight GPU buffer is fully repacked from `materials` every
// frame, the same idiom First_App.cpp already uses for its global UBO.
class MaterialManager {
public:
    static constexpr uint32_t MAX_MATERIALS = 4096;

    explicit MaterialManager(Device& device);

    // Registers a new material and returns its stable global index. Safe to
    // call from a background model-import worker thread as well as the main
    // thread (guarded the same way TextureManager::addTexture is).
    uint32_t addMaterial(Material material);

    // Live handle into the CPU-side material - the Inspector mutates this
    // directly. Stable for the manager's lifetime: `materials` is reserved to
    // MAX_MATERIALS up front so it never reallocates.
    Material& getMaterial(uint32_t index);
    uint32_t getMaterialCount() const { return static_cast<uint32_t>(materials.size()); }

    // Repacks `materials` into this frame-in-flight's GPU buffer. Call once
    // per frame, after any Inspector edits for the frame have been applied.
    void update(int frameIndex);

    VkDescriptorSetLayout getLayout() const { return setLayout->getDescriptorSetLayout(); }
    VkDescriptorSet getDescriptorSet(int frameIndex) const { return descriptorSets[frameIndex]; }

private:
    Device& device;

    std::vector<Material> materials;
    std::mutex mutex;  // guards `materials` size/append bookkeeping, mirrors TextureManager

    std::vector<std::unique_ptr<Buffer>> buffers;  // one per frame in flight
    std::unique_ptr<DescriptorSetLayout> setLayout;
    std::unique_ptr<DescriptorPool> pool;
    std::vector<VkDescriptorSet> descriptorSets;
};
