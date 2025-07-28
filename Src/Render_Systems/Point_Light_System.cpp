#include "Point_Light_System.hpp"

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
    pipelineConfig.attributeDescriptions.clear();
    pipelineConfig.bindingDescriptions.clear();

    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    pipeline = std::make_unique<Pipeline>(
        device,
        "../Src/Shaders/point_light.vert.spv",
        "../Src/Shaders/point_light.frag.spv",
        pipelineConfig);
}


void PointLightSystem::update(FrameInfo &frameInfo, GlobalUbo &ubo) {
    auto rotateLight = glm::rotate(glm::mat4(1.f), frameInfo.frameTime, glm::vec3(0.f, -1.f, 0.f));
    
    int lightIndex = 0;
    for(auto& kv : frameInfo.gameObjects){
        auto& obj = kv.second;
        if(obj.pointLight == nullptr) continue;

        assert(lightIndex < MAX_LIGHTS && "Exceeded maximum number of lights");
        
        // Update light position
        obj.transform.translation = glm::vec3(rotateLight * glm::vec4(obj.transform.translation, 1.f));

        // Copy to ubo
        ubo.pointLights[lightIndex].position = glm::vec4(obj.transform.translation, 1.f);
        ubo.pointLights[lightIndex].colour = glm::vec4(obj.colour, obj.pointLight->lightIntensity);
        lightIndex += 1;
    }
    ubo.numLights = lightIndex;
}


void PointLightSystem::render(FrameInfo &frameInfo) {
    // Sort lights
    std::map<float, GameObject::id_t> sortedLights;
    for(auto& kv : frameInfo.gameObjects){ 
        auto& obj = kv.second;
        if(obj.pointLight == nullptr) continue;

        auto offset = frameInfo.camera.getPosition() - obj.transform.translation;
        float disSquared = glm::dot(offset, offset);
        sortedLights[disSquared] = obj.getId();
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


    // Iterate through sorted lights in reverse order
    for(auto it = sortedLights.rbegin(); it != sortedLights.rend(); ++it) {
        auto& obj = frameInfo.gameObjects.at(it->second);


        PointLightPushConstants push{};
        push.position = glm::vec4(obj.transform.translation, 1.f);
        push.colour = glm::vec4(obj.colour, obj.pointLight->lightIntensity);
        push.radius = obj.transform.scale.x; // Assuming uniform scale for point light radius


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