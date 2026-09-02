#include "Rendering/Resources/TextureManager.hpp"

TextureManager::TextureManager(Device& _device, AsyncLoader& _loader, enki::TaskScheduler& _taskScheduler)
    : device{_device}, loader{_loader}, taskScheduler{_taskScheduler} {
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
    if (nextIndex >= MAX_BINDLESS_TEXTURES) {
        throw std::runtime_error("Maximum number of bindless textures reached.");
    }
    auto it = pathToIndex.find(filepath);
    if (it != pathToIndex.end()) {
        return it->second;
    }

    uint32_t slot = nextIndex++;
    pathToIndex[filepath] = slot;

    textures.push_back(std::make_unique<Texture>(device, loader, filepath, format));
    Texture* tex = textures.back().get();

    {
        std::lock_guard<std::mutex> lock(mutex);
        writeTextureToSlot(*textures[1], slot);  // white placeholder until the upload lands
        pendingTextures[slot] = tex;
    }

    // Register the pending entry above BEFORE the decode task can enqueue an
    // upload the I/O thread might retire.
    auto task = std::make_unique<TextureDecodeTask>();
    task->texture = tex;
    task->id = slot;
    taskScheduler.AddTaskSetToPipe(task.get());
    decodeTasks.push_back(std::move(task));

    return slot;
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


void TextureManager::update() {
    loader.update();
    for (uint32_t slot : loader.pollFinishedUploads()) {
        Texture* texture = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = pendingTextures.find(slot);
            if (it == pendingTextures.end()) continue;
            texture = it->second;
            pendingTextures.erase(it);
        }

        // The upload + ownership acquire are already complete on the GPU, so the
        // image is genuinely SHADER_READ_ONLY_OPTIMAL before its descriptor lands.
        texture->onUploadFinished();

        {
            std::lock_guard<std::mutex> lock(mutex);
            writeTextureToSlot(*texture, slot);
        }
    }
}
