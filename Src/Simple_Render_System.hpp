#pragma once
#include "Camera.hpp"
#include "Device.hpp"
#include "Pipeline.hpp"
#include "Game_Object.hpp"

// Std
#include <memory>
#include <vector>

class SimpleRenderSystem{
    public:
        SimpleRenderSystem(Device& device, VkRenderPass renderPass);
        ~SimpleRenderSystem();

        SimpleRenderSystem (const SimpleRenderSystem&) = delete;
        SimpleRenderSystem&operator = (const SimpleRenderSystem&) = delete;

        void renderGameObjects(VkCommandBuffer commandBuffer, std::vector<GameObject>& gameObjects, const Camera& camera);

    private:
        void createPipelineLayout();
        void createPipeline(VkRenderPass renderPass);
        
        Device& device;
        std::unique_ptr<Pipeline> pipeline;
        VkPipelineLayout pipelineLayout;
  };