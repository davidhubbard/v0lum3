/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include "language.h"
#include "VkInit.h"
#include "VkPtr.h"
#include "VkEnum.h"

#include <string>
#include <map>

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
	if (!strcmp(pLayerPrefix, "loader")) {
		if (strstr(pMsg, "manifest file") && debugLineCount < 20) {
			return false;
		}
		if (!strcmp(pMsg, "Build ICD instance extension list")) {
			extensionListSuppress = 1;
			return false;
		}
		if (extensionListSuppress && !strncmp(pMsg, "Instance Extension:", 19)) {
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

static const char VK_LAYER_LUNARG_standard_validation[] = "VK_LAYER_LUNARG_standard_validation";

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
		if (!strcmp(VK_LAYER_LUNARG_standard_validation, layerprop.layerName)) {
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
		VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
		VK_DEBUG_REPORT_DEBUG_BIT_EXT;
	dinfo.pfnCallback = debugReportCallback;
	iinfo.pNext = &dinfo;

	VkResult v = vkCreateInstance(&iinfo, nullptr, &inst->vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateInstance returned %d", v);
		return 1;
	}
	return 0;
}

static const char * requiredDeviceExtensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

static int initSupportedQueues(Instance * inst, const VkSurfaceKHR& surface,
		const VkPhysicalDevice& phys, std::vector<VkQueueFamilyProperties>& vkQFams) {
	// Construct dev by copy. emplace_back(Device{}) produced this error:
	// "sorry, unimplemented: non-trivial designated initializers not supported"
	Device dev;
	memset(&dev, 0, sizeof(dev));
	dev.phys = phys;

	VkBool32 oneQueueWithPresentSupported = false;
	for (size_t q_i = 0; q_i < vkQFams.size(); q_i++) {
		VkBool32 isPresentSupported = false;
		VkResult v = vkGetPhysicalDeviceSurfaceSupportKHR(phys, q_i, surface, &isPresentSupported);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "dev %zu qfam %zu: vkGetPhysicalDeviceSurfaceSupportKHR returned %d\n",
				inst->devs.size(), q_i, v);
			return 1;
		}
		oneQueueWithPresentSupported |= isPresentSupported;
		printf("dev %zu qfam %zu: isPresentSupported=%s\n", inst->devs.size(), q_i, isPresentSupported ? "T" : "F");

		dev.qfams.emplace_back(QueueFamily{
			vk: vkQFams.at(q_i),
			surfaceSupport: isPresentSupported ? PRESENT : NONE,
			prios: std::vector<float>(),
			queues: std::vector<VkQueue>(),
		});
	}

	auto* devExtensions = Vk::getDeviceExtensions(phys);
	if (!devExtensions) {
		return 1;
	}
	dev.availableExtensions = *devExtensions;
	delete devExtensions;
	devExtensions = nullptr;

	printf("dev %zu oneQueueWithPresentSupported=%s\n", inst->devs.size(), oneQueueWithPresentSupported ? "T" : "F");
	bool allExtensionsFound = true;
	if (oneQueueWithPresentSupported) {
		// A device with a queue that can PRESENT must have
		// all of requiredDeviceExtensions.
		// (You can delete this block of code if your app checks
		// device extension support in devQuery().)
		for (size_t i = 0, j; i < sizeof(requiredDeviceExtensions)/sizeof(requiredDeviceExtensions[0]); i++) {
			for (j = 0; j < dev.availableExtensions.size(); j++) {
				if (!strcmp(dev.availableExtensions.at(j).extensionName, requiredDeviceExtensions[i])) {
					break;
				}
			}
			if (j >= dev.availableExtensions.size()) {
				// requiredDeviceExtensions[i] was not found in dev.availableExtensions.
				printf("dev %zu does not have: %s\n", inst->devs.size(), requiredDeviceExtensions[i]);
				allExtensionsFound = false;
				break;
			}
		}
	}

	printf("dev %zu allExtensionsFound=%s\n", inst->devs.size(), allExtensionsFound ? "T" : "F");
	if (allExtensionsFound) {
		inst->devs.push_back(dev);
	}
	return 0;
}

