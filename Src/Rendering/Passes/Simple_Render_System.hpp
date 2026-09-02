#pragma once
#include "Rendering/RHI/Device.hpp"
#include "Rendering/RHI/Pipeline.hpp"
#include "Rendering/Core/Frame_Info.hpp"

// Std
#include <memory>
#include <vector>

class SimpleRenderSystem{
    public:
        SimpleRenderSystem(Device& device,
            VkRenderPass renderPass,
            VkDescriptorSetLayout globalSetLayout,
            VkDescriptorSetLayout textureSetLayout);
        ~SimpleRenderSystem();

        SimpleRenderSystem (const SimpleRenderSystem&) = delete;
        SimpleRenderSystem&operator = (const SimpleRenderSystem&) = delete;

        void renderGameObjects(FrameInfo &frameInfo);

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout,
                                    VkDescriptorSetLayout textureSetLayout);
        void createPipeline(VkRenderPass renderPass);
        
        Device& device;
        std::unique_ptr<Pipeline> pipeline;
        VkPipelineLayout pipelineLayout;
  };