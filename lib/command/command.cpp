/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include "command.h"
#include <lib/language/VkInit.h>

#define VkOverwrite(x) language::internal::_VkInit(x)

namespace command {

int Shader::loadSPV(const void * spvBegin, const void * spvEnd) {
	VkShaderModuleCreateInfo VkInit(smci);
	smci.codeSize =
		reinterpret_cast<const char *>(spvEnd) -
		reinterpret_cast<const char *>(spvBegin);
	smci.pCode = reinterpret_cast<const uint32_t *>(spvBegin);
	VkResult r = vkCreateShaderModule(dev->dev, &smci, nullptr, &vk);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "loadSPV(%p, %p) vkCreateShaderModule returned %d\n",
			spvBegin, spvEnd, r);
		return 1;
	}
	return 0;
}

PipelineStage::PipelineStage() {
	VkOverwrite(sci);
}

PipelineCreateInfo::PipelineCreateInfo(language::Device& dev) : dev(dev)
{
	VkOverwrite(vertsci);
	VkOverwrite(asci);
	asci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	asci.primitiveRestartEnable = VK_FALSE;

	VkOverwrite(viewsci);
	VkExtent2D& scExtent = dev.swapchainExtent;
	viewports.push_back(VkViewport{
		x: 0.0f,
		y: 0.0f,
		width: (float) scExtent.width,
		height: (float) scExtent.height,
		minDepth: 0.0f,
		maxDepth: 1.0f,
	});
	scissors.push_back(VkRect2D{
		offset: {0, 0},
		extent: scExtent,
	});

	VkOverwrite(rastersci);
	rastersci.depthClampEnable = VK_FALSE;
	rastersci.rasterizerDiscardEnable = VK_FALSE;
	rastersci.polygonMode = VK_POLYGON_MODE_FILL;
	rastersci.lineWidth = 1.0f;
	rastersci.cullMode = VK_CULL_MODE_BACK_BIT;
	rastersci.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rastersci.depthBiasEnable = VK_FALSE;

	VkOverwrite(multisci);
	multisci.sampleShadingEnable = VK_FALSE;
	multisci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkOverwrite(depthsci);
	VkOverwrite(plci);

	VkOverwrite(cbsci);
	cbsci.logicOpEnable = VK_FALSE;
	cbsci.logicOp = VK_LOGIC_OP_COPY;
	cbsci.blendConstants[0] = 0.0f;
	cbsci.blendConstants[1] = 0.0f;
	cbsci.blendConstants[2] = 0.0f;
	cbsci.blendConstants[3] = 0.0f;
	perFramebufColorBlend.push_back(withDisabledAlpha());

	VkAttachmentReference VkInit(aref);
	aref.attachment = 0;  // Assumes RenderPass has a VkAttachmentDescription at index [0].
	aref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttaches.push_back(aref);

	VkOverwrite(subpassci);
	subpassci.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

VkPipelineColorBlendAttachmentState PipelineCreateInfo::withDisabledAlpha() {
	VkPipelineColorBlendAttachmentState VkInit(state);
	state.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	state.blendEnable = VK_FALSE;
	return state;
}

VkPipelineColorBlendAttachmentState PipelineCreateInfo::withEnabledAlpha() {
	VkPipelineColorBlendAttachmentState VkInit(state);
	state.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	state.blendEnable = VK_TRUE;
	state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	state.colorBlendOp = VK_BLEND_OP_ADD;
	state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	state.alphaBlendOp = VK_BLEND_OP_ADD;
	return state;
}

Pipeline::Pipeline(language::Device& dev)
	: pipelineLayout{dev.dev, vkDestroyPipelineLayout}
	, vk{dev.dev, vkDestroyPipeline}
{
};

int Pipeline::init(RenderPass& renderPass, size_t subpass_i, PipelineCreateInfo& pci)
{
	if (subpass_i >= renderPass.pipelines.size()) {
		fprintf(stderr, "Pipeline::init(): subpass_i=%zu when renderPass.pipeline.size=%zu\n",
			subpass_i, renderPass.pipelines.size());
		return 1;
	}

	//
	// Collect PipelineCreateInfo structures into native Vulkan structures.
	//
	pci.viewsci.viewportCount = pci.viewports.size();
	pci.viewsci.pViewports = pci.viewports.data();
	pci.viewsci.scissorCount = pci.scissors.size();
	pci.viewsci.pScissors = pci.scissors.data();

	pci.cbsci.attachmentCount = pci.perFramebufColorBlend.size();
	pci.cbsci.pAttachments = pci.perFramebufColorBlend.data();

	// TODO: Once uniforms are working, add pushConstants as well.
	pci.plci.setLayoutCount = descriptors.size();
	pci.plci.pSetLayouts = descriptors.data();
	pci.plci.pushConstantRangeCount = 0;
	pci.plci.pPushConstantRanges = 0;

	//
	// Create pipelineLayout.
	//
	VkResult v = vkCreatePipelineLayout(pci.dev.dev, &pci.plci, nullptr, &pipelineLayout);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreatePipelineLayout() returned %d\n", v);
		return 1;
	}

	VkGraphicsPipelineCreateInfo VkInit(p);
	std::vector<VkPipelineShaderStageCreateInfo> stagecis;
	for (auto& stage : pci.stages) {
		stage.sci.module = renderPass.shaders.at(stage.shader_i).vk;
		stage.sci.pName = stage.entryPointName.c_str();
		stagecis.push_back(stage.sci);
	}
	p.stageCount = stagecis.size();
	p.pStages = stagecis.data();
	p.pVertexInputState = &pci.vertsci;
	p.pInputAssemblyState = &pci.asci;
	p.pViewportState = &pci.viewsci;
	p.pRasterizationState = &pci.rastersci;
	p.pMultisampleState = &pci.multisci;
	p.pColorBlendState = &pci.cbsci;
	p.layout = pipelineLayout;
	p.renderPass = renderPass.vk;
	p.subpass = subpass_i;
	v = vkCreateGraphicsPipelines(pci.dev.dev, VK_NULL_HANDLE, 1, &p, nullptr, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateGraphicsPipelines() returned %d\n", v);
		return 1;
	}

	//
	// Create pci.dev.framebufs, now that renderPass is created.
	//
	for (size_t i = 0; i < pci.dev.framebufs.size(); i++) {
		auto& framebuf = pci.dev.framebufs.at(i);
		VkImageView attachments[] = { framebuf.imageView };

		VkFramebufferCreateInfo VkInit(fbci);
		fbci.renderPass = renderPass.vk;
		fbci.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
		fbci.pAttachments = attachments;
		fbci.width = pci.dev.swapchainExtent.width;
		fbci.height = pci.dev.swapchainExtent.height;
		fbci.layers = 1;  // Same as layers used in imageView.

		v = vkCreateFramebuffer(pci.dev.dev, &fbci, nullptr, &framebuf.vk);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "vkCreateFramebuffer() returned %d\n", v);
			return 1;
		}
	}
	return 0;
}

