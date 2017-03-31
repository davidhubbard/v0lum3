/* Copyright (c) David Hubbard 2017. Licensed under the GPLv3.
 */
#include "memory.h"
#include <lib/science/science.h>

using namespace science;

namespace memory {

int Image::makeTransitionAccessMasks(VkImageMemoryBarrier& imageB) {
	bool isUnknown = true;
	switch (imageB.oldLayout) {
	case VK_IMAGE_LAYOUT_GENERAL:
		imageB.srcAccessMask =
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_SHADER_READ_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
			VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		imageB.srcAccessMask =
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
	case VK_IMAGE_LAYOUT_UNDEFINED:
		imageB.srcAccessMask = 0;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		imageB.srcAccessMask =
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
		imageB.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		imageB.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		imageB.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		imageB.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		imageB.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_RANGE_SIZE:
	case VK_IMAGE_LAYOUT_MAX_ENUM:
		break;
	}

	switch (imageB.newLayout) {
	case VK_IMAGE_LAYOUT_GENERAL:
		imageB.dstAccessMask =
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_SHADER_READ_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
			VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
			VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
	{
		// Reset imageB.subresourceRange, then set it to Depth.
		SubresUpdate& u = Subres(imageB.subresourceRange).addDepth();
		// Also add Stencil if requested.
		if (hasStencil(info.format)) {
			u.addStencil();
		}

		imageB.dstAccessMask =
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		isUnknown = false;
		break;
	}
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
	{
		// Reset imageB.subresourceRange, then set it to Depth.
		SubresUpdate& u = Subres(imageB.subresourceRange).addDepth();
		// Also add Stencil if requested.
		if (hasStencil(info.format)) {
			u.addStencil();
		}
		imageB.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		isUnknown = false;
		break;
	}
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		imageB.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		imageB.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		imageB.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		imageB.srcAccessMask = VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
		fprintf(stderr, "WARNING: This is probably not what you intended.\n"
			"WARNING: Use VkAttachmentDescription and VkQueuePresent instead.\n");
		imageB.srcAccessMask =
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		isUnknown = false;
		break;

	case VK_IMAGE_LAYOUT_UNDEFINED:
	case VK_IMAGE_LAYOUT_RANGE_SIZE:
	case VK_IMAGE_LAYOUT_MAX_ENUM:
		break;
	}

	if (isUnknown) {
		fprintf(stderr, "makeTransition(): unsupported: %s to %s\n",
			string_VkImageLayout(imageB.oldLayout),
			string_VkImageLayout(imageB.newLayout));
		return 1;
	}
	return 0;
}

VkImageMemoryBarrier Image::makeTransition(VkImageLayout newLayout)
{
		VkImageMemoryBarrier VkInit(imageB);
		imageB.oldLayout = currentLayout;
		imageB.newLayout = newLayout;
		imageB.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageB.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		Subres(imageB.subresourceRange).addColor();

		if (makeTransitionAccessMasks(imageB)) {
			imageB.image = VK_NULL_HANDLE;
			return imageB;
		}

		imageB.image = vk;
		return imageB;
}

}  // namespace memory
