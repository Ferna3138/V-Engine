#pragma once

#include "Device.hpp"
#include "Renderer/Descriptors.hpp"
#include "Renderer/Texture.hpp"
#include "Renderer/Async_Loader.hpp"

// std
#include <string>
#include <memory>
#include <vector>
#include <stdexcept>
#include <unordered_map>

class TextureManager{
    public:
        TextureManager(Device& device, AsyncLoader& loader);
        void update();

        uint32_t addTexture(const std::string& filepath, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

        VkDescriptorSetLayout getLayout() const { return setLayout->getDescriptorSetLayout(); }
        VkDescriptorSet getDescriptorSet() const { return descriptorSet; }
    
    private:
        Device& device;
        
        uint8_t blackPixel[4]  = {0, 0, 0, 255};
        uint8_t whitePixel[4]  = {255, 255, 255, 255};
        uint8_t flatNormal[4]  = {128, 128, 255, 255};

        std::unique_ptr<DescriptorSetLayout> setLayout;
        std::unique_ptr<DescriptorPool> pool;
        VkDescriptorSet descriptorSet;
        
        std::vector<std::unique_ptr<Texture>> textures;
        uint32_t nextIndex = 3;
        
        std::unordered_map<std::string, uint32_t> pathToIndex;

        static constexpr uint32_t MAX_BINDLESS_TEXTURES = 1024;

        void writeTextureToSlot(Texture&, uint32_t slot);

        AsyncLoader& loader;
        struct PendingTexture { Texture* texture; uint32_t slot; };
        std::unordered_map<VkImage, PendingTexture> pendingTextures;
    };