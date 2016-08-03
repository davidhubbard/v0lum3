/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include "language.h"
#include "VkInit.h"
#include "VkEnum.h"

namespace language {
using namespace VkEnum;

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

static const char VK_LAYER_LUNARG_core_validation[] = "VK_LAYER_LUNARG_core_validation";
static const char VK_LAYER_LUNARG_object_tracker[] = "VK_LAYER_LUNARG_object_tracker";
static const char VK_LAYER_LUNARG_parameter_validation[] = "VK_LAYER_LUNARG_parameter_validation";
static const char VK_LAYER_LUNARG_image[] = "VK_LAYER_LUNARG_image";
static const char VK_LAYER_LUNARG_swapchain[] = "VK_LAYER_LUNARG_swapchain";

static int initInstance(Instance* inst, const char ** requiredExtensions, size_t requiredExtensionCount,
		std::vector<VkExtensionProperties>& extensions,
		std::vector<VkLayerProperties>& layers) {
	std::vector<const char *> enabledExtensions;
	std::vector<const char *> disabledExtensions;
	for (const auto& ext : extensions) {
		unsigned int i = 0;
		for (; i < requiredExtensionCount; i++) if (!strcmp(requiredExtensions[i], ext.extensionName)) {
			enabledExtensions.push_back(requiredExtensions[i]);
			break;
		}
		if (i >= requiredExtensionCount) {
			disabledExtensions.push_back(ext.extensionName);
		}
	}

	std::vector<const char *> enabledLayers;
	for (const auto& layerprop : layers) {
		// Enable instance layer "VK_LAYER_LUNARG_standard_validation"
		// Note: Modify this code to suit your application.
		//
		// Getting validation working involves more than just enabling the layer!
		// https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/tree/master/layers
		// 1. Copy libVkLayer_<name>.so to the same dir as your binary, or
		//    set VK_LAYER_PATH to point to where the libVkLayer_<name>.so is.
		// 2. Create a vk_layer_settings.txt file next to libVkLayer_<name>.so.
		// 3. Set the environment variable VK_INSTANCE_LAYERS to activate layers:
		//    export VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_standard_validation
		if (!strcmp(VK_LAYER_LUNARG_standard_validation, layerprop.layerName)) {
			enabledLayers.push_back(layerprop.layerName);
		}
		if (0 && !strcmp(VK_LAYER_LUNARG_core_validation, layerprop.layerName)) {
			enabledLayers.push_back(layerprop.layerName);
		}
		if (0 && !strcmp(VK_LAYER_LUNARG_object_tracker, layerprop.layerName)) {
			enabledLayers.push_back(layerprop.layerName);
		}
		if (0 && !strcmp(VK_LAYER_LUNARG_parameter_validation, layerprop.layerName)) {
			enabledLayers.push_back(layerprop.layerName);
		}
		if (0 && !strcmp(VK_LAYER_LUNARG_image, layerprop.layerName)) {
			enabledLayers.push_back(layerprop.layerName);
		}
		if (0 && !strcmp(VK_LAYER_LUNARG_swapchain, layerprop.layerName)) {
			enabledLayers.push_back(layerprop.layerName);
		}
	}

	// Enable extension "VK_EXT_debug_report".
	// Note: Modify this code to suit your application.
	enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

	//
	// Vulkan app instance creation process:
	// 1. VkApplicationInfo
	// 2. VkInstanceCreateInfo
	// (optional step): VkDebugReportCallbackCreateInfoEXT
	// 3. call VkCreateInstance()
	//
	char appname[2048];
	snprintf(appname, sizeof(appname), "TODO: " __FILE__ ":%d: choose ApplicationName", __LINE__);

	VkApplicationInfo VkInit(app);
	app.apiVersion = VK_API_VERSION_1_0;
	app.pApplicationName = appname;
	app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app.pEngineName = "v0lum3";

	VkInstanceCreateInfo VkInit(iinfo);
	iinfo.pApplicationInfo = &app;
	iinfo.enabledExtensionCount = enabledExtensions.size();
	iinfo.ppEnabledExtensionNames = enabledExtensions.data();
	iinfo.enabledLayerCount = enabledLayers.size();
	iinfo.ppEnabledLayerNames = enabledLayers.data();
	iinfo.enabledLayerCount = 0;

	VkDebugReportCallbackCreateInfoEXT VkInit(dinfo);
	dinfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
		VK_DEBUG_REPORT_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
		VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
	//	VK_DEBUG_REPORT_DEBUG_BIT_EXT;
	dinfo.pfnCallback = debugReportCallback;
	//
	// There is this clever trick in vulkaninfo:
	// iinfo.pNext = &dinfo;
	// Amazing use of pNext. But it triggers a memory leak in
	// loader/loader.c: loader_instance_heap_free()
	// The OBJTRACK layer prints this out right before the double free():
	// "OBJ_STAT Destroy VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT obj 0x775d790 "
	// " (1 total objs remain & 0 VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT objs)"
	//
	// The workaround is to manually call vkCreateDebugReportCallbackEXT().

	VkResult v = vkCreateInstance(&iinfo, nullptr, &inst->vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateInstance returned %d", v);
		return 1;
	}

	auto pCreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)
		vkGetInstanceProcAddr(inst->vk, "vkCreateDebugReportCallbackEXT");
	if (!pCreateDebugReportCallback) {
		fprintf(stderr, "Failed to dlsym(vkCreateDebugReportCallbackEXT)\n");
		return 1;
	}

	inst->pDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)
		vkGetInstanceProcAddr(inst->vk, "vkDestroyDebugReportCallbackEXT");
	if (!inst->pDestroyDebugReportCallbackEXT) {
		fprintf(stderr, "Failed to dlsym(vkDestroyDebugReportCallbackEXT)\n");
		return 1;
	}

	v = pCreateDebugReportCallback(inst->vk, &dinfo, nullptr /*allocator*/,
		&inst->debugReport);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "pCreateDebugReportCallback returned %d\n", v);
		return 1;
	}
	return 0;
}

}  // anonymous namespace

int Instance::ctorError(const char ** requiredExtensions, size_t requiredExtensionCount) {
	auto * extensions = Vk::getExtensions();
	if (extensions == nullptr) {
		return 1;
	}

	auto * layers = Vk::getLayers();
	if (layers == nullptr) {
		delete extensions;
		return 1;
	}

	int r = initInstance(this, requiredExtensions, requiredExtensionCount, *extensions, *layers);
	delete extensions;
	delete layers;
	return r;
}

Instance::~Instance() {
	if (pDestroyDebugReportCallbackEXT) {
		pDestroyDebugReportCallbackEXT(vk, debugReport, nullptr /*allocator*/);
	}
}

int dbg_lvl = 0;
const char VK_LAYER_LUNARG_standard_validation[] = "VK_LAYER_LUNARG_standard_validation";

}  // namespace language
