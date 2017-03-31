/* Copyright (c) David Hubbard 2017. Licensed under the GPLv3.
 */
#include "memory.h"
#include <lib/science/science.h>

using namespace science;

namespace memory {

int DeviceMemory::alloc(MemoryRequirements req, VkMemoryPropertyFlags props)
{
	// This relies on the fact that req.ofProps() updates req.vkalloc.
	if (!req.ofProps(props)) {
		fprintf(stderr, "DeviceMemory::alloc: indexOf returned not found\n");
		return 1;
	}
	vk.reset();
	VkResult v = vkAllocateMemory(req.dev.dev, &req.vkalloc,
		req.dev.dev.allocator, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkAllocateMemory failed: %d (%s)\n", v, string_VkResult(v));
		return 1;
	}
	return 0;
}

int DeviceMemory::mmap(
		language::Device& dev,
		void ** pData,
		VkDeviceSize offset /*= 0*/,
		VkDeviceSize size /*= VK_WHOLE_SIZE*/,
		VkMemoryMapFlags flags /*= 0*/)
{
	VkResult v = vkMapMemory(dev.dev, vk, offset, size, flags, pData);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkMapMemory failed: %d (%s)\n", v, string_VkResult(v));
		return 1;
	}
	return 0;
}

void DeviceMemory::munmap(language::Device& dev)
{
	vkUnmapMemory(dev.dev, vk);
}


int Image::ctorError(language::Device& dev, VkMemoryPropertyFlags props)
{
	if (!info.extent.width || !info.extent.height || !info.extent.depth ||
			!info.format || !info.usage) {
		fprintf(stderr, "Image::ctorError found uninitialized fields\n");
		return 1;
	}

	vk.reset();
	VkResult v = vkCreateImage(dev.dev, &info, dev.dev.allocator, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateImage failed: %d (%s)\n", v, string_VkResult(v));
		return 1;
	}
	currentLayout = info.initialLayout;

	return mem.alloc({dev, vk}, props);
}

int Image::bindMemory(language::Device& dev, VkDeviceSize offset /*= 0*/)
{
	VkResult v = vkBindImageMemory(dev.dev, vk, mem.vk, offset);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkBindImageMemory failed: %d (%s)\n", v, string_VkResult(v));
		return 1;
	}
	return 0;
}

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


int Buffer::ctorError(language::Device& dev, VkMemoryPropertyFlags props)
{
	if (!info.size || !info.usage) {
		fprintf(stderr, "Buffer::ctorError found uninitialized fields\n");
		return 1;
	}

	vk.reset();
	VkResult v = vkCreateBuffer(dev.dev, &info, dev.dev.allocator, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateBuffer failed: %d (%s)\n", v, string_VkResult(v));
		return 1;
	}

	return mem.alloc({dev, vk}, props);
}

int Buffer::bindMemory(language::Device& dev, VkDeviceSize offset /*= 0*/)
{
	VkResult v = vkBindBufferMemory(dev.dev, vk, mem.vk, offset);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkBindBufferMemory failed: %d (%s)\n", v, string_VkResult(v));
		return 1;
	}
	return 0;
}


MemoryRequirements::MemoryRequirements(language::Device& dev, VkImage img)
	: dev(dev)
{
	VkOverwrite(vkalloc);
	vkGetImageMemoryRequirements(dev.dev, img, &vk);
}

MemoryRequirements::MemoryRequirements(language::Device& dev, Image& img)
	: dev(dev)
{
	VkOverwrite(vkalloc);
	vkGetImageMemoryRequirements(dev.dev, img.vk, &vk);
}

MemoryRequirements::MemoryRequirements(language::Device& dev, VkBuffer buf)
	: dev(dev)
{
	VkOverwrite(vkalloc);
	vkGetBufferMemoryRequirements(dev.dev, buf, &vk);
}

MemoryRequirements::MemoryRequirements(language::Device& dev, Buffer& buf)
	: dev(dev)
{
	VkOverwrite(vkalloc);
	vkGetBufferMemoryRequirements(dev.dev, buf.vk, &vk);
}

int MemoryRequirements::indexOf(VkMemoryPropertyFlags props) const
{
	for (uint32_t i = 0; i < dev.memProps.memoryTypeCount; i++) {
		if ((vk.memoryTypeBits & (1 << i)) &&
				(dev.memProps.memoryTypes[i].propertyFlags & props) == props) {
			return i;
		}
	}

	fprintf(stderr, "MemoryRequirements::indexOf(%x): not found in %x\n",
		props, vk.memoryTypeBits);
	return -1;
}

VkMemoryAllocateInfo * MemoryRequirements::ofProps(VkMemoryPropertyFlags props)
{
	int i = indexOf(props);
	if (i == -1) {
		return nullptr;
	}

	vkalloc.memoryTypeIndex = i;
	vkalloc.allocationSize = vk.size;

	return &vkalloc;
}

}  // namespace memory
