#pragma once

#include "Rendering/RHI/Device.hpp"
#include "Rendering/RHI/Pipeline.hpp"
#include "Rendering/RHI/Descriptors.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

class FullscreenPass {
    public:
        FullscreenPass(Device& device, VkRenderPass renderPass, DescriptorPool& pool,
                               const std::string& fragSpvPath,
                               const std::vector<VkImageView>& sampledInputs,
                               uint32_t pushConstantSize);
        ~FullscreenPass();
        
        FullscreenPass(const FullscreenPass&) = delete;
        FullscreenPass& operator=(const FullscreenPass&) = delete;

        void render(VkCommandBuffer cb, const void* pushData, uint32_t pushDataSize);
        
    private:
        void createSampler();
        void createDescriptor(DescriptorPool& pool, const std::vector<VkImageView>& inputs);
        void createPipelineLayout(uint32_t pushConstantSize) ;
        void createPipeline(VkRenderPass renderPass, const std::string& fragSpvPath);

        Device& device;

        VkSampler sampler = VK_NULL_HANDLE;

        std::unique_ptr<DescriptorSetLayout> setLayout;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        std::unique_ptr<Pipeline> pipeline;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};
