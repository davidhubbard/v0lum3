/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 *
 * This is Instance::ctorError(), though it is broken into a few different methods.
 */
#include "language.h"
#include "VkInit.h"
#include "VkEnum.h"

namespace language {
using namespace VkEnum;

const char VK_LAYER_LUNARG_standard_validation[] = "VK_LAYER_LUNARG_standard_validation";
//static const char VK_LAYER_LUNARG_core_validation[] = "VK_LAYER_LUNARG_core_validation";
//static const char VK_LAYER_LUNARG_object_tracker[] = "VK_LAYER_LUNARG_object_tracker";
//static const char VK_LAYER_LUNARG_parameter_validation[] = "VK_LAYER_LUNARG_parameter_validation";
//static const char VK_LAYER_LUNARG_image[] = "VK_LAYER_LUNARG_image";
//static const char VK_LAYER_LUNARG_swapchain[] = "VK_LAYER_LUNARG_swapchain";

namespace {  // use an anonymous namespace to hide all its contents (only reachable from this file)

struct InstanceInternal : public Instance {
	int initInstance(const char ** requiredExtensions, size_t requiredExtensionCount,
			std::vector<VkExtensionProperties>& extensions,
			std::vector<VkLayerProperties>& layers) {
		std::vector<const char *> enabledExtensions;
		for (const auto& ext : extensions) {
			unsigned int i = 0;
			for (; i < requiredExtensionCount; i++) if (!strcmp(requiredExtensions[i], ext.extensionName)) {
				enabledExtensions.push_back(requiredExtensions[i]);
				break;
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
		}

		// Enable extension "VK_EXT_debug_report".
		// Note: Modify this code to suit your application.
		enabledExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

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

		VkResult v = vkCreateInstance(&iinfo, nullptr, &this->vk);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "vkCreateInstance returned %d\n", v);
			return 1;
		}
		return 0;
	}

	int initSupportedQueues(std::vector<VkQueueFamilyProperties>& vkQFams, Device& dev) {
		VkBool32 oneQueueWithPresentSupported = false;
		for (size_t q_i = 0; q_i < vkQFams.size(); q_i++) {
			VkBool32 isPresentSupported = false;
			VkResult v = vkGetPhysicalDeviceSurfaceSupportKHR(dev.phys, q_i, this->surface,
				&isPresentSupported);
			if (v != VK_SUCCESS) {
				fprintf(stderr, "dev %zu qfam %zu: vkGetPhysicalDeviceSurfaceSupportKHR returned %d\n",
					this->devs.size(), q_i, v);
				return 1;
			}
			oneQueueWithPresentSupported |= isPresentSupported;

			dev.qfams.emplace_back(vkQFams.at(q_i), isPresentSupported ? PRESENT : NONE);
		}

		auto* devExtensions = Vk::getDeviceExtensions(dev.phys);
		if (!devExtensions) {
			return 1;
		}
		dev.availableExtensions = *devExtensions;
		delete devExtensions;
		devExtensions = nullptr;

		if (!oneQueueWithPresentSupported) {
			return 0;
		}

		// A device with a queue with PRESENT support should have all of
		// deviceWithPresentRequiredExts.
		static const char * deviceWithPresentRequiredExts[] = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		};

		size_t i = 0, j;
		for (; i < sizeof(deviceWithPresentRequiredExts)/sizeof(deviceWithPresentRequiredExts[0]); i++) {
			for (j = 0; j < dev.availableExtensions.size(); j++) {
				if (!strcmp(dev.availableExtensions.at(j).extensionName, deviceWithPresentRequiredExts[i])) {
					dev.extensionRequests.push_back(deviceWithPresentRequiredExts[i]);
					break;
				}
			}
			if (j >= dev.availableExtensions.size()) {
				// Do not add dev: it claims oneQueueWithPresentSupported but it lacks
				// required extensions. (If it does not do PRESENT at all, it is
				// assumed the device would not be used in the swap chain anyway, so it
				// is not removed.)
				this->devs.pop_back();
				return 0;
			}
		}

		// Init dev.surfaceFormats and dev.presentModes early. Your app can inspect
		// and modify them and then call open().
		int r = this->initSurfaceFormatAndPresentMode(dev);
		if (r) {
			return r;
		}
		if (dev.surfaceFormats.size() == 0 || dev.presentModes.size() == 0) {
			// Do not add dev: it claims oneQueueWithPresentSupported but it has no
			// surfaceFormats -- or no presentModes.
			this->devs.pop_back();
		}
		return 0;
	}

	int initSupportedDevices(std::vector<VkPhysicalDevice>& physDevs) {
		for (const auto& phys : physDevs) {
			//VkPhysicalDeviceProperties gpu_props;
			//vkGetPhysicalDeviceProperties(phys, &gpu_props);

			auto* vkQFams = Vk::getQueueFamilies(phys);
			if (vkQFams == nullptr) {
				return 1;
			}

			// Construct dev in place. emplace_back(Device{}) produced this error:
			// "sorry, unimplemented: non-trivial designated initializers not supported"
			//
			// Be careful to also call pop_back() unless initSupportQueues() succeeded.
			this->devs.resize(this->devs.size() + 1);
			Device& dev = *(this->devs.end() - 1);
			dev.phys = phys;

			//VkPhysicalDeviceFeatures physDevFeatures;
			//vkGetPhysicalDeviceFeatures(phys, &physDevFeatures);

			int r = initSupportedQueues(*vkQFams, dev);
			delete vkQFams;
			vkQFams = nullptr;
			if (r) {
				this->devs.pop_back();
				return r;
			}
		}
		return 0;
	}
};

}  // anonymous namespace

int Instance::ctorError(const char ** requiredExtensions, size_t requiredExtensionCount,
		CreateWindowSurfaceFn createWindowSurface, void *window) {
	auto * extensions = Vk::getExtensions();
	if (extensions == nullptr) {
		return 1;
	}

	auto * layers = Vk::getLayers();
	if (layers == nullptr) {
		delete extensions;
		return 1;
	}

	InstanceInternal * ii = reinterpret_cast<InstanceInternal *>(this);
	int r = ii->initInstance(requiredExtensions, requiredExtensionCount, *extensions, *layers);
	delete extensions;
	extensions = nullptr;
	delete layers;
	layers = nullptr;
	if (r) {
		return r;
	}

	r = ii->initDebug();
	if (r) {
		return r;
	}

	VkResult v = createWindowSurface(*this, window);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "createWindowSurface (the user-provided fn) failed: %d", v);
		return 1;
	}

	std::vector<VkPhysicalDevice> * physDevs = Vk::getDevices(vk);
	if (physDevs == nullptr) {
		return 1;
	}

	r = ii->initSupportedDevices(*physDevs);
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
	return r;
}

}  // namespace language
