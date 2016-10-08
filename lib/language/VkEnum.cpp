/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include <stdio.h>
#include <vector>
#include <vulkan/vulkan.h>
#include "VkPtr.h"
#include "VkEnum.h"
#include "vk_enum_string_helper.h"

namespace language {
namespace VkEnum {
namespace Vk {

std::vector<VkExtensionProperties>* getExtensions() {
	uint32_t extensionCount = 0;
	VkResult r = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkEnumerateInstanceExtensionProperties(count) returned %d (%s)\n", r, string_VkResult(r));
		return nullptr;
	}
	auto* extensions = new std::vector<VkExtensionProperties>(extensionCount);
	r = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions->data());
	if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
		fprintf(stderr, "vkEnumerateInstanceExtensionProperties(all) returned %d (%s)\n", r, string_VkResult(r));
		delete extensions;
		return nullptr;
	}
	if (extensionCount > extensions->size()) {
		// This can happen if an extension was added between the two Enumerate calls.
		fprintf(stderr, "vkEnumerateInstanceExtensionProperties(all) returned count=%u, larger than previously (%zu)\n",
			extensionCount, extensions->size());
		delete extensions;
		return nullptr;
	}
	return extensions;
}

std::vector<VkLayerProperties>* getLayers() {
	uint32_t layerCount = 0;
	VkResult r = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkEnumerateInstanceLayerProperties(count) returned %d (%s)\n", r, string_VkResult(r));
		return nullptr;
	}
	auto* layers = new std::vector<VkLayerProperties>(layerCount);
	r = vkEnumerateInstanceLayerProperties(&layerCount, layers->data());
	if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
		fprintf(stderr, "vkEnumerateInstanceLayerProperties(all) returned %d (%s)\n", r, string_VkResult(r));
		delete layers;
		return nullptr;
	}
	if (layerCount > layers->size()) {
		// This can happen if a layer was added between the two Enumerate calls.
		fprintf(stderr, "vkEnumerateInstanceLayerProperties(all) returned count=%u, larger than previously (%zu)\n",
			layerCount, layers->size());
		delete layers;
		return nullptr;
	}
	return layers;
}

std::vector<VkPhysicalDevice>* getDevices(VkInstance instance) {
	uint32_t devCount = 0;
	VkResult r = vkEnumeratePhysicalDevices(instance, &devCount, nullptr);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkEnumeratePhysicalDevices(count) returned %d (%s)\n", r, string_VkResult(r));
		return nullptr;
	}
	auto* devs = new std::vector<VkPhysicalDevice>(devCount);
	r = vkEnumeratePhysicalDevices(instance, &devCount, devs->data());
	if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
		fprintf(stderr, "vkEnumeratePhysicalDevices(all) returned %d (%s)\n", r, string_VkResult(r));
		delete devs;
		return nullptr;
	}
	if (devCount > devs->size()) {
		// This can happen if a device was added between the two Enumerate calls.
		fprintf(stderr, "vkEnumeratePhysicalDevices(all) returned count=%u, larger than previously (%zu)\n",
			devCount, devs->size());
		delete devs;
		return nullptr;
	}
	return devs;
}

std::vector<VkQueueFamilyProperties>* getQueueFamilies(VkPhysicalDevice dev) {
	uint32_t qCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
	auto* qs = new std::vector<VkQueueFamilyProperties>(qCount);
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, qs->data());
	if (qCount > qs->size()) {
		// This can happen if a queue family was added between the two Enumerate calls.
		fprintf(stderr, "vkGetPhysicalDeviceQueueFamilyProperties(all) returned count=%u, larger than previously (%zu)\n",
			qCount, qs->size());
		delete qs;
		return nullptr;
	}

	for (auto i = qs->begin(); i != qs->end(); i++) {
		if (i->queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
			// Per the vulkan spec for VkQueueFlagBits, GRAPHICS or COMPUTE always imply TRANSFER
			// and TRANSFER does not even have to be reported in that case.
			//
			// In order to make life simple, set TRANSFER if TRANSFER is supported.
			i->queueFlags |= VK_QUEUE_TRANSFER_BIT;
		}
	}
	return qs;
}

