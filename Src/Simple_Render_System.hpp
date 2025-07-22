#pragma once
#include "Systems/Camera.hpp"
#include "Renderer/Device.hpp"
#include "Renderer/Pipeline.hpp"
#include "Components/Game_Object.hpp"

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