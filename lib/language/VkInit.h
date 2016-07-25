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
	// VkPipelineColorBlendAttachmentState has no 'sType'.
}

}  // namespace internal
}  // namespace language
