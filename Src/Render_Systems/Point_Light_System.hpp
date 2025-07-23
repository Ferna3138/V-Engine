#pragma once
#include "Systems/Camera.hpp"
#include "Renderer/Device.hpp"
#include "Renderer/Pipeline.hpp"
#include "Components/Game_Object.hpp"
#include "Frame_Info.hpp"

// Std
#include <memory>
#include <vector>

class PointLightSystem{
    public:
        PointLightSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~PointLightSystem();

        PointLightSystem (const PointLightSystem&) = delete;
        PointLightSystem&operator = (const PointLightSystem&) = delete;

        void update(FrameInfo& frameInfo, GlobalUbo& ubo);
        void render(FrameInfo &frameInfo);

    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(VkRenderPass renderPass);
        
        Device& device;
        std::unique_ptr<Pipeline> pipeline;
        VkPipelineLayout pipelineLayout;
  };