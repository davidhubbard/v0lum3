/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include "language.h"
#include "VkEnum.h"
#include "VkInit.h"

#include <algorithm>

namespace language {
using namespace VkEnum;

namespace {  // use an anonymous namespace to hide all its contents (only reachable from this file)
const uint32_t WIN_W = 800;
const uint32_t WIN_H = 600;

uint32_t calculateMinRequestedImages(const VkSurfaceCapabilitiesKHR& scap) {
	// An optimal number of images is one more than the minimum. For example:
	// double buffering minImageCount = 1.
	// triple buffering minImageCount = 2.
	uint32_t imageCount = scap.minImageCount + 1;

	// maxImageCount = 0 means "there is no maximum except device memory limits".
	if (scap.maxImageCount > 0 && imageCount > scap.maxImageCount) {
		imageCount = scap.maxImageCount;
	}

	// Note: The GPU driver can create more than the number returned here.
	// Device::images.size() gives the actual number created by the GPU driver.
	return imageCount;
}

VkExtent2D calculateSurfaceExtend2D(const VkSurfaceCapabilitiesKHR& scap,
		VkExtent2D surfaceSizeRequest) {
	// If currentExtent == { UINT32_MAX, UINT32_MAX } then Vulkan is telling us:
	// "choose width, height from scap.minImageExtent to scap.maxImageExtent."
	if (scap.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		// Else Vulkan is telling us, "this is the right extent: you already
		// created a surface and Vulkan computed the right size to match it."
		return scap.currentExtent;
	}

	const VkExtent2D& lo = scap.minImageExtent, hi = scap.maxImageExtent;
	return {
		width: std::max(lo.width, std::min(hi.width, surfaceSizeRequest.width)),
		height: std::max(lo.height, std::min(hi.height, surfaceSizeRequest.height)),
	};
}

static int initImageSharingMode(Device& dev, VkSwapchainCreateInfoKHR& scci) {
	(void) dev;

	// If there is one QueueFamily that supports both PRESENT and GRAPHICS:
	//   scci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE
	// Else 2 x QueueFamily setup can use EXCLUSIVE or CONCURRENT...
	//   scci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	//   scci.queueFamilyIndexCount = 2;
	//   scci.pQueueFamilyIndices = uint32_t[]{ GRAPHICS_i, PRESENT_i };
	printf("TODO: either set up CONCURRENT or figure out EXCLUSIVE\n");

	scci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	return 0;
}

}  // anonymous namespace

int Device::createSwapChain(Instance& inst, const VkSurfaceKHR& surface,
		VkExtent2D surfaceSizeRequest) {
	(void) inst;

	VkSurfaceCapabilitiesKHR scap;
	VkResult v = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &scap);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR() returned %d\n", v);
		return 1;
	}

	VkSwapchainCreateInfoKHR VkInit(scci);
	scci.surface = surface;
	scci.minImageCount = calculateMinRequestedImages(scap);
	scci.imageFormat = format.format;
	scci.imageColorSpace = format.colorSpace;
	scci.imageExtent = calculateSurfaceExtend2D(scap, surfaceSizeRequest);
	scci.imageArrayLayers = 1;  // e.g. 2 is for stereo displays.
	scci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	scci.preTransform = scap.currentTransform;
	scci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	scci.presentMode = mode;
	scci.clipped = VK_TRUE;
	scci.oldSwapchain = VK_NULL_HANDLE;
	if (initImageSharingMode(*this, scci)) {
		return 1;
	}
	v = vkCreateSwapchainKHR(dev, &scci, nullptr /*allocator*/, &swapchain);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateSwapchainKHR() returned %d\n", v);
		return 1;
	}

	auto* vkImages = Vk::getSwapchainImages(dev, swapchain);
	if (!vkImages) {
		return 1;
	}
	images = *vkImages;
	delete vkImages;
	return 0;
}

int Instance::initSurfaceFormat(Device& dev, const VkSurfaceKHR& surface) {
	(void) surface;

	if (dev.surfaceFormats.size() == 0) {
		fprintf(stderr, "BUG: should not init a device with 0 SurfaceFormats\n");
		return 1;
	}

	if (dev.surfaceFormats.size() == 1 &&
			dev.surfaceFormats[0].format == VK_FORMAT_UNDEFINED) {
		// Vulkan specifies "you get to choose" by returning VK_FORMAT_UNDEFINED.
		// Prefer 32-bit color and hardware SRGB color space.
		dev.format = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
		return 0;
	}

	for (const auto& availableFormat : dev.surfaceFormats) {
		// Prefer 32-bit color and hardware SRGB color space.
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
				availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			dev.format = availableFormat;
			return 0;
		}
	}

	// This combination has not been seen in the real world.
	// Please file a bug if you see this!
	fprintf(stderr, "Warn: initSurfaceFormat() did not find a great format.\n");
	fprintf(stderr, "      This is an unexpected surprise! Could you send us\n");
	fprintf(stderr, "      what vendor/VulkamSamples/build/demo/vulkaninfo\n");
	fprintf(stderr, "      outputs -- we would love a bug report at:\n");
	fprintf(stderr, "      https://github.com/davidhubbard/v0lum3/issues/new\n");
	dev.format = dev.surfaceFormats[0];
	return 0;
}

int Instance::initPresentMode(Device& dev, const VkSurfaceKHR& surface) {
	(void) surface;

	if (dev.presentModes.size() == 0) {
		fprintf(stderr, "BUG: should not init a device with 0 PresentModes\n");
		return 1;
	}

	bool haveFIFO = false;
	for (const auto& availableMode : dev.presentModes) {
		if (availableMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			dev.mode = availableMode;
			return 0;
		}
		if (availableMode == VK_PRESENT_MODE_FIFO_KHR) {
			haveFIFO = true;
		}
	}

	if (haveFIFO) {
		dev.mode = VK_PRESENT_MODE_FIFO_KHR;
		return 0;
	}

	// TODO: Is this validated by Vulkan validation layers?
	fprintf(stderr, "Warn: initPresentMode() did not find MODE_FIFO.\n");
	fprintf(stderr, "      This is an unexpected surprise! Could you send us\n");
	fprintf(stderr, "      what vendor/VulkamSamples/build/demo/vulkaninfo\n");
	fprintf(stderr, "      outputs -- we would love a bug report at:\n");
	fprintf(stderr, "      https://github.com/davidhubbard/v0lum3/issues/new\n");
	return 1;
}

}  // namespace language