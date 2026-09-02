#pragma once

#include "Rendering/RHI/Device.hpp"
#include "Rendering/Resources/Async_Loader.hpp"

// std
#include <string>
#include <cmath>

class Texture{
    public:
        Texture(Device &device, AsyncLoader& loader, const std::string &filepath, VkFormat format);
        Texture(Device &device, int width, int height, uint8_t* _data, VkFormat format, VkImageUsageFlags usage);
        ~Texture();

        VkImage getImage() { return image; }
        bool isReady() const { return ready; }
        // Runs on a worker thread: decodes the file, creates the GPU image/view/
        // sampler and queues the pixel upload with the AsyncLoader. `id` is the
        // caller's stable handle, echoed back through pollFinishedUploads().
        void asyncLoad(uint32_t id);
        // Called on the I/O thread once the upload + ownership acquire are done.
        void onUploadFinished();

        Texture(const Texture &) = delete;
        Texture&operator=(const Texture &) = delete;
        Texture(Texture &&) = delete;
        Texture &operator=(Texture &&) = delete;

        VkSampler getSampler() { return sampler; }
        VkImageView getImageView() { return imageView; }
        VkImageLayout getImageLayout() { return imageLayout; }

    private:
        void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
        void generateMipmaps();

        int width = 0, height = 0, mipLevels = 1;

        Device &device;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory imageMemory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkFormat imageFormat = VK_FORMAT_UNDEFINED;
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        AsyncLoader* loader = nullptr;
        std::string sourcePath;       // set for the async ctor; empty otherwise
        uint8_t* pendingPixelData = nullptr;
        bool ready = true;
};