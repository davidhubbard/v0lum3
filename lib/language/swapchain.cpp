/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include "language.h"
#include "VkEnum.h"
#include "VkInit.h"

#include <algorithm>

namespace language {
using namespace VkEnum;

namespace {  // use an anonymous namespace to hide all its contents (only reachable from this file)

int initSurfaceFormat(Device& dev) {
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

int initPresentMode(Device& dev) {
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
		/*width:*/ std::max(lo.width, std::min(hi.width, surfaceSizeRequest.width)),
		/*height:*/ std::max(lo.height, std::min(hi.height, surfaceSizeRequest.height)),
	};
}

}  // anonymous namespace

int Instance::initSurfaceFormatAndPresentMode(Device& dev) {
	auto* surfaceFormats = Vk::getSurfaceFormats(dev.phys, this->surface);
	if (!surfaceFormats) {
		return 1;
	}
	dev.surfaceFormats = *surfaceFormats;
	delete surfaceFormats;
	surfaceFormats = nullptr;

	auto* presentModes = Vk::getPresentModes(dev.phys, this->surface);
	if (!presentModes) {
		return 1;
	}
	dev.presentModes = *presentModes;
	delete presentModes;
	presentModes = nullptr;

	if (dev.surfaceFormats.size() == 0 || dev.presentModes.size() == 0) {
		return 0;
	}

	int r = initSurfaceFormat(dev);
	if (r) {
		return r;
	}
	if ((r = initPresentMode(dev)) != 0) {
		return r;
	}
	return 0;
}

int Instance::createSwapchain(size_t dev_i, VkExtent2D surfaceSizeRequest) {
	Device& dev = devs.at(dev_i);

	VkSurfaceCapabilitiesKHR scap;
	VkResult v = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev.phys, surface, &scap);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR() returned %d (%s)\n", v, string_VkResult(v));
		return 1;
	}

	dev.swapchainExtent = calculateSurfaceExtend2D(scap, surfaceSizeRequest);
	VkSwapchainCreateInfoKHR VkInit(scci);
	scci.surface = surface;
	scci.minImageCount = calculateMinRequestedImages(scap);
	scci.imageFormat = dev.format.format;
	scci.imageColorSpace = dev.format.colorSpace;
	scci.imageExtent = dev.swapchainExtent;
	scci.imageArrayLayers = 1;  // e.g. 2 is for stereo displays.
	scci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	scci.preTransform = scap.currentTransform;
	scci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	scci.presentMode = dev.mode;
	scci.clipped = VK_TRUE;
	scci.oldSwapchain = VK_NULL_HANDLE;
	uint32_t qfamIndices[] = {
		(uint32_t) dev.getQfamI(PRESENT),
		(uint32_t) dev.getQfamI(GRAPHICS),
	};
	if (qfamIndices[0] == qfamIndices[1]) {
		// Device queues were set up such that one QueueFamily does both
		// PRESENT and GRAPHICS.
		scci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		scci.queueFamilyIndexCount = 0;
		scci.pQueueFamilyIndices = nullptr;
	} else {
		// Device queues were set up such that a different QueueFamily does PRESENT
		// and a different QueueFamily does GRAPHICS.
		scci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		scci.queueFamilyIndexCount = 2;
		scci.pQueueFamilyIndices = qfamIndices;
	}
	v = vkCreateSwapchainKHR(dev.dev, &scci, nullptr /*allocator*/, &dev.swapchain);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateSwapchainKHR() returned %d\n", v);
		return 1;
	}

	auto* vkImages = Vk::getSwapchainImages(dev.dev, dev.swapchain);
	if (!vkImages) {
		return 1;
	}
	dev.images = *vkImages;
	delete vkImages;

	for (size_t i = 0; i < dev.images.size(); i++) {
		dev.framebufs.emplace_back(dev);
		auto& framebuf = *(dev.framebufs.end() - 1);

		VkImageViewCreateInfo VkInit(ivci);
		ivci.image = dev.images.at(i);
		ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ivci.format = dev.format.format;
		ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ivci.subresourceRange.baseMipLevel = 0;  // No mipmapping.
		ivci.subresourceRange.levelCount = 1;
		ivci.subresourceRange.baseArrayLayer = 0;
		ivci.subresourceRange.layerCount = 1;  // Might be 2 for stereo displays.
		v = vkCreateImageView(dev.dev, &ivci, nullptr, &framebuf.imageView);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "vkCreateImageView[%zu] returned %d\n", i, v);
			return 1;
		}
	}
	return 0;
}

// Framebuf::Framebuf() defined here to have full Device definition.
Framebuf::Framebuf(Device& dev)
	: imageView(dev.dev, vkDestroyImageView)
	, vk(dev.dev, vkDestroyFramebuffer) {};

}  // namespace language
