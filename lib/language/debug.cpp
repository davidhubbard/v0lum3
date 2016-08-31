/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include "language.h"
#include "VkInit.h"

namespace language {

namespace {  // use an anonymous namespace to hide all its contents (only reachable from this file)

static uint64_t debugLineCount = 0;
static uint32_t extensionListSuppress = 0;
static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(VkFlags msgFlags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject,
		size_t location, int32_t msgCode, const char *pLayerPrefix, const char *pMsg, void *pUserData) {
	(void) objType;
	(void) srcObject;
	(void) location;
	(void) pUserData;
	debugLineCount++;

	// Suppress the most common log messages.
	if (!strcmp(pLayerPrefix, "DebugReport")) {
		if (!strcmp(pMsg, "Added callback")) {
			return false;
		}
	} else if (!strcmp(pLayerPrefix, "loader")) {
		if (strstr(pMsg, "manifest file") && debugLineCount < 20) {
			return false;
		}
		if (strstr(pMsg, VK_LAYER_LUNARG_standard_validation) && debugLineCount < 30) {
			return false;
		}
		if (!strcmp(pMsg, "Build ICD instance extension list")) {
			extensionListSuppress = 1;
			return false;
		}
		if (extensionListSuppress && !strncmp(pMsg, "Instance Extension:", 19)) {
			return false;
		}
		if (!strncmp(pMsg, "Searching for ICD drivers named", 31)) {
			return false;
		}
		if (!strncmp(pMsg, "Chain: instance: Loading layer library", 38)) {
			return false;
		}
		extensionListSuppress = 0;
	}
	extensionListSuppress = 0;

	char level[16];
	char * plevel = &level[0];
	if (msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT)             *plevel++ = 'W';
	if (msgFlags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)         *plevel++ = 'I';
	if (msgFlags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) *plevel++ = 'P';
	if (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT)               *plevel++ = 'E';
	if (msgFlags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)               *plevel++ = 'D';
	*plevel = 0;
	fprintf(stderr, "%s %s: code%d: %s\n", level, pLayerPrefix, msgCode, pMsg);
	/*
	 * false indicates that layer should not bail-out of an
	 * API call that had validation failures. This may mean that the
	 * app dies inside the driver due to invalid parameter(s).
	 * That's what would happen without validation layers, so we'll
	 * keep that behavior here.
	 */
	return false;
}

} // anonymous namespace

int Instance::initDebug() {
	//
	// There is this clever trick in the vulkaninfo source code:
	// iinfo.pNext = &dinfo;
	//
	// That is  amazing use of pNext. But it triggers a memory leak in
	// loader/loader.c: loader_instance_heap_free()
	// The OBJTRACK layer prints this out right before the double free():
	// "OBJ_STAT Destroy VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT obj 0x775d790 "
	// " (1 total objs remain & 0 VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT objs)"
	//
	// So manually call vkCreateDebugReportCallbackEXT() instead of using pNext.

	VkDebugReportCallbackCreateInfoEXT VkInit(dinfo);
	dinfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
		VK_DEBUG_REPORT_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
		VK_DEBUG_REPORT_DEBUG_BIT_EXT;
	dinfo.pfnCallback = debugReportCallback;

	auto pCreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)
		vkGetInstanceProcAddr(this->vk, "vkCreateDebugReportCallbackEXT");
	if (!pCreateDebugReportCallback) {
		fprintf(stderr, "Failed to dlsym(vkCreateDebugReportCallbackEXT)\n");
		return 1;
	}

	this->pDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)
		vkGetInstanceProcAddr(this->vk, "vkDestroyDebugReportCallbackEXT");
	if (!this->pDestroyDebugReportCallbackEXT) {
		fprintf(stderr, "Failed to dlsym(vkDestroyDebugReportCallbackEXT)\n");
		return 1;
	}

	VkResult v = pCreateDebugReportCallback(this->vk, &dinfo, nullptr /*allocator*/,
		&this->debugReport);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "pCreateDebugReportCallback returned %d\n", v);
		return 1;
	}
	return 0;
}

int dbg_lvl = 0;

Instance::~Instance() {
	if (pDestroyDebugReportCallbackEXT) {
		pDestroyDebugReportCallbackEXT(vk, debugReport, nullptr /*allocator*/);
	}
}

}  // namespace language
