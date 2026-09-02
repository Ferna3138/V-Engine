#pragma once
#include "Rendering/RHI/Device.hpp"
#include "Rendering/RHI/Swap_Chain.hpp"
#include "Foundation/Platform/Window.hpp"

// Std
#include <cassert>
#include <memory>
#include <vector>

class Renderer {
    public:
        Renderer(Window& window, Device& device);
        ~Renderer();

        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;

        VkRenderPass getSwapChainRenderPass() const { return swapChain->getRenderPass(); }

        float getAspectRatio() const { return swapChain->extentAspectRatio(); }

        VkExtent2D getSwapChainExtent() const { return swapChain->getSwapChainExtent(); }

        VkImage getCurrentSwapChainImage() const { return swapChain->getImage(currentImageIndex); }

        bool isFrameInProgress() const { return isFrameStarted; }

        VkCommandBuffer getCurrentCommandBuffer() const {
            assert(isFrameStarted && "Cannot get command buffer when frame is not in progress");
            return commandBuffers[currentFrameIndex]; 
        }

        VkCommandBuffer beginFrame();
        void endFrame();
        void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);  
        void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

        int getFrameIndex() const {
            assert(isFrameStarted && "Cannot get frame index when frame is not in progress");
            return currentFrameIndex;
        }

        VkFormat getSwapChainFormat() { return swapChain->getSwapChainImageFormat(); }

        static constexpr int kNumRecorders = 3;  // SimpleRenderSystem, PointLightSystem, ImGui

        std::vector<VkCommandPool> secondaryCommandPools;      // size MAX_FRAMES_IN_FLIGHT * kNumRecorders
        std::vector<VkCommandBuffer> secondaryCommandBuffers;  // same size/indexing

        VkCommandBuffer getSecondaryCommandBuffer(int recorderIndex) const {
            return secondaryCommandBuffers[currentFrameIndex * kNumRecorders + recorderIndex];
        }

    private:
        void createCommandBuffers();
        void freeCommandBuffers();
        void recreateSwapChain();

        Window& window;
        Device& device;
        std::unique_ptr<SwapChain> swapChain;
        std::vector<VkCommandBuffer> commandBuffers;

        uint32_t currentImageIndex;
        int currentFrameIndex = 0;
        bool isFrameStarted = false;

        void createSecondaryCommandBuffers();
        void freeSecondaryCommandBuffers();
};