#pragma once
#include "Rendering/Core/Camera.hpp"
#include "Rendering/RHI/Device.hpp"
#include "Rendering/RHI/Pipeline.hpp"

#include "Rendering/Core/Frame_Info.hpp"

class RayTracing{

    void initRayTracing();

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};


    private:
        Device& device;
};