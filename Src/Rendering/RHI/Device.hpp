#pragma once

#if defined(__APPLE__)
    #define VK_ENABLE_BETA_EXTENSIONS
#endif

#include "Foundation/Platform/Window.hpp"

// std lib headers
#include <string>
#include <vector>
#include <mutex>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_beta.h>


struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
    uint32_t graphicsFamily;
    uint32_t presentFamily;
    uint32_t transferFamily;
    bool graphicsFamilyHasValue = false;
    bool presentFamilyHasValue = false;
    bool transferFamilyHasValue = false;
    bool isComplete() { return graphicsFamilyHasValue && presentFamilyHasValue; }
};

class Device {
    public:
        #ifdef NDEBUG
            const bool enableValidationLayers = false;
        #else
            const bool enableValidationLayers = true;
        #endif

        Device(Window &window);
        ~Device();

        // Not copyable or movable
        Device(const Device &) = delete;
        Device& operator=(const Device &) = delete;
        Device(Device &&) = delete;
        Device &operator=(Device &&) = delete;

        VkCommandPool getCommandPool() { return commandPool; }
        VkCommandPool getTransferCommandPool() { return transferCommandPool; }

        VkPipelineCache getPipelineCache() { return pipelineCache; }
        VkDevice device() { return device_; }
        
        
        VkQueue transferQueue() { return transferQueue_; }

        VkSurfaceKHR surface() { return surface_; }
        VkQueue graphicsQueue() { return graphicsQueue_; }
        VkQueue presentQueue() { return presentQueue_; }

        // Guards ALL host access to the VkQueues (submit / present / wait-idle).
        // The async loader services uploads on its own thread, so every queue
        // operation on any thread - including vkDeviceWaitIdle, which the spec
        // requires to be externally synchronised against every queue - must hold
        // this. On GPUs without a dedicated transfer family the transfer and
        // graphics queues are the same handle, so one mutex covers both.
        std::mutex queueMutex;

        // Guards `loadCommandPool` (the pool beginSingleTimeCommands()/
        // endSingleTimeCommands() allocate from). Vulkan requires allocate/free -
        // and, per spec, recording - on a VkCommandPool to be externally
        // synchronised, and asset loading (model import, baked textures) can now
        // run off the main thread, so concurrent one-shot loads must serialise
        // against each other. This never contends with the render thread: its
        // per-frame command buffers come from a separate pool (`commandPool`).
        // Held for the whole begin/end span - locked in beginSingleTimeCommands(),
        // unlocked in endSingleTimeCommands().
        std::mutex commandPoolMutex;

        // vkDeviceWaitIdle, serialised against queue submissions on other threads.
        void waitIdle() {
            std::lock_guard<std::mutex> lock(queueMutex);
            vkDeviceWaitIdle(device_);
        }

        void createPipelineCache();
        void savePipelineCache();

        SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport(physicalDevice); }
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(physicalDevice); }
        VkFormat findSupportedFormat(
            const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

        // Buffer Helper Functions
        void createBuffer(
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties,
            VkBuffer &buffer,
            VkDeviceMemory &bufferMemory);
        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        void copyBufferToImage(
            VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);

        void createImageWithInfo(
            const VkImageCreateInfo &imageInfo,
            VkMemoryPropertyFlags properties,
            VkImage &image,
            VkDeviceMemory &imageMemory);

        VkPhysicalDeviceProperties properties;

        VkInstance getInstance() {return instance;}
        VkPhysicalDevice getPhysicalDevice() {return physicalDevice;}

    private:
        void createInstance();
        void setupDebugMessenger();
        void createSurface();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createCommandPool();
        void createTransferCommandPool();
        void createLoadCommandPool();

        // helper functions
        bool isDeviceSuitable(VkPhysicalDevice device);
        std::vector<const char *> getRequiredExtensions();
        bool checkValidationLayerSupport();
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
        void hasGflwRequiredInstanceExtensions();
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

        VkInstance instance;
        VkDebugUtilsMessengerEXT debugMessenger;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        Window &window;
        VkCommandPool commandPool;         // Renderer's per-frame primary command buffers only.
        VkCommandPool transferCommandPool; // AsyncLoader's dedicated streaming pool only.
        VkCommandPool loadCommandPool;     // beginSingleTimeCommands()/endSingleTimeCommands() only - kept
                                            // separate from `commandPool` so a background model import's
                                            // one-shot allocate/free never races the render thread's
                                            // per-frame recording on the same VkCommandPool.

        VkDevice device_;
        VkSurfaceKHR surface_;
        VkQueue graphicsQueue_;
        VkQueue presentQueue_;
        VkQueue transferQueue_;

        const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

        #if defined(__APPLE__)
        const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, 
                                                            VK_KHR_MAINTENANCE1_EXTENSION_NAME,
                                                            VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME};
        #else
        const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, 
                                                            VK_KHR_MAINTENANCE1_EXTENSION_NAME,

                                                            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
                                                            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                                                            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                                                            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                                                            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                                                            VK_KHR_SPIRV_1_4_EXTENSION_NAME,
                                                            VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
                                                    };
        #endif

        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        static inline const std::string pipelineCachePath = "pipeline_cache.bin";

        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
        };

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
        };

        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeature{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES
        };

};