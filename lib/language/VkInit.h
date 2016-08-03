/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include <string.h>
#include <vulkan/vulkan.h>

#pragma once

// VkInit simplifies vulkan object initialization idioms by automatically
// initializing the Vulkan object as soon as it is instantiated.
#define VkInit(x) x; ::language::internal::_VkInit(x)

namespace language {
namespace internal {

inline void _VkInit(VkApplicationInfo& app) {
	memset(&app, 0, sizeof(app));
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
};

inline void _VkInit(VkInstanceCreateInfo& ici) {
	memset(&ici, 0, sizeof(ici));
	ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
};

inline void _VkInit(VkDebugReportCallbackCreateInfoEXT& drcb) {
	memset(&drcb, 0, sizeof(drcb));
	drcb.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
};

inline void _VkInit(VkDeviceCreateInfo& dci) {
	memset(&dci, 0, sizeof(dci));
	dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
}

inline void _VkInit(VkDeviceQueueCreateInfo& qinfo) {
	memset(&qinfo, 0, sizeof(qinfo));
	qinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
}

inline void _VkInit(VkPhysicalDeviceFeatures& df) {
	memset(&df, 0, sizeof(df));
	// VkPhysicalDeviceFeatures has no 'sType'.
}

inline void _VkInit(VkSwapchainCreateInfoKHR& scci) {
	memset(&scci, 0, sizeof(scci));
	scci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
}

inline void _VkInit(VkImageViewCreateInfo& ivci) {
	memset(&ivci, 0, sizeof(ivci));
	ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
}

inline void _VkInit(VkShaderModuleCreateInfo& smci) {
	memset(&smci, 0, sizeof(smci));
	smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
}

inline void _VkInit(VkPipelineShaderStageCreateInfo& ssci) {
	memset(&ssci, 0, sizeof(ssci));
	ssci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
}

inline void _VkInit(VkPipelineVertexInputStateCreateInfo& vsci) {
	memset(&vsci, 0, sizeof(vsci));
	vsci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineInputAssemblyStateCreateInfo& asci) {
	memset(&asci, 0, sizeof(asci));
	asci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineViewportStateCreateInfo& vsci) {
	memset(&vsci, 0, sizeof(vsci));
	vsci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineRasterizationStateCreateInfo& rsci) {
	memset(&rsci, 0, sizeof(rsci));
	rsci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineMultisampleStateCreateInfo& msci) {
	memset(&msci, 0, sizeof(msci));
	msci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineDepthStencilStateCreateInfo& dsci) {
	memset(&dsci, 0, sizeof(dsci));
	dsci.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineColorBlendAttachmentState& cbas) {
	memset(&cbas, 0, sizeof(cbas));
	// VkPipelineColorBlendAttachmentState has no 'sType'.
}

inline void _VkInit(VkPipelineColorBlendStateCreateInfo& cbsci) {
	memset(&cbsci, 0, sizeof(cbsci));
	cbsci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
}

inline void _VkInit(VkPipelineLayoutCreateInfo& plci) {
	memset(&plci, 0, sizeof(plci));
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
}

inline void _VkInit(VkAttachmentDescription& ad) {
	memset(&ad, 0, sizeof(ad));
	// VkAttachmentDescription has no 'sType'.
}

inline void _VkInit(VkAttachmentReference& ar) {
	memset(&ar, 0, sizeof(ar));
	// VkAttachmentReference has no 'sType'.
}

inline void _VkInit(VkSubpassDescription& sd) {
	memset(&sd, 0, sizeof(sd));
	// VkSubpassDescription has no 'sType'.
}

inline void _VkInit(VkRenderPassCreateInfo& rpci) {
	memset(&rpci, 0, sizeof(rpci));
	rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
}

inline void _VkInit(VkSubpassDependency& spd) {
	memset(&spd, 0, sizeof(spd));
	// VkSubpassDependency has no 'sType'.
}

inline void _VkInit(VkGraphicsPipelineCreateInfo& gpci) {
	memset(&gpci, 0, sizeof(gpci));
	gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
}

inline void _VkInit(VkFramebufferCreateInfo &fbci) {
	memset(&fbci, 0, sizeof(fbci));
	fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
}

inline void _VkInit(VkSemaphoreCreateInfo &sci) {
	memset(&sci, 0, sizeof(sci));
	sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
}

inline void _VkInit(VkFenceCreateInfo &fci) {
	memset(&fci, 0, sizeof(fci));
	fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
}

inline void _VkInit(VkCommandPoolCreateInfo &cpci) {
	memset(&cpci, 0, sizeof(cpci));
	cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
}

inline void _VkInit(VkRenderPassBeginInfo &rpbi) {
	memset(&rpbi, 0, sizeof(rpbi));
	rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
}

inline void _VkInit(VkPresentInfoKHR &pik) {
	memset(&pik, 0, sizeof(pik));
	pik.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
}

}  // namespace internal
}  // namespace language
