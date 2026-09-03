#pragma once

#include "Rendering/Core/Camera.hpp"
#include "Rendering/Core/Render_Scene.hpp"


// Lib
#include <vulkan/vulkan.h>

#define MAX_LIGHTS 10

struct PointLight {
    glm::vec4 position{}; // Ignore w
    glm::vec4 colour{}; // W is Intensity
};

struct GlobalUbo{
    glm::mat4 projection{1.f};
    glm::mat4 view{1.f};
    glm::mat4 inverseView{1.f};
    glm::mat4 inverseProj{1.f};
    glm::vec4 ambientLightColour{1.f, 1.f, 1.f, 0.02f}; // W is intensity
    PointLight pointLights[MAX_LIGHTS];    
    int numLights;

};


struct FrameInfo {
    int frameIndex;
    float frameTime;
    VkCommandBuffer commandBuffer;
    Camera& camera;
    VkDescriptorSet globalDescriptorSet;
    VkDescriptorSet textureDescriptorSet;
    const RenderScene& renderScene;
};
