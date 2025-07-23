#pragma once

#include "Systems/Camera.hpp"
#include "Components/Game_Object.hpp"

// Lib
#include <vulkan/vulkan.h>

struct FrameInfo {
    int frameIndex;
    float frameTime;
    VkCommandBuffer commandBuffer;
    Camera& camera;

    VkDescriptorSet globalDescriptorSet;
    GameObject::Map gameObjects;
};