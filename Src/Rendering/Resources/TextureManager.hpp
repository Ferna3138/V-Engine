#pragma once

#include "Rendering/RHI/Device.hpp"
#include "Rendering/RHI/Descriptors.hpp"
#include "Rendering/Resources/Texture.hpp"
#include "Rendering/Resources/Async_Loader.hpp"

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

        // Creates (or returns a cached) 1x1 texture with a constant colour. Used to
        // bake scalar material factors (e.g. an OBJ shininess -> roughness value)
        // into an MR texture the bindless shader can sample uniformly. Uploaded
        // synchronously; `key` must be unique per colour.
        uint32_t addRawTexture(const std::string& key, uint8_t r, uint8_t g, uint8_t b,
                               uint8_t a = 255, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

        VkDescriptorSetLayout getLayout() const { return setLayout->getDescriptorSetLayout(); }
        VkDescriptorSet getDescriptorSet() const { return descriptorSet; }

        // For UI previews (e.g. an Inspector thumbnail) that need the raw
        // sampler/image behind a bindless slot rather than sampling it in a
        // shader. Returns nullptr if the slot is unused; check isReady() before
        // trusting the image's contents/layout - a still-streaming texture has
        // a valid VkImageView/VkSampler but the pixel data (and final layout)
        // may not have landed yet.
        Texture* getTexture(uint32_t slot);

    private:
        Device& device;
        
        // Reserved fallback slots (0,1,2). MR neutral = AO 1, roughness 1, metal 0
        // so an untextured surface is matte dielectric, never chrome.
        uint8_t mrNeutral[4]   = {255, 255, 0, 255};
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

        // update() runs on the I/O thread; addTexture()/addRawTexture() run on
        // the main thread or a background model-import worker thread. Guards
        // nextIndex, pathToIndex, textures, decodeTasks, pendingTextures, and
        // every writeTextureToSlot() (i.e. host access to the shared bindless
        // descriptor set).
        std::mutex mutex;
    };