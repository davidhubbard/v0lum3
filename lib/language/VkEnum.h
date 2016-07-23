/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#pragma once

#ifndef VK_NULL_HANDLE
#error VkEnum.h requires <vulkan/vulkan.h> to be included before it.
#else

namespace language {
namespace VkEnum {
namespace Vk {

std::vector<VkExtensionProperties> * getExtensions();
std::vector<VkLayerProperties> * getLayers();
std::vector<VkPhysicalDevice> * getDevices(const VkInstance& instance);
std::vector<VkQueueFamilyProperties>* getQueueFamilies(const VkPhysicalDevice& dev);

}  // namespace Vk
}  // namespace VkEnum
}  // namespace language

#endif // ifdef VK_NULL_HANDLE
