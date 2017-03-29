/* Copyright (c) David Hubbard 2017. Licensed under the GPLv3.
 */
#include "language.h"
#include "VkInit.h"

namespace language {

Device::~Device()
{
}

// ImageView::ImageView() defined here to have full Device definition.
ImageView::ImageView(Device& dev) : vk(dev.dev, vkDestroyImageView)
{
	vk.allocator = dev.dev.allocator;
	VkOverwrite(info);
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A,
		};
	// info.subresourceRange could be set up using range1MipAndColor() in
	// <lib/science/science.h> -- but it would be a circular dependency.
	info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	info.subresourceRange.baseMipLevel = 0;  // Mipmap level offset (none).
	info.subresourceRange.levelCount = 1;  // There is 1 mipmap (no mipmapping).
	info.subresourceRange.baseArrayLayer = 0;  // Offset in layerCount layers.
	info.subresourceRange.layerCount = 1;  // Might be 2 for stereo displays.
};

int ImageView::ctorError(Device& dev, VkImage image, VkFormat format)
{
	info.image = image;
	info.format = format;
	vk.reset();
	VkResult v = vkCreateImageView(dev.dev, &info, dev.dev.allocator, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateImageView returned %d (%s)\n", v, string_VkResult(v));
		return 1;
	}
	return 0;
}

// Framebuf::Framebuf() defined here to have full Device definition.
Framebuf::Framebuf(Device& dev)
		: imageView0(dev)
		, vk(dev.dev, vkDestroyFramebuffer) {
	vk.allocator = dev.dev.allocator;
}

int Framebuf::ctorError(
		Device& dev,
		VkRenderPass renderPass,
		VkExtent2D swapChainExtent) {
	if (!attachments.size()) {
		// Better to print this than segfault in the vulkan driver.
		fprintf(stderr, "Framebuf::ctorError with attachments.size == 0\n");
		return 1;
	}
	if (attachments.at(0) != imageView0.vk) {
		// If you hit this error, it means your code removed imageView0.vk from
		// attachments. In that case, please derive a subclass and define your
		// own method to populate VkFramebufferCreateInfo::layers appropriately.
		fprintf(stderr, "BUG: it is probably ok to throw out imageView0, but "
			"this code depends on imageView0.info.subresourceRange.layerCount!\n");
		return 1;
	}
	VkFramebufferCreateInfo VkInit(fbci);
	fbci.renderPass = renderPass;
	fbci.attachmentCount = attachments.size();
	fbci.pAttachments = attachments.data();
	fbci.width = swapChainExtent.width;
	fbci.height = swapChainExtent.height;
	fbci.layers = imageView0.info.subresourceRange.layerCount;

	VkResult v = vkCreateFramebuffer(dev.dev, &fbci, dev.dev.allocator, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateFramebuffer returned %d (%s)\n", v, string_VkResult(v));
		return 1;
	}
	return 0;
}

}  // namespace language
