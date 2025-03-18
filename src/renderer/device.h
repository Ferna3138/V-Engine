#pragma once
#define VULKAN_HPP_NO_EXCEPTIONS
#include <vulkan/vulkan.hpp>

bool supports(
    const vk::PhysicalDevice& device,
    const char** ppRequestedExtensions,
    const uint32_t requestedExtensionCount);


bool is_suitable(const vk::PhysicalDevice& device);

vk::PhysicalDevice choose_physical_device(const vk::Instance& instance);