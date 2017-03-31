/* Copyright (c) David Hubbard 2017. Licensed under the GPLv3.
 */
#include "memory.h"
#include <lib/science/science.h>

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
