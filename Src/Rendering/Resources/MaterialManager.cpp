#include "Rendering/Resources/MaterialManager.hpp"
#include "Rendering/RHI/Swap_Chain.hpp"

#include <cassert>
#include <cstring>

namespace {

// std430 mirror of `Material`, uploaded to the GPU verbatim - field order here
// must match the `Material` struct declared in simple_shader.frag exactly.
// Layout (zero padding): vec4 (16-aligned) then 8 tightly-packed 4-byte
// scalars (32 bytes, itself 16-aligned) -> 48 bytes total, already a multiple
// of 16 so the array stride needs no trailing pad either.
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
};
static_assert(sizeof(MaterialGPU) == 48, "MaterialGPU must match the std430 layout in simple_shader.frag");

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
