/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include "language.h"
#include "VkInit.h"
#include "VkEnum.h"

#include <map>

namespace language {
using namespace VkEnum;

namespace {  // use an anonymous namespace to hide all its contents (only reachable from this file)

static const char * requiredDeviceExtensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

struct InstanceInternal : public Instance {
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

		if (oneQueueWithPresentSupported) {
			// A device with a queue with PRESENT support should have all of
			// requiredDeviceExtensions. You can delete this block of code if your app has
			// different device extension logic -- e.g. check the device in devQuery().
			for (size_t i = 0, j; i < sizeof(requiredDeviceExtensions)/sizeof(requiredDeviceExtensions[0]); i++) {
				for (j = 0; j < dev.availableExtensions.size(); j++) {
					if (!strcmp(dev.availableExtensions.at(j).extensionName, requiredDeviceExtensions[i])) {
						dev.extensionRequests.push_back(requiredDeviceExtensions[i]);
						break;
					}
				}
				if (j >= dev.availableExtensions.size()) {
					// Do not add dev: it claims oneQueueWithPresentSupported but it cannot
					// support a required extension. (If it does not do PRESENT at all, it is
					// assumed the device would not be used in the swap chain anyway, so go
					// ahead and let devQuery() look at it.)
					return 0;
				}
			}

			auto* surfaceFormats = Vk::getSurfaceFormats(dev.phys, this->surface);
			if (!surfaceFormats) {
				return 1;
			}
			dev.surfaceFormats = *surfaceFormats;
			delete surfaceFormats;
			surfaceFormats = nullptr;

			auto* presentModes = Vk::getPresentModes(dev.phys, this->surface);
			if (!presentModes) {
				return 1;
			}
			dev.presentModes = *presentModes;
			delete presentModes;
			presentModes = nullptr;

			if (dev.surfaceFormats.size() == 0 || dev.presentModes.size() == 0) {
				// Do not add dev: it claims oneQueueWithPresentSupported but it has no
				// surfaceFormats -- or no presentModes.
				return 0;
			}
			int r = this->initSurfaceFormat(dev);
			if (r) {
				return r;
			}
			if ((r = this->initPresentMode(dev)) != 0) {
				return r;
			}
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

int Instance::open(VkExtent2D surfaceSizeRequest,
		devQueryFn devQuery) {
	std::vector<VkPhysicalDevice> * physDevs = Vk::getDevices(vk);
	if (physDevs == nullptr) {
		return 1;
	}

	InstanceInternal * ii = reinterpret_cast<InstanceInternal *>(this);
	int r = ii->initSupportedDevices(*physDevs);
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
			if (qfam.prios.size() < 1) {
				continue;  // This qfam is not being requested on this dev.
			} else if (qfam.prios.size() > qfam.vk.queueCount) {
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

	size_t swap_chain_count = 0;
	for (const auto& kv : requested_devs) {
		auto& dev = devs.at(kv.first);
		size_t q_count = 0;
		for (size_t q_i = 0; q_i < dev.qfams.size(); q_i++) {
			auto& qfam = dev.qfams.at(q_i);
			if (dbg_lvl && qfam.prios.size()) {
				printf("dev_i=%u q_count=%zu adding qfam[%zu] x %zu\n",
					kv.first, q_count, q_i, qfam.prios.size());
			}
			// Copy the newly minted VkQueue objects into dev.qfam.queues.
			for (size_t i = 0; i < qfam.prios.size(); i++) {
				qfam.queues.emplace_back();
				vkGetDeviceQueue(dev.dev, q_i, i, &(*(qfam.queues.end() - 1)));
				q_count++;
			}
		}
		if (q_count && dev.presentModes.size()) {
			if (swap_chain_count == 1) {
				fprintf(stderr, "Warn: A multi-GPU setup probably does not work.\n");
				fprintf(stderr, "Warn: Here be dragons.\n");
				fprintf(stderr, "Warn: https://lunarg.com/faqs/vulkan-multiple-gpus-acceleration/\n");
				/**
				 * https://lunarg.com/faqs/vulkan-multiple-gpus-acceleration/
				 *
				 * Does Vulkan support multiple GPUs or multiple GPU acceleration?
				 *
				 * There is no multiple GPU support in version 1.0. That was
				 * unfortunately a feature Khronos had to cut in order to preserve
				 * schedule. It is expected to be near the top of the list for
				 * Vulkan 1.1. It is perfectly possible for a Vulkan implementation to
				 * expose multiple GPUs. What Vulkan currently can’t do is allow
				 * resource sharing between them. So from a point of view of, for
				 * example, a Windows system manager, its possible to recognize
				 * multiple ways to render to a surface and then use operating system
				 * hooks to transfer that to the screen. What Vulkan doesn’t have is
				 * the ability to share a texture or a render target between multiple
				 * GPUs.
				 */
			}
			if (this->createSwapchain(kv.first, surfaceSizeRequest)) {
				return 1;
			}
			swap_chain_count++;
		}
	}
	return 0;
}

}  // namespace language
