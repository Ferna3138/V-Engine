#include "Simple_Render_System.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"

// Std
#include <cassert>
#include <stdexcept>
#include <array>

#include <glm/gtc/constants.hpp>


struct SimplePushConstantData {
    glm::mat4 modelMatrix{1.f};
    glm::mat4 normalMatrix{1.f};
};


SimpleRenderSystem::SimpleRenderSystem(Device& _device, 
                                        VkRenderPass renderPass, 
                                        VkDescriptorSetLayout globalSetLayout, 
                                        VkDescriptorSetLayout textureSetLayout) : device{_device} {
    createPipelineLayout(globalSetLayout, textureSetLayout);
    createPipeline(renderPass);
}

SimpleRenderSystem::~SimpleRenderSystem() { vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr); }

void SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout textureSetLayout) {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SimplePushConstantData);

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {globalSetLayout, textureSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }
}



void SimpleRenderSystem::createPipeline(VkRenderPass renderPass) {
    assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipeline = std::make_unique<Pipeline>(
        device,
        "Src/Shaders/simple_shader.vert.spv",
        "Src/Shaders/simple_shader.frag.spv",
        pipelineConfig);
}




void SimpleRenderSystem::renderGameObjects(FrameInfo &frameInfo) {
    pipeline->bind(frameInfo.commandBuffer);
    std::array<VkDescriptorSet, 2> sets{frameInfo.globalDescriptorSet, frameInfo.textureDescriptorSet};

    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0, static_cast<uint32_t>(sets.size()),
        sets.data(),
        0,
        nullptr
    );

    
    auto view = frameInfo.registry.view<TransformComponent, ModelComponent>();
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& modelComp = view.get<ModelComponent>(entity);

        if (!modelComp.model) continue;

        SimplePushConstantData push{};
        push.modelMatrix = toMat4(transform);
        push.normalMatrix = toNormalMatrix(transform);

        vkCmdPushConstants(frameInfo.commandBuffer,
                           pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(SimplePushConstantData),
                           &push);

        modelComp.model->bind(frameInfo.commandBuffer);
        modelComp.model->draw(frameInfo.commandBuffer); 
    }
}

