#include "Renderer/Async_Loader.hpp"
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
}

AsyncLoader::~AsyncLoader() {
    vkDestroyFence(device.device(), transferFence, nullptr);
    vkDestroySemaphore(device.device(), transferCompleteSemaphore, nullptr);
    vkFreeCommandBuffers(device.device(), device.getTransferCommandPool(), 1, &transferCommandBuffer);
}

void AsyncLoader::addUploadRequest(const UploadRequest& request) {
    std::lock_guard<std::mutex> lock(requestMutex);
    uploadRequests.push_back(request);
}

void AsyncLoader::update() {
    if (hasInFlightRequest) {
        if (vkGetFenceStatus(device.device(), transferFence) != VK_SUCCESS) { return; }
        {
            std::lock_guard<std::mutex> lock(finishedMutex);
            finishedUploads.push_back(inFlightRequest.image);
        }
        hasInFlightRequest = false;
    }

    UploadRequest request;
    {
        std::lock_guard<std::mutex> lock(requestMutex);
        if (uploadRequests.empty()) return;
        request = uploadRequests.back();
        uploadRequests.pop_back();
    }

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

    // transferCompleteSemaphore is unused for now — completion is tracked via transferFence
    // (polled in pollFinishedUploads()) and acquireAndFinalize() waits via vkQueueWaitIdle.
    // It'll be wired in once acquireAndFinalize() moves off that CPU-side wait onto a real
    // semaphore-gated submit (see the pinned I/O thread phase).
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &transferCommandBuffer;
    vkQueueSubmit(device.transferQueue(), 1, &submitInfo, transferFence);

    inFlightRequest = request;
    hasInFlightRequest = true;
}

void AsyncLoader::acquireAndFinalize(VkImage image) {
    QueueFamilyIndices indices = device.findPhysicalQueueFamilies();

    VkCommandBuffer commandBuffer = device.beginSingleTimeCommands();

    VkImageMemoryBarrier acquireBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    acquireBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    acquireBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    acquireBarrier.srcQueueFamilyIndex = indices.transferFamily;
    acquireBarrier.dstQueueFamilyIndex = indices.graphicsFamily;
    acquireBarrier.image = image;
    acquireBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    acquireBarrier.srcAccessMask = 0;
    acquireBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &acquireBarrier);

    device.endSingleTimeCommands(commandBuffer);
}

std::vector<VkImage> AsyncLoader::pollFinishedUploads() {
    std::lock_guard<std::mutex> lock(finishedMutex);
    std::vector<VkImage> result = std::move(finishedUploads);
    finishedUploads.clear();
    return result;
}