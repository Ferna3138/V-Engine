#pragma once

#include "Device.hpp"
#include "Renderer/Async_Loader.hpp"

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

        int width, height, mipLevels;

        Device &device;
        VkImage image;
        VkDeviceMemory imageMemory;
        VkImageView imageView;
        VkSampler sampler;
        VkFormat imageFormat;
        VkImageLayout imageLayout;

        AsyncLoader* loader = nullptr;
        uint8_t* pendingPixelData = nullptr;
        bool ready = true;
};