#include "Renderer/Async_Loader.hpp"
#include <cassert>
#include <cstring>

AsyncLoader::AsyncLoader(Device& _device)
    : device{_device},
      stagingBuffer{
          device,
          k_stagingBufferSize,
          1,
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
      } {
    stagingBuffer.map();
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = device.getTransferCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(device.device(), &allocInfo, &transferCommandBuffer);

    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &transferCompleteSemaphore);

    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(device.device(), &fenceInfo, nullptr, &transferFence);

    // Own graphics-family pool + fence for the ownership acquire, so finalization
    // never races the main thread on the Device's shared command pool / queue.
    QueueFamilyIndices indices = device.findPhysicalQueueFamilies();
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.queueFamilyIndex = indices.graphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(device.device(), &poolInfo, nullptr, &finalizeCommandPool);

    VkCommandBufferAllocateInfo finalizeAllocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    finalizeAllocInfo.commandPool = finalizeCommandPool;
    finalizeAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    finalizeAllocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(device.device(), &finalizeAllocInfo, &finalizeCommandBuffer);

    VkFenceCreateInfo finalizeFenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCreateFence(device.device(), &finalizeFenceInfo, nullptr, &finalizeFence);
}

AsyncLoader::~AsyncLoader() {
    vkDestroyFence(device.device(), finalizeFence, nullptr);
    vkDestroyCommandPool(device.device(), finalizeCommandPool, nullptr);  // frees its buffer too
    vkDestroyFence(device.device(), transferFence, nullptr);
    vkDestroySemaphore(device.device(), transferCompleteSemaphore, nullptr);
    vkFreeCommandBuffers(device.device(), device.getTransferCommandPool(), 1, &transferCommandBuffer);
}

void AsyncLoader::addUploadRequest(const UploadRequest& request) {
    {
        std::lock_guard<std::mutex> lock(requestMutex);
        uploadRequests.push_back(request);
    }
    requestCv.notify_one();
}

void AsyncLoader::waitForWork() {
    // Advance an in-flight stage: block (bounded) on whichever fence we're
    // waiting on, then let the caller run update(). These flags are only ever
    // written on this same (I/O) thread.
    if (hasInFlightRequest) {
        vkWaitForFences(device.device(), 1, &transferFence, VK_TRUE, k_fenceWaitTimeoutNs);
        return;
    }
    if (hasFinalizingRequest) {
        vkWaitForFences(device.device(), 1, &finalizeFence, VK_TRUE, k_fenceWaitTimeoutNs);
        return;
    }
    std::unique_lock<std::mutex> lock(requestMutex);
    requestCv.wait(lock, [this] { return shuttingDown || !uploadRequests.empty(); });
}

void AsyncLoader::wakeForShutdown() {
    {
        std::lock_guard<std::mutex> lock(requestMutex);
        shuttingDown = true;
    }
    requestCv.notify_all();
}

void AsyncLoader::update() {
    // 1. Retire a completed acquire -> the texture is fully resident.
    if (hasFinalizingRequest &&
        vkGetFenceStatus(device.device(), finalizeFence) == VK_SUCCESS) {
        {
            std::lock_guard<std::mutex> lock(finishedMutex);
            finishedUploads.push_back(finalizingRequest.id);
        }
        hasFinalizingRequest = false;
    }

    // 2. Retire a completed transfer -> hand the image to the graphics queue.
    //    Only when the single finalize slot is free; until then the transfer's
    //    semaphore signal stays unconsumed and no new transfer may start.
    if (hasInFlightRequest && !hasFinalizingRequest &&
        vkGetFenceStatus(device.device(), transferFence) == VK_SUCCESS) {
        submitAcquire(inFlightRequest.image);
        finalizingRequest = inFlightRequest;
        hasFinalizingRequest = true;
        hasInFlightRequest = false;
    }

    // 3. Start the next transfer (semaphore is now guaranteed unsignalled).
    if (!hasInFlightRequest) {
        UploadRequest request;
        {
            std::lock_guard<std::mutex> lock(requestMutex);
            if (uploadRequests.empty()) return;
            request = uploadRequests.back();
            uploadRequests.pop_back();
        }
        submitTransfer(request);
        inFlightRequest = request;
        hasInFlightRequest = true;
    }
}

void AsyncLoader::submitTransfer(const UploadRequest& request) {
    // A texture larger than the staging buffer would silently corrupt memory.
    assert(request.dataSize <= k_stagingBufferSize && "texture exceeds AsyncLoader staging buffer");

    vkResetFences(device.device(), 1, &transferFence);

    memcpy(stagingBuffer.getMappedMemory(), request.data, request.dataSize);

    vkResetCommandBuffer(transferCommandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

    VkImageMemoryBarrier toTransferDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.image = request.image;
    toTransferDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    toTransferDst.srcAccessMask = 0;
    toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(transferCommandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toTransferDst);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent = { request.width, request.height, 1 };
    vkCmdCopyBufferToImage(transferCommandBuffer, stagingBuffer.getBuffer(), request.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    QueueFamilyIndices indices = device.findPhysicalQueueFamilies();
    VkImageMemoryBarrier releaseToGraphics{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    releaseToGraphics.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    releaseToGraphics.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    releaseToGraphics.srcQueueFamilyIndex = indices.transferFamily;
    releaseToGraphics.dstQueueFamilyIndex = indices.graphicsFamily;
    releaseToGraphics.image = request.image;
    releaseToGraphics.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    releaseToGraphics.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    releaseToGraphics.dstAccessMask = 0;
    vkCmdPipelineBarrier(transferCommandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &releaseToGraphics);

    vkEndCommandBuffer(transferCommandBuffer);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &transferCommandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &transferCompleteSemaphore;
    vkQueueSubmit(device.transferQueue(), 1, &submitInfo, transferFence);
}

void AsyncLoader::submitAcquire(VkImage image) {
    QueueFamilyIndices indices = device.findPhysicalQueueFamilies();

    vkResetCommandBuffer(finalizeCommandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(finalizeCommandBuffer, &beginInfo);

    VkImageMemoryBarrier acquireBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    acquireBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    acquireBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    acquireBarrier.srcQueueFamilyIndex = indices.transferFamily;
    acquireBarrier.dstQueueFamilyIndex = indices.graphicsFamily;
    acquireBarrier.image = image;
    acquireBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    acquireBarrier.srcAccessMask = 0;
    acquireBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(finalizeCommandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &acquireBarrier);

    vkEndCommandBuffer(finalizeCommandBuffer);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &transferCompleteSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &finalizeCommandBuffer;

    vkResetFences(device.device(), 1, &finalizeFence);
    {
        // vkQueueSubmit must be externally synchronised per queue — the main
        // thread submits render work to the same graphics queue.
        std::lock_guard<std::mutex> lock(device.graphicsQueueMutex);
        vkQueueSubmit(device.graphicsQueue(), 1, &submitInfo, finalizeFence);
    }
    // No CPU wait here: completion is picked up by update()'s finalizeFence poll,
    // so the graphics queue is never flushed.
}

std::vector<uint32_t> AsyncLoader::pollFinishedUploads() {
    std::lock_guard<std::mutex> lock(finishedMutex);
    std::vector<uint32_t> result = std::move(finishedUploads);
    finishedUploads.clear();
    return result;
}
