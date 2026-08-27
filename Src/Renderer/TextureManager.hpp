#pragma once

#include "Device.hpp"
#include "Renderer/Descriptors.hpp"
#include "Renderer/Texture.hpp"

// std
#include <string>
#include <cmath>
#include <memory>
#include <unordered_map>
#include <vector>
#include <iostream>

class TextureManager{
    public:
        TextureManager(Device& device);

        uint32_t addTexture(const std::string& filepath);

        VkDescriptorSetLayout getLayout() const { return setLayout->getDescriptorSetLayout(); }
        VkDescriptorSet getDescriptorSet() const { return descriptorSet; }
    
    private:
        Device& device;
        std::unique_ptr<DescriptorSetLayout> setLayout;
        std::unique_ptr<DescriptorPool> pool;
        VkDescriptorSet descriptorSet;
        std::vector<std::unique_ptr<Texture>> textures;
        uint32_t nextIndex = 1;

        static constexpr uint32_t MAX_BINDLESS_TEXTURES = 1024;
};