RenderPass::RenderPass(language::Device& dev)
		: vk{dev.dev, vkDestroyRenderPass} {
	VkAttachmentDescription VkInit(ad);
	ad.format = dev.format.format;
	ad.samples = VK_SAMPLE_COUNT_1_BIT;
	ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	ad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ad.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	colorAttaches.push_back(ad);

	VkOverwrite(rpci);
}

Shader& RenderPass::addShader(PipelineCreateInfo& pci,
		VkShaderStageFlagBits stage, std::string entryPointName) {
	shaders.emplace_back(pci.dev);
	auto& shader = *(shaders.end() - 1);

	pci.stages.emplace_back();
	auto& pipelinestage = *(pci.stages.end() - 1);
	pipelinestage.sci.stage = stage;
	pipelinestage.shader_i = shaders.size() - 1;
	pipelinestage.entryPointName = entryPointName;

	return shader;
}

int RenderPass::getSubpassDeps(size_t subpass_i,
		const std::vector<PipelineCreateInfo>& pcis,
		std::vector<VkSubpassDependency>& subpassdeps) {
	// Link this subpass to the previous one.
	VkSubpassDependency VkInit(fromprev);
	fromprev.srcSubpass = (subpass_i == 0) ? VK_SUBPASS_EXTERNAL : (subpass_i - 1);
	fromprev.dstSubpass = subpass_i;
	fromprev.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
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

	// Link this subpass to the next one.
	VkSubpassDependency VkInit(tonext);
	tonext.srcSubpass = subpass_i;
	tonext.dstSubpass = (subpass_i == pcis.size() - 1) ? VK_SUBPASS_EXTERNAL : subpass_i + 1;
	tonext.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
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
	subpassdeps.push_back(fromprev);
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

		pci.subpassci.colorAttachmentCount = pci.colorAttaches.size();
		pci.subpassci.pColorAttachments = pci.colorAttaches.data();
		only_vk_subpasses.push_back(pci.subpassci);
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
		fprintf(stderr, "vkCreateRenderPass() returned %d\n", v);
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
	return 0;
}

int Semaphore::ctor(language::Device& dev) {
	VkSemaphoreCreateInfo VkInit(sci);
	VkResult v = vkCreateSemaphore(dev.dev, &sci, nullptr, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateSemaphore returned %d\n", v);
		return 1;
	}
	return 0;
}

int Fence::ctor(language::Device& dev) {
	VkFenceCreateInfo VkInit(fci);
	VkResult v = vkCreateFence(dev.dev, &fci, nullptr, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateFence returned %d\n", v);
		return 1;
	}
	return 0;
}

int CommandPool::ctor(language::Device& dev, VkCommandPoolCreateFlags flags)
{
	VkCommandPoolCreateInfo VkInit(cpci);
	cpci.queueFamilyIndex = qfam_i;
	cpci.flags = flags;
	VkResult v = vkCreateCommandPool(dev.dev, &cpci, nullptr, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateCommandPool returned %d\n", v);
		return 1;
	}
	return 0;
};

}  // namespace command
