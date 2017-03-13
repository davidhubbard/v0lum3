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
		dev.format = {VK_FORMAT_B8G8R8A8_UNORM, dev.surfaceFormats[0].colorSpace};
		return 0;
	}

	if (0) {
		// Hmm... this is too picky. Just go with the first option returned.
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

	dev.format = dev.surfaceFormats.at(0);
	return 0;
}

int initPresentMode(Device& dev) {
	if (dev.presentModes.size() == 0) {
		fprintf(stderr, "BUG: should not init a device with 0 PresentModes\n");
		return 1;
	}

	bool haveMailbox = false;
	bool haveImmediate = false;
	bool haveFIFO = false;
	bool haveFIFOrelaxed = false;
	for (const auto& availableMode : dev.presentModes) {
		switch (availableMode) {
		case VK_PRESENT_MODE_MAILBOX_KHR:
			haveMailbox = true;
			break;
		case VK_PRESENT_MODE_IMMEDIATE_KHR:
			haveImmediate = true;
			break;
		case VK_PRESENT_MODE_FIFO_KHR:
			haveFIFO = true;
			break;
		case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
			haveFIFOrelaxed = true;
			break;
		case VK_PRESENT_MODE_RANGE_SIZE_KHR:
		case VK_PRESENT_MODE_MAX_ENUM_KHR:
			fprintf(stderr, "BUG: invalid presentMode 0x%x\n",
				availableMode);
			return 1;
		}
	}

	if (!haveFIFO) {
		// VK_PRESENT_MODE_FIFO_KHR is required to always be present by the spec.
		// TODO: Is this validated by Vulkan validation layers?
		fprintf(stderr, "Warn: initPresentMode() did not find "
			"VK_PRESENT_MODE_FIFO_KHR.\n"
			"      This is an unexpected surprise! Could you send us\n"
			"      what vendor/VulkamSamples/build/demo/vulkaninfo\n"
			"      outputs -- we would love a bug report at:\n"
			"      https://github.com/davidhubbard/v0lum3/issues/new\n");
		return 1;
	}

	// TODO: Add a way to prefer IMMEDIATE instead (perhaps the user wants
	// to ensure the swapChain is as small as possible).
	if (haveMailbox) {
		dev.freerunMode = VK_PRESENT_MODE_MAILBOX_KHR;
	} else if (haveImmediate) {
		dev.freerunMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	} else if (haveFIFOrelaxed) {
		dev.freerunMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	} else if (haveFIFO) {
		dev.freerunMode = VK_PRESENT_MODE_FIFO_KHR;
	} else {
		fprintf(stderr, "BUG: should always haveFIFO for freerunMode");
		exit(1);
	}

	// TODO: Add a way to prefer FIFO_RELAXED instead.
	if (haveFIFO) {
		dev.vsyncMode = VK_PRESENT_MODE_FIFO_KHR;
	} else {
		fprintf(stderr, "BUG: should always haveFIFO for vsyncMode");
		exit(1);
	}
	return 0;
}

uint32_t calculateMinRequestedImages(const VkSurfaceCapabilitiesKHR& scap) {
	// An optimal number of images is one more than the minimum. For example:
	// double buffering minImageCount = 1. imageCount = 2.
	// triple buffering minImageCount = 2. imageCount = 3.
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
	// If currentExtent != { UINT32_MAX, UINT32_MAX } then Vulkan is telling us:
	// "this is the right extent: you already created a surface and Vulkan
	// computed the right size to match it."
	if (scap.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return scap.currentExtent;
	}

	// Vulkan is telling us "choose width, height from scap.minImageExtent
	// to scap.maxImageExtent." Attempt to satisfy surfaceSizeRequest.
	const VkExtent2D& lo = scap.minImageExtent, hi = scap.maxImageExtent;
	return {
		/*width:*/ std::max(lo.width, std::min(hi.width, surfaceSizeRequest.width)),
		/*height:*/ std::max(lo.height, std::min(hi.height, surfaceSizeRequest.height)),
	};
}

