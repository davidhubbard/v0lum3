/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include "command.h"


namespace command {

int Shader::loadSPV(const void * spvBegin, const void * spvEnd) {
	VkShaderModuleCreateInfo VkInit(smci);
	smci.codeSize =
		reinterpret_cast<const char *>(spvEnd) -
		reinterpret_cast<const char *>(spvBegin);
	if (smci.codeSize % 4 != 0) {
		fprintf(stderr, "LoadSPV(%p, %p) size %zu is invalid\n",
			spvBegin, spvEnd, smci.codeSize);
		return 1;
	}

	smci.pCode = reinterpret_cast<const uint32_t *>(spvBegin);
	VkResult r = vkCreateShaderModule(dev->dev, &smci, nullptr, &vk);
	if (r != VK_SUCCESS) {
		fprintf(stderr, "loadSPV(%p, %p) vkCreateShaderModule returned %d (%s)\n",
			spvBegin, spvEnd, r, string_VkResult(r));
		return 1;
	}
	return 0;
}

int Shader::loadSPV(const char * filename) {
	int infile = open(filename, O_RDONLY);
	if (infile < 0) {
		fprintf(stderr, "loadSPV: open(%s) failed: %d %s\n", filename, errno, strerror(errno));
		return 1;
	}
	struct stat s;
	if (fstat(infile, &s) == -1) {
		fprintf(stderr, "loadSPV: fstat(%s) failed: %d %s\n", filename, errno, strerror(errno));
		return 1;
	}
	char * map = (char *) mmap(0, s.st_size, PROT_READ, MAP_SHARED, infile, 0 /*offset*/);
	if (!map) {
		fprintf(stderr, "loadSPV: mmap(%s) failed: %d %s\n", filename, errno, strerror(errno));
		close(infile);
		return 1;
	}

	int r = loadSPV(map, map + s.st_size);
	if (munmap(map, s.st_size) < 0) {
		fprintf(stderr, "loadSPV: munmap(%s) failed: %d %s\n", filename, errno, strerror(errno));
		return 1;
	}
	if (close(infile) < 0) {
		fprintf(stderr, "loadSPV: close(%s) failed: %d %s\n", filename, errno, strerror(errno));
		return 1;
	}
	return r;
}

PipelineCreateInfo::PipelineCreateInfo(language::Device& dev,
		RenderPass& renderPass) : dev(dev), renderPass(renderPass)
{
	VkOverwrite(vertsci);
	VkOverwrite(asci);
	asci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	asci.primitiveRestartEnable = VK_FALSE;

	VkOverwrite(viewsci);
	VkExtent2D& scExtent = dev.swapChainExtent;
	viewports.push_back(VkViewport{
		/*x:*/ 0.0f,
		/*y:*/ 0.0f,
		/*width:*/ (float) scExtent.width,
		/*height:*/ (float) scExtent.height,
		/*minDepth:*/ 0.0f,
		/*maxDepth:*/ 1.0f,
	});
	scissors.push_back(VkRect2D{
		/*offset:*/ {0, 0},
		/*extent:*/ scExtent,
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

	VkOverwrite(subpassDesc);
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

Shader& PipelineCreateInfo::addShader(VkShaderStageFlagBits stageBits,
		std::string entryPointName) {
	renderPass.shaders.emplace_back(dev);
	auto& shader = *(renderPass.shaders.end() - 1);

	stages.emplace_back();
	auto& pipelinestage = *(stages.end() - 1);
	pipelinestage.sci.stage = stageBits;
	pipelinestage.shader_i = renderPass.shaders.size() - 1;
	pipelinestage.entryPointName = entryPointName;

	return shader;
}

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
	pipelineLayout.allocator = dev.dev.allocator;
	vk.allocator = dev.dev.allocator;
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
		fprintf(stderr, "vkCreatePipelineLayout() returned %d (%s)\n", v, string_VkResult(v));
		return 1;
	}

	VkGraphicsPipelineCreateInfo VkInit(p);
	std::vector<VkPipelineShaderStageCreateInfo> stagecis;
	for (auto& stage : pci.stages) {
		stage.sci.module = renderPass.shaders.at(stage.shader_i).vk;
		stage.sci.pName = "main";
//stage.entryPointName.c_str();
		//fprintf(stderr, "Pipeline::init: stage[%zu] \"%p\"\n", stagecis.size(), stage.sci.pName);
		//fprintf(stderr, "Pipeline::init:      [%zu] \"%s\"\n", stagecis.size(), stage.sci.pName);
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

	vk.reset();
	v = vkCreateGraphicsPipelines(pci.dev.dev, VK_NULL_HANDLE, 1, &p, nullptr, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateGraphicsPipelines() returned %d (%s)\n", v, string_VkResult(v));
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
		fbci.width = pci.dev.swapChainExtent.width;
		fbci.height = pci.dev.swapChainExtent.height;
		fbci.layers = 1;  // Same as layers used in imageView.

		v = vkCreateFramebuffer(pci.dev.dev, &fbci, nullptr, &framebuf.vk);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "vkCreateFramebuffer() returned %d (%s)\n", v, string_VkResult(v));
			return 1;
		}
	}
	return 0;
}

}  // namespace command
