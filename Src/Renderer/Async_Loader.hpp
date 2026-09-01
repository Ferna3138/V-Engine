#pragma once

#include "Renderer/Buffer.hpp"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

struct UploadRequest {
    uint32_t id;          // stable id assigned by the caller (survives VkImage handle reuse)
    VkImage image;
    const void* data;
    size_t dataSize;
    uint32_t width;
    uint32_t height;
};

// Streams texture pixel data to the GPU on the dedicated transfer queue, then
// hands ownership to the graphics queue. Everything here runs on one pinned I/O
// thread except addUploadRequest() (any thread) and wakeForShutdown().
//
// Per-texture pipeline, one stage deep each:
//   queued  --(transfer submit, signals transferCompleteSemaphore)-->  uploading
//   uploading --(transferFence)-->  --(acquire submit, waits semaphore)-->  finalizing
//   finalizing --(finalizeFence)-->  finished  (id surfaced via pollFinishedUploads)
class AsyncLoader {
    public:
        AsyncLoader(Device& device);
        ~AsyncLoader();

        AsyncLoader(const AsyncLoader&) = delete;
        AsyncLoader& operator=(const AsyncLoader&) = delete;

        void addUploadRequest(const UploadRequest& request);
        void update();

        // Ids of textures whose pixels are uploaded AND whose image has been
        // acquired by the graphics queue (layout is SHADER_READ_ONLY_OPTIMAL).
        std::vector<uint32_t> pollFinishedUploads();

        // Blocks the I/O thread until there is something to do: a queued request,
        // an in-flight transfer/acquire to advance, or shutdown.
        void waitForWork();
        // Unblocks waitForWork() so the I/O thread can observe a shutdown request.
        void wakeForShutdown();
    private:
        static constexpr VkDeviceSize k_stagingBufferSize = 64 * 1024 * 1024;
        static constexpr uint64_t k_fenceWaitTimeoutNs = 100'000'000ull;  // 100 ms

        void submitTransfer(const UploadRequest& request);
        void submitAcquire(VkImage image);

        Device& device;
        Buffer stagingBuffer;

        VkCommandBuffer transferCommandBuffer;
        VkFence transferFence;
        // Signalled by the transfer submit, waited on by the acquire submit — the
        // cross-queue dependency for the queue-family-ownership transfer. Binary,
        // reused 1:1 because only one transfer is ever in flight.
        VkSemaphore transferCompleteSemaphore;

        // Dedicated graphics-family pool/fence for the ownership acquire, so the
        // I/O thread never touches the Device's shared command pool or stalls the
        // graphics queue.
        VkCommandPool finalizeCommandPool;
        VkCommandBuffer finalizeCommandBuffer;
        VkFence finalizeFence;

        std::vector<UploadRequest> uploadRequests;
        std::mutex requestMutex;
        std::condition_variable requestCv;
        bool shuttingDown = false;  // guarded by requestMutex

        // I/O-thread-only state.
        UploadRequest inFlightRequest{};
        bool hasInFlightRequest = false;
        UploadRequest finalizingRequest{};
        bool hasFinalizingRequest = false;

        std::vector<uint32_t> finishedUploads;
        std::mutex finishedMutex;
};
