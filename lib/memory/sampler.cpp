/* Copyright (c) David Hubbard 2017. Licensed under the GPLv3.
 */
#include "memory.h"
#include <lib/science/science.h>

namespace memory {

int Sampler::ctorError(
		language::Device& dev,
		command::CommandBuilder& builder,
		Image& src)
{
	vk.reset();
	VkResult v = vkCreateSampler(dev.dev, &info, dev.dev.allocator, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateSampler failed: %d (%s)\n", v, string_VkResult(v));
		return 1;
	}

	// Construct image as a USAGE_SAMPLED | TRANSFER_DST, then use
	// CommandBuilder::copyImage() to transfer its contents into it.
	image.info.extent = src.info.extent;
	image.info.format = src.info.format;
	image.info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image.info.usage =
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	if (image.ctorDeviceLocal(dev) || image.bindMemory(dev)) {
		fprintf(stderr, "ctorDeviceLocal or bindMemory failed\n");
		return 1;
	}
	if (imageView.ctorError(dev, image.vk, image.info.format)) {
		fprintf(stderr, "imageView.ctorError failed\n");
		return 1;
	}

	command::CommandBuilder::BarrierSet bsetSrc;
	bsetSrc.img.push_back(
		src.makeTransition(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
	bsetSrc.img.push_back(
		image.makeTransition(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
	if (builder.barrier(bsetSrc,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)) {
		fprintf(stderr, "builder.barrier(src) failed\n");
		return 1;
	}
	src.currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	image.currentLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	VkImageCopy region = {};
	science::Subres(region.srcSubresource).addColor();
	science::Subres(region.dstSubresource).addColor();
	region.srcOffset = {0, 0, 0};
	region.dstOffset = {0, 0, 0};
	region.extent = src.info.extent;
	if (builder.copyImage(src.vk, image.vk, std::vector<VkImageCopy>{region})) {
		fprintf(stderr, "builder.copyImage failed\n");
		return 1;
	}

	command::CommandBuilder::BarrierSet bsetShader;
	bsetShader.img.push_back(
		image.makeTransition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
	if (builder.barrier(bsetShader,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)) {
		fprintf(stderr, "builder.barrier(shader) failed\n");
		return 1;
	}
	image.currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	return 0;
}

}  // namespace memory
