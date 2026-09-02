#include "Rendering/Passes/Point_Light_System.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"

// Std
#include <array>
#include <cassert>
#include <map>
#include <stdexcept>

#include <glm/gtc/constants.hpp>


PointLightSystem::PointLightSystem(Device& _device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) : device{_device} {
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
}

PointLightSystem::~PointLightSystem() { vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr); }

void PointLightSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
    
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PointLightPushConstants);

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {globalSetLayout};

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



void PointLightSystem::createPipeline(VkRenderPass renderPass) {
    assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

    PipelineConfigInfo pipelineConfig{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfig);
    Pipeline::enableAlphaBlending(pipelineConfig);
    // Depth pre-pass already ran; test against it, don't fight it.
    pipelineConfig.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineConfig.attributeDescriptions.clear();
    pipelineConfig.bindingDescriptions.clear();

    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipeline = std::make_unique<Pipeline>(
        device,
        "Src/Rendering/Shaders/point_light.vert.spv",
        "Src/Rendering/Shaders/point_light.frag.spv",
        pipelineConfig);
}


void PointLightSystem::update(FrameInfo &frameInfo, GlobalUbo &ubo) {
    int lightIndex = 0;

    for (const auto& light : frameInfo.renderScene.lights) {
        assert(lightIndex < MAX_LIGHTS && "Exceeded maximum number of lights");

        ubo.pointLights[lightIndex].position = glm::vec4(light.position, light.radius);
        ubo.pointLights[lightIndex].colour   = light.colour; // full vec4, alpha = intensity

        ++lightIndex;
    }

    ubo.numLights = lightIndex;
}



void PointLightSystem::render(FrameInfo &frameInfo) {
    // Sort back-to-front so alpha-blended billboards composite correctly.
    std::map<float, const RenderLight*> sortedLights;
    for (const auto& light : frameInfo.renderScene.lights) {
        glm::vec3 offset = frameInfo.camera.getPosition() - light.position;
        sortedLights[glm::dot(offset, offset)] = &light;
    }

    pipeline->bind(frameInfo.commandBuffer);

    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0, 1,
        &frameInfo.globalDescriptorSet,
        0,
        nullptr
    );

    for (auto it = sortedLights.rbegin(); it != sortedLights.rend(); ++it) {
        const RenderLight& light = *it->second;

        PointLightPushConstants push{};
        push.position = glm::vec4(light.position, 1.f);
        push.colour   = light.colour; // vec4, alpha = intensity
        push.radius   = light.radius;

        vkCmdPushConstants(
            frameInfo.commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PointLightPushConstants),
            &push
        );

        vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
    }
}