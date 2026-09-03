#include "Rendering/Passes/Full_Screen_System.hpp"
#include <stdexcept>


FullscreenPass::FullscreenPass(Device& device, VkRenderPass renderPass, DescriptorPool& pool,
                               const std::string& fragSpvPath,
                               const std::vector<VkImageView>& sampledInputs,
                               uint32_t pushConstantSize)
    : device{device} {
    createSampler();
    createDescriptor(pool, sampledInputs);
    createPipelineLayout(pushConstantSize);
    createPipeline(renderPass, fragSpvPath);
}

FullscreenPass::~FullscreenPass() {
    vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    vkDestroySampler(device.device(), sampler, nullptr);
}

void FullscreenPass::createSampler() {
    VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    info.magFilter = info.minFilter = VK_FILTER_LINEAR;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressModeU = info.addressModeV = info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    info.maxLod = 0.0f;
    if (vkCreateSampler(device.device(), &info, nullptr, &sampler) != VK_SUCCESS)
        throw std::runtime_error("FullscreenPass: sampler");
}

void FullscreenPass::createDescriptor(DescriptorPool& pool, const std::vector<VkImageView>& inputs) {
    DescriptorSetLayout::Builder builder(device);
    for (uint32_t i = 0; i < inputs.size(); ++i)
        builder.addBinding(i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    setLayout = builder.build();

    std::vector<VkDescriptorImageInfo> infos(inputs.size());   // pre-sized: no realloc, pointers stay valid
    DescriptorWriter writer(*setLayout, pool);
    for (uint32_t i = 0; i < inputs.size(); ++i) {
        infos[i].sampler = sampler;
        infos[i].imageView = inputs[i];
        infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        writer.writeImage(i, &infos[i]);
    }
    writer.build(descriptorSet);
}

void FullscreenPass::createPipelineLayout(uint32_t pushConstantSize) {
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    range.size = pushConstantSize;

    VkDescriptorSetLayout layouts[] = { setLayout->getDescriptorSetLayout() };
    VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    info.setLayoutCount = 1;
    info.pSetLayouts = layouts;
    info.pushConstantRangeCount = pushConstantSize > 0 ? 1 : 0;
    info.pPushConstantRanges     = pushConstantSize > 0 ? &range : nullptr;
    if (vkCreatePipelineLayout(device.device(), &info, nullptr, &pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("FullscreenPass: pipeline layout");
}

void FullscreenPass::createPipeline(VkRenderPass renderPass, const std::string& fragSpvPath) {
    PipelineConfigInfo config{};
    Pipeline::defaultPipelineConfigInfo(config);
    config.bindingDescriptions.clear();
    config.attributeDescriptions.clear();
    config.depthStencilInfo.depthTestEnable  = VK_FALSE;
    config.depthStencilInfo.depthWriteEnable = VK_FALSE;
    config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    config.renderPass = renderPass;
    config.pipelineLayout = pipelineLayout;
    pipeline = std::make_unique<Pipeline>(
        device, "Src/Rendering/Shaders/fullscreen.vert.spv", fragSpvPath, config);
}

void FullscreenPass::render(VkCommandBuffer cb, const void* pushData, uint32_t pushDataSize) {
    pipeline->bind(cb);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    if (pushDataSize > 0 && pushData)
        vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, pushDataSize, pushData);
    vkCmdDraw(cb, 3, 1, 0, 0);
}