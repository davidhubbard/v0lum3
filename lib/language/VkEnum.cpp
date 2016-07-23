/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include <stdio.h>
#include <vector>
#include <vulkan/vulkan.h>
#include "VkPtr.h"
#include "VkEnum.h"

namespace language {
namespace VkEnum {
namespace Vk {

std::vector<VkExtensionProperties>* getExtensions() {
	uint32_t extensionCount = 0;
	VkResult r = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkEnumerateInstanceExtensionProperties(count) returned %d", r);
		return nullptr;
	}
	auto* extensions = new std::vector<VkExtensionProperties>(extensionCount);
	r = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions->data());
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkEnumerateInstanceExtensionProperties(all) returned %d", r);
		delete extensions;
		return nullptr;
	}
	if (extensionCount > extensions->size()) {
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
		fprintf(stderr, "vkEnumerateInstanceLayerProperties(count) returned %d", r);
		return nullptr;
	}
	auto* layers = new std::vector<VkLayerProperties>(layerCount);
	r = vkEnumerateInstanceLayerProperties(&layerCount, layers->data());
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkEnumerateInstanceLayerProperties(all) returned %d", r);
		delete layers;
		return nullptr;
	}
	if (layerCount > layers->size()) {
		fprintf(stderr, "vkEnumerateInstanceLayerProperties(all) returned count=%u, larger than previously (%zu)\n",
			layerCount, layers->size());
		delete layers;
		return nullptr;
	}
	return layers;
}

std::vector<VkPhysicalDevice>* getDevices(const VkInstance& instance) {
	uint32_t devCount = 0;
	VkResult r = vkEnumeratePhysicalDevices(instance, &devCount, nullptr);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkEnumeratePhysicalDevices(count) returned %d", r);
		return nullptr;
	}
	auto* devs = new std::vector<VkPhysicalDevice>(devCount);
	r = vkEnumeratePhysicalDevices(instance, &devCount, devs->data());
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkEnumeratePhysicalDevices(all) returned %d", r);
		delete devs;
		return nullptr;
	}
	if (devCount > devs->size()) {
		fprintf(stderr, "vkEnumeratePhysicalDevices(all) returned count=%u, larger than previously (%zu)\n",
			devCount, devs->size());
		delete devs;
		return nullptr;
	}
	return devs;
}

std::vector<VkQueueFamilyProperties>* getQueueFamilies(const VkPhysicalDevice& dev) {
	uint32_t qCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, nullptr);
	auto* qs = new std::vector<VkQueueFamilyProperties>(qCount);
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &qCount, qs->data());
	if (qCount > qs->size()) {
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

std::vector<VkExtensionProperties> * getDeviceExtensions(const VkPhysicalDevice& dev) {
	uint32_t extensionCount = 0;
	VkResult r = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, nullptr);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkEnumerateDeviceExtensionProperties(count) returned %d", r);
		return nullptr;
	}
	auto* extensions = new std::vector<VkExtensionProperties>(extensionCount);
	r = vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, extensions->data());
	if (r != VK_SUCCESS) {
		fprintf(stderr, "vkEnumerateDeviceExtensionProperties(all) returned %d", r);
		delete extensions;
		return nullptr;
	}
	if (extensionCount > extensions->size()) {
		fprintf(stderr, "vkEnumerateDeviceExtensionProperties(all) returned count=%u, larger than previously (%zu)\n",
			extensionCount, extensions->size());
		delete extensions;
		return nullptr;
	}
	return extensions;
}

}  // namespace Vk
}  // namespace VkEnum
}  // namespace language
