#include "Renderer/TextureManager.hpp"

TextureManager::TextureManager(Device& _device) : device{_device} {
    setLayout = DescriptorSetLayout::Builder(device)
        .addBinding(
            0,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            MAX_BINDLESS_TEXTURES,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT)
            .build();

    pool = DescriptorPool::Builder(device)
            .setMaxSets(1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_BINDLESS_TEXTURES)
            .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT)
            .build();
    
    pool->allocateDescriptor(setLayout->getDescriptorSetLayout(), descriptorSet, MAX_BINDLESS_TEXTURES);

    // Fallback texture
    textures.push_back(std::make_unique<Texture>(device, 1, 1, blackPixel, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT));
    writeTextureToSlot(*textures.back(), 0);
    textures.push_back(std::make_unique<Texture>(device, 1, 1, whitePixel, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT));
    writeTextureToSlot(*textures.back(), 1);
    textures.push_back(std::make_unique<Texture>(device, 1, 1, flatNormal, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT));
    writeTextureToSlot(*textures.back(), 2);
}

uint32_t TextureManager::addTexture(const std::string& filepath, VkFormat format) {
    auto it = pathToIndex.find(filepath);
    if (it != pathToIndex.end()) {
        return it->second;
    }

    textures.push_back(std::make_unique<Texture>(device, filepath, format));
    writeTextureToSlot(*textures.back(), nextIndex);

    uint32_t currentIndex = nextIndex;
    pathToIndex[filepath] = currentIndex;
    nextIndex++;

    return currentIndex;
}


void TextureManager::writeTextureToSlot(Texture& tex, uint32_t slot){
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = tex.getSampler();
    imageInfo.imageView = tex.getImageView();
    imageInfo.imageLayout = tex.getImageLayout();

    DescriptorWriter(*setLayout, *pool)
        .writeImage(0, &imageInfo, 1, slot)
        .overwrite(descriptorSet);
}

