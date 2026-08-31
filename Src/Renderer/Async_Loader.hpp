#pragma once

#include "Renderer/Buffer.hpp"
#include <mutex>
#include <vector>

struct UploadRequest {
    VkImage image;
    const void* data;
    size_t dataSize;
    uint32_t width;
    uint32_t height;
};

class AsyncLoader {
    public:
        AsyncLoader(Device& device);
        ~AsyncLoader();

        AsyncLoader(const AsyncLoader&) = delete;
        AsyncLoader& operator=(const AsyncLoader&) = delete;

        void addUploadRequest(const UploadRequest& request);
        void update();
        void acquireAndFinalize(VkImage image);

        std::vector<VkImage> pollFinishedUploads();
    private:
        static constexpr VkDeviceSize k_stagingBufferSize = 64 * 1024 * 1024;

        Device& device;
        Buffer stagingBuffer;
        VkCommandBuffer transferCommandBuffer;
        VkFence transferFence;
        VkSemaphore transferCompleteSemaphore;

        std::vector<UploadRequest> uploadRequests;
        std::mutex requestMutex;

        UploadRequest inFlightRequest;
        bool hasInFlightRequest = false;

        std::vector<VkImage> finishedUploads;
        std::mutex finishedMutex;
};