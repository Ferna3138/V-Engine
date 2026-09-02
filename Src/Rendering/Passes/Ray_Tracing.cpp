#include "Rendering/Passes/Ray_Tracing.hpp"

void RayTracing::initRayTracing(){
  // Requesting ray tracing properties
  VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  prop2.pNext = &m_rtProperties;
  vkGetPhysicalDeviceProperties2(device.getPhysicalDevice(), &prop2);
}