VkSurfaceTransformFlagBitsKHR calculateSurfaceTransform(
		const VkSurfaceCapabilitiesKHR& scap) {
	// Prefer no rotation (VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR).
	if (scap.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
		return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	return scap.currentTransform;
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

int Instance::createSwapChain(size_t dev_i, VkExtent2D sizeRequest) {
	Device& dev = devs.at(dev_i);
	VkSwapchainKHR oldSwapChain = dev.swapChain;

	VkSurfaceCapabilitiesKHR scap;
	VkResult v = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev.phys, surface, &scap);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR() returned %d (%s)\n", v, string_VkResult(v));
		return 1;
	}

	dev.swapChainExtent = calculateSurfaceExtend2D(scap, sizeRequest);

	VkSwapchainCreateInfoKHR VkInit(scci);
	scci.surface = surface;
	scci.minImageCount = calculateMinRequestedImages(scap);
	scci.imageFormat = dev.format.format;
	scci.imageColorSpace = dev.format.colorSpace;
	scci.imageExtent = dev.swapChainExtent;
	scci.imageArrayLayers = 1;  // e.g. 2 is for stereo displays.
	scci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	scci.preTransform = calculateSurfaceTransform(scap);
	scci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	// TODO: vsyncMode support.
	scci.presentMode = dev.freerunMode;
	scci.clipped = VK_TRUE;
	scci.oldSwapchain = oldSwapChain;
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
		fprintf(stderr, "CONCURRENT not tested\n");
		exit(1);
		// Device queues were set up such that a different QueueFamily does PRESENT
		// and a different QueueFamily does GRAPHICS.
		scci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		scci.queueFamilyIndexCount = 2;
		scci.pQueueFamilyIndices = qfamIndices;
	}
	v = vkCreateSwapchainKHR(dev.dev, &scci, pAllocator, &dev.swapChain);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateSwapchainKHR() returned %d\n", v);
		return 1;
	}
	dev.swapChain.allocator = pAllocator;
	if (oldSwapChain) {
		fprintf(stderr, "TODO: clean up oldswapChain\n");
		exit(1);
	}

	auto* vkImages = Vk::getSwapchainImages(dev.dev, dev.swapChain);
	if (!vkImages) {
		return 1;
	}

	for (size_t i = 0; i < vkImages->size(); i++) {
		dev.framebufs.emplace_back(dev);
		auto& framebuf = *(dev.framebufs.end() - 1);
		framebuf.image = vkImages->at(i);

		VkImageViewCreateInfo VkInit(ivci);
		ivci.image = framebuf.image;
		ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ivci.format = dev.format.format;
		ivci.components = {
				VK_COMPONENT_SWIZZLE_R,
				VK_COMPONENT_SWIZZLE_G,
				VK_COMPONENT_SWIZZLE_B,
				VK_COMPONENT_SWIZZLE_A,
			};
		ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ivci.subresourceRange.baseMipLevel = 0;  // No mipmapping.
		ivci.subresourceRange.levelCount = 1;
		ivci.subresourceRange.baseArrayLayer = 0;
		ivci.subresourceRange.layerCount = 1;  // Might be 2 for stereo displays.
		v = vkCreateImageView(dev.dev, &ivci, pAllocator, &framebuf.imageView);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "vkCreateImageView[%zu] returned %d\n", i, v);
			delete vkImages;
			return 1;
		}
	}
	delete vkImages;
	return 0;
}

// Framebuf::Framebuf() defined here to have full Device definition.
Framebuf::Framebuf(Device& dev)
		: imageView(dev.dev, vkDestroyImageView)
		, vk(dev.dev, vkDestroyFramebuffer) {
	imageView.allocator = dev.dev.allocator;
	vk.allocator = dev.dev.allocator;
};

}  // namespace language
