#include "Rendering/Resources/MaterialManager.hpp"
#include "Rendering/RHI/Swap_Chain.hpp"

#include <cassert>
#include <cstring>

namespace {

// std430 mirror of `Material`, uploaded to the GPU verbatim - field order here
// must match the `Material` struct declared in simple_shader.frag exactly.
// Layout: vec4 (16-aligned) then 8 tightly-packed 4-byte scalars (48 bytes),
// then 3 flattened TextureTransforms (vec2+vec2+float+int = 24 bytes each,
// all 8-byte-aligned offsets so no internal padding) -> 120 bytes, then 8
// bytes of trailing pad so the total (128) is a multiple of 16 - required for
// the array stride, since a nested struct member would force 16-byte
// alignment in GLSL that a flat layout like this sidesteps entirely.
//
// Material::globalTransform is NOT part of this layout - it's a CPU/editor-
// only bookkeeping value the Inspector uses to compute deltas for its "move
// everything at once" controls, which it applies directly into each channel's
// own TextureTransform. The GPU never needs to know it existed.
struct MaterialGPU {
    glm::vec4 baseColour{1.f};
    int32_t albedoTexIndex = -1;
    int32_t mrTexIndex = -1;
    int32_t normalTexIndex = -1;
    int32_t useAlbedoTexture = 0;
    float metallic = 0.f;
    float roughness = 1.f;
    int32_t useMRTexture = 0;
    int32_t useNormalTexture = 0;

    glm::vec2 albedoOffset{0.f};
    glm::vec2 albedoScale{1.f};
    float albedoRotation = 0.f;
    int32_t albedoWrapMode = 0;

    glm::vec2 mrOffset{0.f};
    glm::vec2 mrScale{1.f};
    float mrRotation = 0.f;
    int32_t mrWrapMode = 0;

    glm::vec2 normalOffset{0.f};
    glm::vec2 normalScale{1.f};
    float normalRotation = 0.f;
    int32_t normalWrapMode = 0;

    float _pad0 = 0.f;
    float _pad1 = 0.f;
};
static_assert(sizeof(MaterialGPU) == 128, "MaterialGPU must match the std430 layout in simple_shader.frag");

void packTransform(const TextureTransform& src, glm::vec2& offset, glm::vec2& scale, float& rotation, int32_t& wrapMode) {
    offset = src.offset;
    scale = src.scale;
    rotation = src.rotationDegrees;
    wrapMode = static_cast<int32_t>(src.wrapMode);
}

}  // namespace

MaterialManager::MaterialManager(Device& _device) : device{_device} {
    materials.reserve(MAX_MATERIALS);

    setLayout = DescriptorSetLayout::Builder(device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build();

    pool = DescriptorPool::Builder(device)
        .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT)
        .build();

    buffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    descriptorSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
        buffers[i] = std::make_unique<Buffer>(
            device,
            sizeof(MaterialGPU),
            MAX_MATERIALS,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        buffers[i]->map();

        auto bufferInfo = buffers[i]->descriptorInfo();
        DescriptorWriter(*setLayout, *pool)
            .writeBuffer(0, &bufferInfo)
            .build(descriptorSets[i]);
    }
}

uint32_t MaterialManager::addMaterial(Material material) {
    std::lock_guard<std::mutex> lock(mutex);
    assert(materials.size() < MAX_MATERIALS && "Maximum number of materials reached.");
    uint32_t index = static_cast<uint32_t>(materials.size());
    materials.push_back(std::move(material));
    return index;
}

Material& MaterialManager::getMaterial(uint32_t index) {
    return materials[index];
}

void MaterialManager::update(int frameIndex) {
    // `materials` doesn't reallocate (reserved up front) and only grows on the
    // main/import threads between frames, so reading its current size here
    // without holding `mutex` for the whole packing loop is safe in practice;
    // still take it for the size read to stay honest about the shared state.
    size_t count;
    {
        std::lock_guard<std::mutex> lock(mutex);
        count = materials.size();
    }

    std::vector<MaterialGPU> packed(count);
    for (size_t i = 0; i < count; i++) {
        const Material& src = materials[i];
        MaterialGPU& dst = packed[i];
        dst.baseColour = glm::vec4(src.baseColour, 1.f);
        dst.albedoTexIndex = static_cast<int32_t>(src.albedoTexIndex);
        dst.mrTexIndex = static_cast<int32_t>(src.mrTexIndex);
        dst.normalTexIndex = static_cast<int32_t>(src.normalTexIndex);
        dst.useAlbedoTexture = src.useAlbedoTexture ? 1 : 0;
        dst.metallic = src.metallic;
        dst.roughness = src.roughness;
        dst.useMRTexture = src.useMRTexture ? 1 : 0;
        dst.useNormalTexture = src.useNormalTexture ? 1 : 0;

        packTransform(src.albedoTransform, dst.albedoOffset, dst.albedoScale, dst.albedoRotation, dst.albedoWrapMode);
        packTransform(src.mrTransform, dst.mrOffset, dst.mrScale, dst.mrRotation, dst.mrWrapMode);
        packTransform(src.normalTransform, dst.normalOffset, dst.normalScale, dst.normalRotation, dst.normalWrapMode);
    }

    if (count == 0) return;
    Buffer& buffer = *buffers[frameIndex];
    buffer.writeToBuffer(packed.data(), sizeof(MaterialGPU) * count);
    // Flush the whole allocation rather than just the written prefix: a
    // partial-size flush must be a multiple of nonCoherentAtomSize (or exactly
    // reach the end of the allocation), which sizeof(MaterialGPU)*count isn't
    // guaranteed to satisfy. The buffer is tiny (a few hundred KB), so flushing
    // it whole every frame is cheap.
    buffer.flush();
}
