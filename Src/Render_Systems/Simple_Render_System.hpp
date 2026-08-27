#pragma once
#include "Systems/Camera.hpp"
#include "Renderer/Device.hpp"
#include "Renderer/Pipeline.hpp"
//#include "Components/Game_Object.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "Components/Components.hpp"

#include "Frame_Info.hpp"

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