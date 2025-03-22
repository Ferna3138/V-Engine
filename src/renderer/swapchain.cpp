#include "swapchain.h"
#include "../logging/logger.h"

SurfaceDetails Swapchain::query_surface_support(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface) {

}


vk::Extent2D Swapchain::choose_extent(uint32_t width, uint32_t height, vk::SurfaceCapabilitiesKHR capabilities) {
    if(capabilities.currentExtent.width != UINT32_MAX){
        return capabilities.currentExtent;
    }

    vk::Extent2D extent;

    extent.width = std::min(
        capabilities.maxImageExtent.width,
        std::max(capabilities.minImageExtent.width, width));

    extent.height = std::min(
        capabilities.maxImageExtent.height,
        std::max(capabilities.minImageExtent.height, height));

    return extent;
}


vk::PresentModeKHR Swapchain::choose_present_mode(std::vector<vk::PresentModeKHR> presentModes) {
    
}


vk::SurfaceFormatKHR Swapchain::choose_surface_format(std::vector<vk::SurfaceFormatKHR> formats) {

}