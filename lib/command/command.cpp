/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

RenderPass::RenderPass(language::Device& dev)
		: vk{dev.dev, vkDestroyRenderPass}
		, passBeginClearColor{0.0f, 0.0f, 0.0f, 1.0f} {
	VkAttachmentDescription VkInit(ad);
	ad.format = dev.format.format;
	ad.samples = VK_SAMPLE_COUNT_1_BIT;
	ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	ad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// This color attachment (the VkImage in the Framebuffer) will be transitioned
	// automatically just before the RenderPass. It will be transitioned from...
	ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// (VK_IMAGE_LAYOUT_UNDEFINED meaning throw away any data in the Framebuffer)
	//
	// Then the RenderPass is performed.
	//
	// Then after the RenderPass ends, automatically transition the Framebuffer
	// VkImage to:
	ad.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// (The above are default values. Customize as needed for your application.)
	colorAttaches.push_back(ad);

	VkOverwrite(rpci);
	VkOverwrite(passBeginInfo);
}

int RenderPass::getSubpassDeps(size_t subpass_i,
		const std::vector<PipelineCreateInfo>& pcis,
		std::vector<VkSubpassDependency>& subpassdeps) {
	// Link this subpass to the previous one.
	VkSubpassDependency VkInit(fromprev);
	fromprev.srcSubpass = (subpass_i == 0) ? VK_SUBPASS_EXTERNAL : (subpass_i - 1);
	fromprev.dstSubpass = subpass_i;
	fromprev.dependencyFlags = 0;
	if (subpass_i == 0) {
		fromprev.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		fromprev.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	} else {
		// See https://www.khronos.org/registry/vulkan/specs/1.0/man/html/VkSubpassDependency.html
		fprintf(stderr, "TODO: getSubpassDep(src) needs to know the VK_PIPELINE_STAGE_...\n");
		return 1;
	}
	if (subpass_i == pcis.size() - 1) {
		fromprev.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		fromprev.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	} else {
		fprintf(stderr, "TODO: getSubpassDep(dst) needs to know the VK_PIPELINE_STAGE_...\n");
		return 1;
	}
	subpassdeps.push_back(fromprev);

#if 0
	// Link this subpass to the next one, as shown in
	// https://github.com/GameTechDev/IntroductionToVulkan/blob/master/Project/Tutorial04/Tutorial04.cpp
	//
	// This is not needed as described here:
	// https://forums.khronos.org/showthread.php/13119-Intel-TuT-4-uses-a-SubpassDependency-but-why?p=40575&viewfull=1#post40575
	VkSubpassDependency VkInit(tonext);
	tonext.srcSubpass = subpass_i;
	tonext.dstSubpass = (subpass_i == pcis.size() - 1) ? VK_SUBPASS_EXTERNAL : subpass_i + 1;
	tonext.dependencyFlags = 0;
	if (subpass_i == pcis.size() - 1) {
		tonext.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		tonext.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	} else {
		// See https://www.khronos.org/registry/vulkan/specs/1.0/man/html/VkSubpassDependency.html
		fprintf(stderr, "TODO: getSubpassDep(src) needs to know the VK_PIPELINE_STAGE_...\n");
		return 1;
	}
	if (subpass_i == pcis.size() - 1) {
		tonext.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		tonext.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	} else {
		fprintf(stderr, "TODO: getSubpassDep(dst) needs to know the VK_PIPELINE_STAGE_...\n");
		return 1;
	}
	subpassdeps.push_back(tonext);
#endif
	return 0;
}

