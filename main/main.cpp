/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#define GLFW_INCLUDE_VULKAN

#include <lib/command/command.h>
#include <GLFW/glfw3.h>
#include <lib/language/VkPtr.h>
//#include <lib/language/VkEnum.h>
// TODO: VkInit.h probably not needed anymore.
#include <lib/language/VkInit.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include "objsyms.h"

const int WIN_W = 800;
const int WIN_H = 600;

void hexdump(const char * start, const char * end) {
	auto* p = start;
	size_t line_ofs = 0;
	size_t ofs = (size_t) p;
	for (; p < end; p++, ofs++) {
		if ((ofs & ~15) != line_ofs) {
			line_ofs = ofs & ~15;
			fprintf(stderr, "%08zx:", line_ofs);
			while (line_ofs < ofs) fprintf(stderr, "   ");
		}
		fprintf(stderr, " %02x", (unsigned) (unsigned char) *p);
		if ((ofs & 15) == 15) fprintf(stderr, "\n");
	}
	if ((ofs & 15) != 0) fprintf(stderr, "\n");
}

static int mainLoop(GLFWwindow * window, language::Instance& inst) {
	if (!inst.devs_size()) {
		fprintf(stderr, "BUG: no devices created\n");
		return 1;
	}
	language::Device& dev = inst.at(0);
	command::RenderPass renderPass(dev);
	command::PipelineCreateInfo pipe0params(dev);

	if (renderPass.addShader(pipe0params, VK_SHADER_STAGE_VERTEX_BIT, "main")
		.loadSPV(
			_binary_main_vert_spv_start, _binary_main_vert_spv_end)) {
		return 1;
	}
	if (renderPass.addShader(pipe0params, VK_SHADER_STAGE_FRAGMENT_BIT, "main")
		.loadSPV(
			_binary_main_frag_spv_start, _binary_main_frag_spv_end)) {
		return 1;
	}

	fprintf(stderr, "renderPass.init: main.vert.spv (0x%zx bytes) main.frag.spv (0x%zx bytes)\n",
		(size_t) (_binary_main_vert_spv_end - _binary_main_vert_spv_start),
		(size_t) (_binary_main_frag_spv_end - _binary_main_frag_spv_start));

	renderPass.pipelines.emplace_back(dev);
	if (renderPass.init(std::vector<command::PipelineCreateInfo>{pipe0params})) {
		return 1;
	}

	auto present_i = dev.getQfamI(language::PRESENT);
	if (present_i == (size_t) -1) {
		return 1;
	}
	auto graphics_i = dev.getQfamI(language::GRAPHICS);
	if (graphics_i == (size_t) -1) {
		return 1;
	}
	command::CommandPool cpool(dev, graphics_i);
	if (cpool.ctor(dev, 0)) {
		return 1;
	}
	std::vector<VkCommandBuffer> commandBuffers;
	commandBuffers.resize(dev.framebufs.size());
	VkCommandBufferAllocateInfo allocInfo;
	memset(&allocInfo, 0, sizeof(allocInfo));
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = cpool.vk;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

	if (vkAllocateCommandBuffers(dev.dev, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
		fprintf(stderr, "vkAllocateCommandBuffers returned error\n");
		return 1;
	}

	for (size_t i = 0; i < commandBuffers.size(); i++) {
		VkCommandBufferBeginInfo beginInfo;
		memset(&beginInfo, 0, sizeof(beginInfo));
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		if (vkBeginCommandBuffer(commandBuffers.at(i), &beginInfo) != VK_SUCCESS) {
			fprintf(stderr, "vkBeginCommandBuffer returned error\n");
			return 1;
		}

		VkRenderPassBeginInfo renderPassInfo;
		memset(&renderPassInfo, 0, sizeof(renderPassInfo));
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass.vk;
		renderPassInfo.framebuffer = dev.framebufs.at(i).vk;
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = dev.swapchainExtent;

		VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffers.at(i), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffers.at(i), VK_PIPELINE_BIND_POINT_GRAPHICS, renderPass.pipelines.at(0).vk);

		vkCmdDraw(commandBuffers.at(i), 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffers.at(i));

		if (vkEndCommandBuffer(commandBuffers.at(i)) != VK_SUCCESS) {
			fprintf(stderr, "vkEndCommandBuffer returned error\n");
			return 1;
		}
	}

	command::Semaphore imageAvailableSemaphore(dev);
	if (imageAvailableSemaphore.ctor(dev)) {
		return 1;
	}
	command::Semaphore renderFinishedSemaphore(dev);
	if (renderFinishedSemaphore.ctor(dev)) {
		return 1;
	}

	fprintf(stderr, "start main loop\n");
	int count = 0;
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		fprintf(stderr, "vkAcquireNextImageKHR\n");
		uint32_t next_image_i;
    if (vkAcquireNextImageKHR(dev.dev, dev.swapchain, std::numeric_limits<uint64_t>::max(),
				imageAvailableSemaphore.vk, VK_NULL_HANDLE, &next_image_i) != VK_SUCCESS) {
			fprintf(stderr, "vkAcquireNextImageKHR returned error\n");
			return 1;
		}
		VkSubmitInfo submitInfo;
		memset(&submitInfo, 0, sizeof(submitInfo));
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = {imageAvailableSemaphore.vk};
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[next_image_i];

		VkSemaphore signalSemaphores[] = {renderFinishedSemaphore.vk};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		fprintf(stderr, "vkQueueSubmit\n");
		VkResult v = vkQueueSubmit(dev.qfams.at(graphics_i).queues.at(0), 1, &submitInfo, VK_NULL_HANDLE);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "vkQueueSubmit returned %d\n", v);
			return 1;
		}

		VkSwapchainKHR swapChains[] = {dev.swapchain};
		VkPresentInfoKHR VkInit(presentInfo);
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &next_image_i;

		fprintf(stderr, "vkQueuePresentKHR\n");
		v = vkQueuePresentKHR(dev.qfams.at(present_i).queues.at(0), &presentInfo);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "vkQueuePresentKHR returned error\n");
			return 1;
		}
		fprintf(stderr, "vkQueuePresentKHR done, end of loop %d\n", count);
		//
		// Probability of vkDeviceWaitIdle() failing with error:
		// "W ParameterValidation: code9: vkDeviceWaitIdle: returned VK_ERROR_DEVICE_LOST, indicating
		// that the logical device has been lost"
		// "vkDeviceWaitIdle returned -4"
		// 0 <= count <= 664: 0%
		// 0 <= count <= 665: 5%
		// 0 <= count <= 666: 100%
		//
		count++;
		if (count > 666) break;
	}

	fprintf(stderr, "vkDeviceWaitIdle\n");
	VkResult v = vkDeviceWaitIdle(dev.dev);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkDeviceWaitIdle returned %d\n", v);
		return 1;
	}
	return 0;
}

