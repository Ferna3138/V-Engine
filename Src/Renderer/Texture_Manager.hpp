#pragma once

// Std
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

#include "Device.hpp"
#include "Texture.hpp"


struct TextureHandle {
    uint32_t index = UINT32_MAX;
    bool valid() const { return index != UINT32_MAX; }
};

class TextureManager {
public:
    static constexpr uint32_t kMaxBindless = 4096; // adjust after querying limits

    TextureManager(Device& device, VkDescriptorSet bindlessSet, uint32_t bindlessBinding = 0);

    // Loads (or returns cached) texture. Paths are the deduplication key.
    TextureHandle load2D(const std::string& path);

    // 1x1 fallback textures (created on init)
    TextureHandle white() const { return whiteHandle; }
    TextureHandle black() const { return blackHandle; }

    // Call after a batch of loads (optional – safe to not call, we update immediately)
    void flush();

    // Cleanup not strictly required if using smart ptrs, but provided for symmetry
    void destroy();

    uint32_t count() const { return (uint32_t)entries.size(); }
    VkDescriptorSet getBindlessSet() const { return bindlessSet; }

private:
    struct Entry {
        std::string key;
        std::unique_ptr<Texture> texture; // owns VkImage/View/Sampler via your class
        uint32_t descriptorIndex = UINT32_MAX;
        uint32_t refCount = 0;
    };

    Device& device;
    VkDescriptorSet bindlessSet;
    uint32_t bindlessBinding;

    std::unordered_map<std::string, uint32_t> keyToIndex;
    std::vector<Entry> entries;

    // freelist if you later add unloading; for now we just append
    uint32_t allocateSlot();

    // Descriptors
    void writeCombinedImageSampler(uint32_t arrayIndex, VkImageView view, VkSampler sampler, VkImageLayout layout);

    // Fallbacks
    TextureHandle whiteHandle{};
    TextureHandle blackHandle{};
    void createFallbacks();
};