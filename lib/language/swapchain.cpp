/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include "language.h"
#include "VkEnum.h"

namespace language {
using namespace VkEnum;

int Instance::initSurfaceFormat(Device& dev) {
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

int Instance::initPresentMode(Device& dev) {
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
