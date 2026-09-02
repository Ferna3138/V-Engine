#pragma once
#include "Rendering/RHI/Device.hpp"
#include "Rendering/RHI/Pipeline.hpp"
#include "Rendering/Core/Frame_Info.hpp"
#include <glm/gtc/matrix_transform.hpp>
// Std
#include <memory>
#include <vector>

class PointLightSystem{
    public:
        struct PointLightPushConstants{
            glm::vec4 position{};
            glm::vec4 colour{};
            float radius;
        };
        
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