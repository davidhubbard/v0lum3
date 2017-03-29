/* Copyright (c) David Hubbard 2017. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

int RenderPass::getSubpassDeps(size_t subpass_i,
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
	if (subpass_i == pipelines.size() - 1) {
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

int RenderPass::ctorError(language::Device& dev) {
	if (shaders.empty()) {
		fprintf(stderr, "RenderPass::init(): 0 shaders\n");
		return 1;
	}
	if (pipelines.empty()) {
		fprintf(stderr, "RenderPass::init(): 0 pipelines\n");
		return 1;
	}

	std::vector<VkAttachmentDescription> attachmentVk;
	std::vector<VkAttachmentReference> attachmentRefVk;
	std::vector<VkSubpassDescription> subpassVk;
	std::vector<VkSubpassDependency> depVk;
	for (size_t subpass_i = 0; subpass_i < pipelines.size(); subpass_i++) {
		auto& pci = pipelines.at(subpass_i).info;
		if (pci.stages.empty()) {
			fprintf(stderr, "RenderPass::init(): pcis[%zu].stages "
				"vector cannot be empty\n", subpass_i);
			return 1;
		}

		// Bookmark where the pci.attach data will be saved in attachmentRefs.
		auto prevRefSize = attachmentRefVk.size();
		VkAttachmentReference depthRef;
		int depthRefIndex = -1;
		for (size_t attach_i = 0; attach_i < pci.attach.size(); attach_i++) {
			auto& a = pci.attach.at(attach_i);

			// Save each a.refvk into attachmentRefVk, each a.vk into attachmentVk.
			// Up to one depth buffer is added to attachmentRefVk much later.
			if (a.refvk.layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
				if (depthRefIndex != -1) {
					fprintf(stderr, "PipelineCreateInfo[%zu].attach[%zu] and "
						"attach[%d] are both DEPTH. Can only have one!\n",
						subpass_i, attach_i, depthRefIndex);
					return 1;
				}
				depthRefIndex = attach_i;
				depthRef = a.refvk;
			} else {
				a.refvk.attachment = attachmentVk.size();
				attachmentRefVk.emplace_back(a.refvk);
				attachmentVk.emplace_back(a.vk);
			}
			if (attachmentRefVk.size() != attachmentVk.size()) {
				fprintf(stderr, "BUG: attachmentVk and attachmentRefVk out of sync\n");
				exit(1);
				return 1;
			}
		}

		// Write attachmentRefVk[prevRefSize:] to pci.subpassDesc.
		// Note that these are ONLY color attachments. depthRef is left out.
		pci.subpassDesc.colorAttachmentCount = attachmentRefVk.size() - prevRefSize;
		pci.subpassDesc.pColorAttachments = attachmentRefVk.data() + prevRefSize;

		// Write depthRef last.
		if (depthRefIndex != -1) {
			depthRef.attachment = attachmentVk.size();
			pci.attach.at(depthRefIndex).refvk.attachment = attachmentVk.size();
			attachmentRefVk.emplace_back(depthRef);
			attachmentVk.emplace_back(pci.attach.at(depthRefIndex).vk);
			pci.subpassDesc.pDepthStencilAttachment = &*(attachmentRefVk.end() - 1);
		}

		// Save pci.subpassDesc into only_vk_subpasses.
		subpassVk.push_back(pci.subpassDesc);
		if (getSubpassDeps(subpass_i, depVk)) {
			return 1;
		}
	}
	rpci.attachmentCount = attachmentVk.size();
	rpci.pAttachments = attachmentVk.data();
	rpci.subpassCount = subpassVk.size();
	rpci.pSubpasses = subpassVk.data();

	rpci.dependencyCount = depVk.size();
	rpci.pDependencies = depVk.data();

	VkResult v = vkCreateRenderPass(dev.dev, &rpci, nullptr, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateRenderPass() returned %d (%s)\n", v, string_VkResult(v));
		return 1;
	}

	for (size_t subpass_i = 0; subpass_i < pipelines.size(); subpass_i++) {
		auto& pipeline = pipelines.at(subpass_i);
		if (pipeline.init(dev, *this, subpass_i)) {
			fprintf(stderr, "RenderPass::init() pipeline[%zu] "
				"init() failed\n", subpass_i);
			return 1;
		}
	}

	passBeginInfo.renderPass = vk;
	// passBeginInfo.framebuffer is left unset. You MUST update it for each frame!
	passBeginInfo.renderArea.offset = {0, 0};
	passBeginInfo.renderArea.extent = dev.swapChainExtent;

	passBeginInfo.clearValueCount = passBeginClearColors.size();
	passBeginInfo.pClearValues = passBeginClearColors.data();
	return 0;
}

}  // namespace command