int RenderPass::init(std::vector<PipelineCreateInfo> pcis) {
	if (shaders.empty()) {
		fprintf(stderr, "RenderPass::init(): 0 shaders\n");
		return 1;
	}
	if (pipelines.empty()) {
		fprintf(stderr, "RenderPass::init(): 0 pipelines\n");
		return 1;
	}
	if (pcis.size() != pipelines.size()) {
		fprintf(stderr, "Every Pipeline needs a PipelineCreateInfo.\n"
			"pipelines.size=%zu PipelineCreateInfo.size=%zu\n",
			pipelines.size(), pcis.size());
		return 1;
	}

	language::Device * checkDevice = &pcis.at(0).dev;
	std::vector<VkSubpassDescription> only_vk_subpasses;
	std::vector<VkSubpassDependency> only_vk_deps;
	for (size_t subpass_i = 0; subpass_i < pcis.size(); subpass_i++) {
		auto& pci = pcis.at(subpass_i);
		if (pci.stages.empty()) {
			fprintf(stderr, "RenderPass::init(): pcis[%zu].stages "
				"vector cannot be empty\n", subpass_i);
			return 1;
		}
		if (checkDevice != &pci.dev) {
			fprintf(stderr, "RenderPass::init(): all pcis must use "
				"the same Device=%p, got pci[%zu].dev=%p\n",
				checkDevice, subpass_i, &pci.dev);
			return 1;
		}

		pci.subpassDesc.colorAttachmentCount = pci.colorAttaches.size();
		pci.subpassDesc.pColorAttachments = pci.colorAttaches.data();
		only_vk_subpasses.push_back(pci.subpassDesc);
		if (getSubpassDeps(subpass_i, pcis, only_vk_deps)) {
			return 1;
		}
	}
	rpci.attachmentCount = colorAttaches.size();
	rpci.pAttachments = colorAttaches.data();
	rpci.subpassCount = only_vk_subpasses.size();
	rpci.pSubpasses = only_vk_subpasses.data();

	rpci.dependencyCount = only_vk_deps.size();
	rpci.pDependencies = only_vk_deps.data();

	VkResult v = vkCreateRenderPass(checkDevice->dev, &rpci, nullptr, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateRenderPass() returned %d (%s)\n", v, string_VkResult(v));
		return 1;
	}

	for (size_t subpass_i = 0; subpass_i < pipelines.size(); subpass_i++) {
		auto& pipeline = pipelines.at(subpass_i);
		if (pipeline.init(*this, subpass_i, pcis.at(subpass_i))) {
			fprintf(stderr, "RenderPass::init() pipeline[%zu] "
				"init() failed\n", subpass_i);
			return 1;
		}
	}

	passBeginInfo.renderPass = vk;
	// passBeginInfo.framebuffer is left unset. You MUST update it for each frame!
	passBeginInfo.renderArea.offset = {0, 0};
	passBeginInfo.renderArea.extent = checkDevice->swapChainExtent;

	passBeginInfo.clearValueCount = 1;
	passBeginInfo.pClearValues = &passBeginClearColor;
	return 0;
}

int CommandPool::ctorError(language::Device& dev, VkCommandPoolCreateFlags flags)
{
	auto qfam_i = dev.getQfamI(queueFamily);
	if (qfam_i == (decltype(qfam_i)) -1) {
		return 1;
	}

	// Cache QueueFamily, as all commands in this pool must be submitted here.
	qf_ = &dev.qfams.at(qfam_i);

	VkCommandPoolCreateInfo VkInit(cpci);
	cpci.queueFamilyIndex = qfam_i;
	cpci.flags = flags;
	VkResult v = vkCreateCommandPool(dev.dev, &cpci, nullptr, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateCommandPool returned %d (%s)\n", v, string_VkResult(v));
		return 1;
	}
	return 0;
}

int CommandPool::alloc(std::vector<VkCommandBuffer>& buf,
		VkCommandBufferLevel level) {
	VkCommandBufferAllocateInfo VkInit(ai);
	ai.commandPool = vk;
	ai.level = level;
	ai.commandBufferCount = (decltype(ai.commandBufferCount)) buf.size();

	VkResult v = vkAllocateCommandBuffers(vkdev, &ai, buf.data());
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkAllocateCommandBuffers failed: %d (%s)\n", v, string_VkResult(v));
		return 1;
	}
	return 0;
}

}  // namespace command
