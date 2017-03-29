/* Copyright (c) David Hubbard 2017. Licensed under the GPLv3.
 */
#include "science.h"

namespace science {

int PipeBuilder::addDepthImage(
		language::Device& dev,
		command::RenderPass& pass,
		command::CommandBuilder& builder,
		const std::vector<VkFormat>& formatChoices) {
	if (depthImage.info.format != VK_FORMAT_UNDEFINED || depthImage.vk) {
		// More advanced use cases like dynamic shadowmaps cannot use this
		// method.
		fprintf(stderr, "addDepthImage can only be called once, "
			"for vanilla depth testing.\n");
		return 1;
	}

	// pass should clear the depth buffer along with any color buffers.
	VkClearValue depthClear = {};
	depthClear.depthStencil = {1.0f, 0};
	pass.passBeginClearColors.emplace_back(depthClear);

	// Use pipline.info.depthsci to turn on the fixed-function depthTestEnable.
	pipeline.info.depthsci.depthTestEnable = VK_TRUE;
	pipeline.info.depthsci.depthWriteEnable = VK_TRUE;

	// Fill in depthImage.info for recreateSwapChainExtent.
	// depthImage.info.extent is written by recreateSwapChainExtent.
	depthImage.info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthImage.info.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthImage.info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	depthImage.info.format = dev.chooseFormat(
		depthImage.info.tiling,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
		formatChoices);
	if (depthImage.info.format == VK_FORMAT_UNDEFINED) {
		fprintf(stderr, "PipeBuilder::addDepthImage: "
			"none of formatChoices chosen.\n");
		return 1;
	}

	pipeline.info.attach.emplace_back(dev, depthImage.info.format,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	return recreateSwapChainExtent(dev, builder);
}

int PipeBuilder::recreateSwapChainExtent(
		language::Device& dev, command::CommandBuilder& builder) {

	// if addDepthImage() was called, recreate the depthImage.
	if (depthImage.info.format != VK_FORMAT_UNDEFINED) {
		depthImage.info.extent = { 1, 1, 1 };
		depthImage.info.extent.width = dev.swapChainExtent.width;
		depthImage.info.extent.height = dev.swapChainExtent.height;
		if (depthImage.ctorDeviceLocal(dev) || depthImage.bindMemory(dev)) {
			fprintf(stderr, "PipeBuilder::recreateSwapChainExtent: "
				"depthImage.ctorError failed\n");
			return 1;
		}

		depthImageView.info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (depthImageView.ctorError(dev, depthImage.vk, depthImage.info.format)) {
			fprintf(stderr, "PipeBuilder::recreateSwapChainExtent: "
				"depthImageView.ctorError failed\n");
			return 1;
		}

		command::CommandBuilder::BarrierSet bset;
		bset.img.emplace_back(depthImage.makeTransition(
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));
		depthImage.currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		if (builder.barrier(bset,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)) {
			return 1;
		}

		for (auto& framebuf : dev.framebufs) {
			framebuf.attachments.emplace_back(depthImageView.vk);
		}
	}

	return 0;
}

}  // namespace science