static int initSupportedDevices(Instance * inst, const VkSurfaceKHR& surface,
		std::vector<VkPhysicalDevice>& physDevs) {
	for (const auto& phys : physDevs) {
		auto* vkQFams = Vk::getQueueFamilies(phys);
		if (vkQFams == nullptr) {
			return 1;
		}
		int r = initSupportedQueues(inst, surface, phys, *vkQFams);
		delete vkQFams;
		vkQFams = nullptr;
		if (r) {
			return r;
		}
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
	vkDestroyInstance(vk, nullptr);
}

int Instance::open(const VkSurfaceKHR& surface, devQueryFn devQuery) {
	std::vector<VkPhysicalDevice> * physDevs = Vk::getDevices(vk);
	if (physDevs == nullptr) {
		return 1;
	}
	int r = initSupportedDevices(this, surface, *physDevs);
	delete physDevs;
	physDevs = nullptr;
	if (r) {
		return r;
	}

	if (devs.size() == 0) {
		fprintf(stderr, "No Vulkan-capable devices found on your system. Try running vulkaninfo to troubleshoot.\n");
		return 1;
	}

	if (dbg_lvl > 0) {
		printf("%zu physical device%s:\n", devs.size(), devs.size() != 1 ? "s" : "");
		for (size_t n = 0; n < devs.size(); n++) {
			const auto& phys = devs.at(n).phys;
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(phys, &props);
			printf("  [%zu] \"%s\"\n", n, props.deviceName);
		}
	}

	std::vector<QueueRequest> request;
	if ((r = devQuery(request)) != 0) {
		return r;
	}

	// Split up QueueRequest by device index. This has the side effect of
	// ignoring any device for which there is no QueueRequest. Use a std::map
	// instead of a std::multimap to be able to iterator over keys.
	std::map<uint32_t, std::vector<QueueRequest>> requested_devs;
	for (auto& req : request) {
		auto it = requested_devs.find(req.dev_index);
		if (it == requested_devs.end()) {
			auto ret = requested_devs.emplace(req.dev_index, std::vector<QueueRequest>());
			it = ret.first;
		}
		it->second.push_back(req);
	}

	for (const auto& kv : requested_devs) {
		auto& dev = devs.at(kv.first);
		std::vector<VkDeviceQueueCreateInfo> allQci;

		// Vulkan wants the number of queues by queue family (per device).
		// kv.second (vector<QueueRequest>) has a list of dev_qfam_index (per device).
		// This loop groups QueueRequest by dev_qfam_index == q_i.
		for (size_t q_i = 0; q_i < dev.qfams.size(); q_i++) {
			auto& qfam = dev.qfams.at(q_i);
			for (const auto& qr : kv.second) {
				if (qr.dev_qfam_index == q_i) {
					qfam.prios.push_back(qr.priority);
				}
			}

			// Now have c.prios.size() == how many queues to create of this queue family.
			if (qfam.prios.size() > qfam.vk.queueCount) {
				fprintf(stderr, "Cannot request %zu of dev_i=%zu, qFam[%zu] (max %zu allowed)\n",
					qfam.prios.size(), (size_t) kv.first, q_i, (size_t) qfam.vk.queueCount);
				return 1;
			}

			VkDeviceQueueCreateInfo VkInit(dqci);
			dqci.queueFamilyIndex = q_i;
			dqci.queueCount = qfam.prios.size();
			dqci.pQueuePriorities = qfam.prios.data();
			allQci.push_back(dqci);
		}

		VkPhysicalDeviceFeatures VkInit(deviceFeatures);
		// TODO: add deviceFeatures.

		// Enable device layer "VK_LAYER_LUNARG_standard_validation"
		// Note: Modify this code to suit your application.
		std::vector<const char *> enabledLayers;
		enabledLayers.push_back(VK_LAYER_LUNARG_standard_validation);

		VkDeviceCreateInfo VkInit(dCreateInfo);
		dCreateInfo.queueCreateInfoCount = allQci.size();
		dCreateInfo.pQueueCreateInfos = allQci.data();
		dCreateInfo.pEnabledFeatures = &deviceFeatures;
		dCreateInfo.enabledExtensionCount = dev.extensionRequests.size();
		dCreateInfo.ppEnabledExtensionNames = dev.extensionRequests.data();
		dCreateInfo.enabledLayerCount = enabledLayers.size();
		dCreateInfo.ppEnabledLayerNames = enabledLayers.data();

		VkResult v = vkCreateDevice(dev.phys, &dCreateInfo, nullptr /*allocator*/, &dev.dev);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "dev_i=%zu VkCreateDevice() returned %d\n", (size_t) kv.first, v);
			return 1;
		}
	}

	// Copy KvQueue objects into dev.qfam.queues.
	for (const auto& kv : requested_devs) {
		auto& dev = devs.at(kv.first);
		for (size_t q_i = 0; q_i < dev.qfams.size(); q_i++) {
			auto& qfam = dev.qfams.at(q_i);
			if (dbg_lvl && qfam.prios.size()) {
				printf("OK: dev %zu qfam %zu: got %zu queue%s\n",
					(size_t) kv.first, q_i, qfam.prios.size(), qfam.prios.size() != 1 ? "s" : "");
			}
			for (size_t i = 0; i < qfam.prios.size(); i++) {
				qfam.queues.emplace_back();
				vkGetDeviceQueue(dev.dev, q_i, i, &(*(qfam.queues.end() - 1)));
			}
		}
	}
	return 0;
}

int dbg_lvl = 0;

}  // namespace language
