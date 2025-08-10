#pragma once
#include "Systems/Camera.hpp"
#include "Renderer/Device.hpp"
#include "Renderer/Pipeline.hpp"

#include "Frame_Info.hpp"

class RayTracing{

    void initRayTracing();

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};


    private:
        Device& device;
};