#include "Rendering/Passes/Depth_Prepass_System.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"

#include <stdexcept>

// Must match the Push block in simple_shader.vert.
struct DepthPushConstantData {
    glm::mat4 modelMatrix{1.f};
    glm::mat4 normalMatrix{1.f};
};

DepthPrepassSystem::DepthPrepassSystem(Device& _device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
    : device{_device} {
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
}

DepthPrepassSystem::~DepthPrepassSystem() {
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
}

void DepthPrepassSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(DepthPushConstantData);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &globalSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create depth pre-pass pipeline layout!");
    }
}

void DepthPrepassSystem::createPipeline(VkRenderPass renderPass) {
    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    // Render pass has no colour attachment.
    pipelineConfig.colorBlendInfo.attachmentCount = 0;
    pipelineConfig.colorBlendInfo.pAttachments = nullptr;
    // depth_only.vert reads position only; keep the full-vertex stride, drop the
    // rest of the attributes.
    pipelineConfig.attributeDescriptions = { {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0} };
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipeline = std::make_unique<Pipeline>(
        device,
        "Src/Rendering/Shaders/depth_only.vert.spv",
        "Src/Rendering/Shaders/depth_only.frag.spv",
        pipelineConfig);
}

void DepthPrepassSystem::render(FrameInfo& frameInfo) {
    pipeline->bind(frameInfo.commandBuffer);

    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0, 1, &frameInfo.globalDescriptorSet,
        0, nullptr);

    for (const auto& object : frameInfo.renderScene.objects) {
        if (!object.model) continue;

        DepthPushConstantData push{};
        push.modelMatrix = object.modelMatrix;
        push.normalMatrix = object.normalMatrix;

        vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DepthPushConstantData), &push);

        object.model->bind(frameInfo.commandBuffer);
        object.model->draw(frameInfo.commandBuffer);
    }
}
