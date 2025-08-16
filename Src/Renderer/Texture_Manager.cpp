#include "Texture_Manager.hpp"
#include <cassert>

TextureManager::TextureManager(Device& device_, VkDescriptorSet bindlessSet_, uint32_t bindlessBinding_)
: device(device_), bindlessSet(bindlessSet_), bindlessBinding(bindlessBinding_) {
    entries.reserve(128);
    createFallbacks();
}

uint32_t TextureManager::allocateSlot() {
    // simple append – no reuse
    uint32_t idx = (uint32_t)entries.size();
    if (idx >= kMaxBindless) {
        throw std::runtime_error("TextureManager: exceeded kMaxBindless descriptor slots");
    }
    return idx;
}

void TextureManager::writeCombinedImageSampler(uint32_t arrayIndex, VkImageView view, VkSampler sampler, VkImageLayout layout) {
    VkDescriptorImageInfo img{};
    img.imageView = view;
    img.sampler   = sampler;
    img.imageLayout = layout; // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet          = bindlessSet;
    write.dstBinding      = bindlessBinding;
    write.dstArrayElement = arrayIndex;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo      = &img;

    vkUpdateDescriptorSets(device.device(), 1, &write, 0, nullptr);
}

TextureHandle TextureManager::load2D(const std::string& path) {
    if (auto it = keyToIndex.find(path); it != keyToIndex.end()) {
        entries[it->second].refCount++;
        return TextureHandle{ it->second };
    }

    // Create texture (your class already does mips + transitions)
    auto tex = std::make_unique<Texture>(device, path);
    // If you plan to support non-sRGB, add a path or flag in Texture to choose format

    // Allocate descriptor slot and write
    uint32_t slot = allocateSlot();

    Entry e{};
    e.key            = path;
    e.texture        = std::move(tex);
    e.descriptorIndex= slot;
    e.refCount       = 1;

    writeCombinedImageSampler(
        slot,
        e.texture->getImageView(),  // expose getters in Texture.hpp if you haven't
        e.texture->getSampler(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    keyToIndex[path] = slot;
    entries.emplace_back(std::move(e));
    return TextureHandle{ slot };
}

void TextureManager::flush() {
    // no-op for immediate updates
}

void TextureManager::destroy() {
    // unique_ptr<Texture> auto-cleans in Entry dtor
    entries.clear();
    keyToIndex.clear();
}

void TextureManager::createFallbacks() {
    // Create 1x1 white and black using your second Texture ctor
    {
        auto t = std::make_unique<Texture>(device, 1, 1, VK_FORMAT_R8G8B8A8_SRGB,
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        // You could upload a single white pixel with a tiny staging write (optional: Texture ctor can accept raw data)
        // For simplicity we’ll just rely on zero-initialized -> black; for true white, write a pixel.
        uint32_t slot = allocateSlot();
        writeCombinedImageSampler(slot, t->getImageView(), t->getSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        Entry e{};
        e.key = "__fallback_white__";
        e.texture = std::move(t);
        e.descriptorIndex = slot;
        e.refCount = UINT32_MAX; // never free
        whiteHandle = TextureHandle{slot};
        entries.emplace_back(std::move(e));
    }
    {
        auto t = std::make_unique<Texture>(device, 1, 1, VK_FORMAT_R8G8B8A8_SRGB,
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        uint32_t slot = allocateSlot();
        writeCombinedImageSampler(slot, t->getImageView(), t->getSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        Entry e{};
        e.key = "__fallback_black__";
        e.texture = std::move(t);
        e.descriptorIndex = slot;
        e.refCount = UINT32_MAX;
        blackHandle = TextureHandle{slot};
        entries.emplace_back(std::move(e));
    }
}
