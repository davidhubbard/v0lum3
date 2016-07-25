/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include <vulkan/vulkan.h>

#pragma once

namespace language {
namespace VkEnum {
namespace Vk {

std::vector<VkExtensionProperties> * getExtensions();
std::vector<VkLayerProperties> * getLayers();
std::vector<VkPhysicalDevice> * getDevices(VkInstance instance);

std::vector<VkQueueFamilyProperties>* getQueueFamilies(VkPhysicalDevice dev);
std::vector<VkExtensionProperties> * getDeviceExtensions(VkPhysicalDevice dev);

std::vector<VkSurfaceFormatKHR> * getSurfaceFormats(VkPhysicalDevice dev,
		VkSurfaceKHR surface);
std::vector<VkPresentModeKHR> * getPresentModes(VkPhysicalDevice dev,
		VkSurfaceKHR surface);
std::vector<VkImage> * getSwapchainImages(VkDevice dev,
		VkSwapchainKHR swapchain);

}  // namespace Vk
}  // namespace VkEnum
}  // namespace language
