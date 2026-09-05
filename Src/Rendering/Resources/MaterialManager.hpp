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

// CPU-side, freely editable by the Inspector - this is exactly what ImGui
// mutates directly; MaterialManager::update() pushes it to the GPU every
// frame, so no dirty-flag/notification plumbing is needed for an edit to
// take effect.
struct Material {
    std::string name;

    glm::vec3 baseColour{1.f};
    bool useAlbedoTexture = false;
    uint32_t albedoTexIndex = 1;   // TextureManager fallback slot 1 (white)
    std::string albedoTexturePath;

    float metallic = 0.f;
    float roughness = 1.f;
    bool useMRTexture = false;
    uint32_t mrTexIndex = 0;       // TextureManager fallback slot 0 (MR-neutral)
    std::string mrTexturePath;

    bool useNormalTexture = false;
    uint32_t normalTexIndex = 2;   // TextureManager fallback slot 2 (flat normal)
    std::string normalTexturePath;
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
