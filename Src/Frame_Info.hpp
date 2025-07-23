#pragma once

#include "Systems/Camera.hpp"

// Lib
#include <vulkan/vulkan.h>

struct FrameInfo {
    int frameIndex;
    float frameTime;
    VkCommandBuffer commandBuffer;
    Camera& camera;

    VkDescriptorSet globalDescriptorSet;
};