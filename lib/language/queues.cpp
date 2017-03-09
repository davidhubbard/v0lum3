/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 *
 * This is Instance::open(), though it is broken into a few different methods.
 */
#include "language.h"
#include "VkInit.h"
#include "VkEnum.h"

#include <map>

namespace language {
using namespace VkEnum;

int Instance::initQueues(std::vector<QueueRequest>& request) {
	// Search for a single device that can do both PRESENT and GRAPHICS.
	bool foundQueue = false;
	for (size_t dev_i = 0; dev_i < devs_size(); dev_i++) {
		auto selectedQfams = requestQfams(dev_i, {language::PRESENT, language::GRAPHICS});
		foundQueue |= selectedQfams.size() > 0;
		request.insert(request.end(), selectedQfams.begin(), selectedQfams.end());
	}
	if (!foundQueue) {
		fprintf(stderr, "Error: no device has both PRESENT and GRAPHICS queues.\n");
		return 1;
	}
	return 0;
}

int Instance::open(VkExtent2D surfaceSizeRequest) {
	std::vector<QueueRequest> request;
	int r = initQueues(request);
	if (r != 0) {
		return r;
	}

	// Split up QueueRequest by device index. This has the side effect of
	// ignoring any device for which there is no QueueRequest.
	std::map<uint32_t, std::vector<QueueRequest>> requested_devs;
	for (auto& req : request) {
		auto it = requested_devs.find(req.dev_index);
		if (it == requested_devs.end()) {
			auto ret = requested_devs.emplace(req.dev_index, std::vector<QueueRequest>());
			it = ret.first;
		}
		it->second.push_back(req);
	}

	// For each device that has one or more queues requested, call vkCreateDevice()
	// i.e. dispatch each queue request's dev_index (devs.at(dev_index)) to vkCreateDevice()
	for (const auto& kv : requested_devs) {
		auto& dev = devs.at(kv.first);
		std::vector<VkDeviceQueueCreateInfo> allQci;

		// Vulkan wants the queues grouped by queue family (and also grouped by device).
		// kv.second (a vector<QueueRequest>) has an unordered list of dev_qfam_index.
		// Assemble QueueRequests by brute force looking for each dev_qfam_index.
		for (size_t q_i = 0; q_i < dev.qfams.size(); q_i++) {
			auto& qfam = dev.qfams.at(q_i);
			for (const auto& qr : kv.second) {
				if (qr.dev_qfam_index == q_i) {
					// prios vector size() is the number of requests for this qfam
					qfam.prios.push_back(qr.priority);
				}
			}

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

		//VkPhysicalDeviceFeatures VkInit(physDevFeatures);
		//vkGetPhysicalDeviceFeatures(dev.phys, &physDevFeatures);
		VkPhysicalDeviceFeatures VkInit(enabledFeatures);

		// Enable device layer "VK_LAYER_LUNARG_standard_validation"
		std::vector<const char *> enabledLayers;
		enabledLayers.push_back(VK_LAYER_LUNARG_standard_validation);

		VkDeviceCreateInfo VkInit(dCreateInfo);
		dCreateInfo.queueCreateInfoCount = allQci.size();
		dCreateInfo.pQueueCreateInfos = allQci.data();
		dCreateInfo.pEnabledFeatures = &enabledFeatures;
		if (dev.extensionRequests.size()) {
			dCreateInfo.enabledExtensionCount = dev.extensionRequests.size();
			dCreateInfo.ppEnabledExtensionNames = dev.extensionRequests.data();
		}
		dCreateInfo.enabledLayerCount = enabledLayers.size();
		dCreateInfo.ppEnabledLayerNames = enabledLayers.data();

		VkResult v = vkCreateDevice(dev.phys, &dCreateInfo, nullptr /*allocator*/, &dev.dev);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "dev_i=%zu VkCreateDevice() returned %d (%s)\n", (size_t) kv.first, v, string_VkResult(v));
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