std::vector<VkExtensionProperties> * getDeviceExtensions(VkPhysicalDevice dev) {
	uint32_t extensionCount = 0;
	VkResult r = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, nullptr);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkEnumerateDeviceExtensionProperties(count) returned %d (%s)\n", r, string_VkResult(r));
		return nullptr;
	}
	auto* extensions = new std::vector<VkExtensionProperties>(extensionCount);
	r = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, extensions->data());
	if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
		fprintf(stderr, "vkEnumerateDeviceExtensionProperties(all) returned %d (%s)\n", r, string_VkResult(r));
		delete extensions;
		return nullptr;
	}
	if (extensionCount > extensions->size()) {
		// This can happen if an extension was added between the two Enumerate calls.
		fprintf(stderr, "vkEnumerateDeviceExtensionProperties(all) returned count=%u, larger than previously (%zu)\n",
			extensionCount, extensions->size());
		delete extensions;
		return nullptr;
	}
	return extensions;
}

std::vector<VkSurfaceFormatKHR> * getSurfaceFormats(VkPhysicalDevice dev,
		VkSurfaceKHR surface) {
	uint32_t formatCount = 0;
	VkResult r = vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, nullptr);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkGetPhysicalDeviceSurfaceFormatsKHR(count) returned %d\n", r);
		return nullptr;
	}
	auto* formats = new std::vector<VkSurfaceFormatKHR>(formatCount);
	r = vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, formats->data());
	if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
		fprintf(stderr, "vkGetPhysicalDeviceSurfaceFormatsKHR(all) returned %d\n", r);
		delete formats;
		return nullptr;
	}
	if (formatCount > formats->size()) {
		// This can happen if a format was added between the two Get calls.
		fprintf(stderr, "vkGetPhysicalDeviceSurfaceFormatsKHR(all) returned count=%u, larger than previously (%zu)\n",
			formatCount, formats->size());
		delete formats;
		return nullptr;
	}
	return formats;
}

std::vector<VkPresentModeKHR> * getPresentModes(VkPhysicalDevice dev,
		VkSurfaceKHR surface) {
	uint32_t modeCount = 0;
	VkResult r = vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &modeCount, nullptr);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkGetPhysicalDeviceSurfacePresentModesKHR(count) returned %d (%s)\n", r, string_VkResult(r));
		return nullptr;
	}
	auto* modes = new std::vector<VkPresentModeKHR>(modeCount);
	r = vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &modeCount, modes->data());
	if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
		fprintf(stderr, "vkGetPhysicalDeviceSurfacePresentModesKHR(all) returned %d (%s)\n", r, string_VkResult(r));
		delete modes;
		return nullptr;
	}
	if (modeCount > modes->size()) {
		// This can happen if a mode was added between the two Get calls.
		fprintf(stderr, "vkGetPhysicalDeviceSurfacePresentModesKHR(all) returned count=%u, larger than previously (%zu)\n",
			modeCount, modes->size());
		delete modes;
		return nullptr;
	}
	return modes;
}

std::vector<VkImage> * getSwapchainImages(VkDevice dev,
		VkSwapchainKHR swapchain) {
	uint32_t imageCount = 0;
	VkResult r = vkGetSwapchainImagesKHR(dev, swapchain, &imageCount, nullptr);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkGetSwapchainImagesKHR(count) returned %d (%s)\n", r, string_VkResult(r));
		return nullptr;
	}
	auto* images = new std::vector<VkImage>(imageCount);
	r = vkGetSwapchainImagesKHR(dev, swapchain, &imageCount, images->data());
	if (r != VK_SUCCESS && r != VK_INCOMPLETE) {
		fprintf(stderr, "vkGetSwapchainImagesKHR(all) returned %d (%s)\n", r, string_VkResult(r));
		delete images;
		return nullptr;
	}
	if (imageCount > images->size()) {
		// This can happen if an image was added between the two Get calls.
		fprintf(stderr, "vkGetSwapchainImagesKHR(all) returned count=%u, larger than previously (%zu)\n",
			imageCount, images->size());
		delete images;
		return nullptr;
	}
	return images;
}

}  // namespace Vk
}  // namespace VkEnum
}  // namespace language
