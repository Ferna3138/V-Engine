#pragma once

#include "Device.hpp"
#include "Renderer/Descriptors.hpp"
#include "Renderer/Texture.hpp"
#include "Renderer/Async_Loader.hpp"

#include "TaskScheduler.h"

// std
#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <stdexcept>
#include <unordered_map>

class TextureManager{
    public:
        TextureManager(Device& device, AsyncLoader& loader, enki::TaskScheduler& taskScheduler);
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
        enki::TaskScheduler& taskScheduler;

        // File decode + GPU resource creation for one streamed texture, run on a
        // worker thread. Self-owned by TextureManager (kept alive until shutdown,
        // when WaitforAllAndShutdown() has drained every task).
        struct TextureDecodeTask : enki::ITaskSet {
            Texture* texture = nullptr;
            uint32_t id = 0;
            void ExecuteRange(enki::TaskSetPartition, uint32_t) override { texture->asyncLoad(id); }
        };
        std::vector<std::unique_ptr<TextureDecodeTask>> decodeTasks;

        // slot index -> texture still streaming in. Keyed by the stable slot id,
        // not VkImage (which doesn't exist until the decode task runs).
        std::unordered_map<uint32_t, Texture*> pendingTextures;

        // addTexture() runs on the main thread; update() runs on the I/O thread.
        // Guards pendingTextures and every writeTextureToSlot() (i.e. host access
        // to the shared bindless descriptor set).
        std::mutex mutex;
    };