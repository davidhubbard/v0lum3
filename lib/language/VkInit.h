/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include <string.h>

#pragma once

#ifndef VK_NULL_HANDLE
#error VkInit.h requires <vulkan/vulkan.h> to be included before it.
#else

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

}  // namespace internal
}  // namespace language

#endif // ifdef VK_NULL_HANDLE
