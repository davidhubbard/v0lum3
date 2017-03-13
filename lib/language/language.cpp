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
		for (size_t i = 0; i < requiredExtensionCount; i++) {
			if (requiredExtensions[i] == nullptr) {
				fprintf(stderr, "invalid requiredExtensions[%zu]\n", i);
				return 1;
			}
			bool found = true;
			for (const auto& ext : extensions) {
				if (!strcmp(requiredExtensions[i], ext.extensionName)) {
					enabledExtensions.push_back(requiredExtensions[i]);
					found = true;
					break;
				}
			}
			if (!found) {
				fprintf(stderr, "requiredExtensions[%zu]=\"%s\": "
					"no devices with this extension found.\n",
					i, requiredExtensions[i]);
			}
		}

		std::vector<const char *> enabledLayers;
		for (const auto& layerprop : layers) {
			// Enable instance layer "VK_LAYER_LUNARG_standard_validation"
			// TODO: permit customization of the enabled instance layers.
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

		VkInstanceCreateInfo VkInit(iinfo);
		iinfo.pApplicationInfo = &applicationInfo;
		if (enabledExtensions.size()) {
			iinfo.enabledExtensionCount = enabledExtensions.size();
			iinfo.ppEnabledExtensionNames = enabledExtensions.data();
		}
		iinfo.enabledLayerCount = enabledLayers.size();
		iinfo.ppEnabledLayerNames = enabledLayers.data();
		iinfo.enabledLayerCount = 0;

		VkResult v = vkCreateInstance(&iinfo, pAllocator, &this->vk);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "vkCreateInstance failed: %d (%s)\n", v, string_VkResult(v));
			if (v == VK_ERROR_INCOMPATIBLE_DRIVER) {
				fprintf(stderr, "Most likely cause: your GPU does not support Vulkan yet.\n"
					"You may try updating your graphics driver.\n");
			} else if (v == VK_ERROR_OUT_OF_HOST_MEMORY) {
				fprintf(stderr, "Primary cause: you *might* be out of memory (unlikely).\n"
					"Secondary causes: conflicting vulkan drivers installed.\n"
					"Secondary causes: broken driver installation.\n"
					"You may want to search the web for more information.\n");
			}
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
				fprintf(stderr, "dev %zu qfam %zu: vkGetPhysicalDeviceSurfaceSupportKHR returned %d (%s)\n",
					this->devs.size(), q_i, v, string_VkResult(v));
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
			vkGetPhysicalDeviceProperties(phys, &dev.physProp);

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

Instance::Instance() {
	applicationName = "TODO: " __FILE__ ": customize applicationName";
	engineName = "v0lum3";

	VkOverwrite(applicationInfo);
	applicationInfo.apiVersion = VK_API_VERSION_1_0;
	applicationInfo.pApplicationName = applicationName.c_str();
	applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	applicationInfo.pEngineName = engineName.c_str();
}

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
		fprintf(stderr, "createWindowSurface (the user-provided fn) failed: %d (%s)", v, string_VkResult(v));
		return 1;
	}
	surface.allocator = pAllocator;

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
		fprintf(stderr, "%zu physical device%s:\n", devs.size(), devs.size() != 1 ? "s" : "");
		for (size_t n = 0; n < devs.size(); n++) {
			fprintf(stderr, "  [%zu] \"%s\"\n", n, devs.at(n).physProp.deviceName);
		}
	}
	return r;
}

}  // namespace language
