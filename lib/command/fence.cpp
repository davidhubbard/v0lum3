/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#include "command.h"

namespace command {

int Semaphore::ctorError(language::Device& dev) {
	VkSemaphoreCreateInfo VkInit(sci);
	VkResult v = vkCreateSemaphore(dev.dev, &sci, nullptr, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateSemaphore returned %d (%s)\n", v, string_VkResult(v));
		return 1;
	}
	return 0;
}

int Fence::ctorError(language::Device& dev) {
	VkFenceCreateInfo VkInit(fci);
	VkResult v = vkCreateFence(dev.dev, &fci, nullptr, &vk);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkCreateFence returned %d (%s)\n", v, string_VkResult(v));
		return 1;
	}
	return 0;
}

int PresentSemaphore::present(uint32_t image_i) {
	VkSemaphore semaphores[] = { vk };
	VkSwapchainKHR swapChains[] = { dev.swapChain };

	VkPresentInfoKHR VkInit(presentInfo);
	presentInfo.waitSemaphoreCount = sizeof(semaphores)/sizeof(semaphores[0]);
	presentInfo.pWaitSemaphores = semaphores;
	presentInfo.swapchainCount = sizeof(swapChains)/sizeof(swapChains[0]);
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &image_i;

	VkResult v = vkQueuePresentKHR(q, &presentInfo);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkQueuePresentKHR returned %d (%s)\n", v, string_VkResult(v));
		return 1;
	}
	return 0;
};

}  // namespace command