static int runLanguage(GLFWwindow * window) {
	unsigned int glfwExtensionCount = 0;
	const char ** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	language::Instance inst;
	if (inst.ctorError(glfwExtensions, glfwExtensionCount)) {
		return 1;
	}

	VkResult v = glfwCreateWindowSurface(inst.vk, window, nullptr /*allocator*/,
		&inst.surface);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "glfwCreateWindowSurface() returned %d\n", v);
		return 1;
	}

	int r = inst.open({WIN_W, WIN_H},
	[&](std::vector<language::QueueRequest>& request) -> int {
		// Search for a single device that can do both PRESENT and GRAPHICS.
		bool foundPRESENTdev = false;
		for (size_t dev_i = 0; dev_i < inst.devs_size(); dev_i++) {
			auto selectedQfams =
				inst.requestQfams(dev_i, {language::PRESENT, language::GRAPHICS});
			foundPRESENTdev |= selectedQfams.size() > 0;
			request.insert(request.end(),
				selectedQfams.begin(), selectedQfams.end());
		}
		if (!foundPRESENTdev) {
			fprintf(stderr, "Error: no device has both PRESENT and GRAPHICS queues.\n");
			return 1;
		}
		return 0;
	});
	if (r) {
		return r;
	}
	return mainLoop(window, inst);
}

static int runGLFW() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow * window = glfwCreateWindow(WIN_W, WIN_H, "Vulkan window", nullptr, nullptr);
	int r = runLanguage(window);
	glfwDestroyWindow(window);
	glfwTerminate();
	return r;
}

int main(int argc, char ** argv) {
	(void) argc;
	(void) argv;
	return runGLFW();
}