#pragma once

#include "Rendering/RHI/Device.hpp"
#include "Rendering/RHI/Pipeline.hpp"
#include "Rendering/Core/Frame_Info.hpp"

#include <memory>

// Depth-only pass: draws every model with position alone so the forward pass can
// run with cheap LESS_OR_EQUAL / no depth writes. Its render pass has no colour
// attachment.
class DepthPrepassSystem {
    public:
        DepthPrepassSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~DepthPrepassSystem();

        DepthPrepassSystem(const DepthPrepassSystem&) = delete;
        DepthPrepassSystem& operator=(const DepthPrepassSystem&) = delete;

        void render(FrameInfo& frameInfo);

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(VkRenderPass renderPass);

        Device& device;
        std::unique_ptr<Pipeline> pipeline;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};
