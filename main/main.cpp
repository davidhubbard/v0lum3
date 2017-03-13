/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#define GLFW_INCLUDE_VULKAN

#include <lib/command/command.h>
#include <GLFW/glfw3.h>
#include <lib/language/VkPtr.h>
#include <lib/language/VkInit.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include "objsyms.h"

class SimplePipeline {
public:
	language::Device& dev;
	VkSurfaceKHR surface;
	command::RenderPass * pass;
	command::CommandPool cpool;
	std::vector<VkCommandBuffer> buf;

	SimplePipeline(language::Instance& inst, language::SurfaceSupport queueFamily)
		: dev(inst.at(0))
		, surface(inst.surface)
		, pass(nullptr)
		, cpool(dev, queueFamily) {};

	~SimplePipeline() {
		delete pass;
		pass = nullptr;
	};

	int ctorError(GLFWwindow * window) {
		if (cpool.ctorError(dev, 0)) {
			return 1;
		}
		glfwSetWindowUserPointer(window, this);
		glfwSetWindowSizeCallback(window, windowResized);
		return build();
	};

	static void windowResized(GLFWwindow * window, int glfw_w, int glfw_h) {
		if (glfw_w == 0 || glfw_h == 0) {
			return;
		}
		uint32_t width = glfw_w, height = glfw_h;
		SimplePipeline * self = (SimplePipeline *) glfwGetWindowUserPointer(window);
		VkResult v = vkDeviceWaitIdle(self->dev.dev);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "vkDeviceWaitIdle in resize: %d (%s)\n", v, string_VkResult(v));
		}
		if (self->dev.resetSwapChain(self->surface, VkExtent2D{width, height})) {
			return;
		}
		if (self->build()) {
			return;
		}
	};

protected:
	int build()
	{
		if (pass) delete pass;
		pass = new command::RenderPass(dev);

		command::PipelineCreateInfo pipe0params(dev, *pass);
		if (pipe0params.addShader(VK_SHADER_STAGE_VERTEX_BIT, "main")
				.loadSPV(obj_main_vert_spv_start, obj_main_vert_spv_end)) {
			return 1;
		}
		if (pipe0params.addShader(VK_SHADER_STAGE_FRAGMENT_BIT, "main")
				.loadSPV(obj_main_frag_spv_start, obj_main_frag_spv_end)) {
			return 1;
		}

		if (0) fprintf(stderr, "renderPass.init: "
			"main.vert.spv (0x%zx bytes) main.frag.spv (0x%zx bytes)"
			" wxh=%dx%d\n",
			(size_t) (obj_main_vert_spv_end - obj_main_vert_spv_start),
			(size_t) (obj_main_frag_spv_end - obj_main_frag_spv_start),
			dev.swapChainExtent.width, dev.swapChainExtent.height);

		pass->pipelines.emplace_back(dev);
		if (pass->init(std::vector<command::PipelineCreateInfo>{pipe0params})) {
			return 1;
		}

		if (buf.size() > 0) {
			vkFreeCommandBuffers(dev.dev, cpool.vk, buf.size(), buf.data());
		}

		buf.resize(dev.framebufs.size());
		VkCommandBufferAllocateInfo allocInfo;
		memset(&allocInfo, 0, sizeof(allocInfo));
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = cpool.vk;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t) buf.size();

		if (vkAllocateCommandBuffers(dev.dev, &allocInfo, buf.data()) != VK_SUCCESS) {
			fprintf(stderr, "vkAllocateCommandBuffers returned error\n");
			return 1;
		}

		for (size_t i = 0; i < buf.size(); i++) {
			VkCommandBufferBeginInfo beginInfo;
			memset(&beginInfo, 0, sizeof(beginInfo));
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			if (vkBeginCommandBuffer(buf.at(i), &beginInfo) != VK_SUCCESS) {
				fprintf(stderr, "vkBeginCommandBuffer returned error\n");
				return 1;
			}

			VkRenderPassBeginInfo renderPassInfo;
			memset(&renderPassInfo, 0, sizeof(renderPassInfo));
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = pass->vk;
			renderPassInfo.framebuffer = dev.framebufs.at(i).vk;
			renderPassInfo.renderArea.offset = {0, 0};
			renderPassInfo.renderArea.extent = dev.swapChainExtent;

			VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			vkCmdBeginRenderPass(buf.at(i), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(buf.at(i), VK_PIPELINE_BIND_POINT_GRAPHICS,
				pass->pipelines.at(0).vk);

			vkCmdDraw(buf.at(i), 3, 1, 0, 0);

			vkCmdEndRenderPass(buf.at(i));

			if (vkEndCommandBuffer(buf.at(i)) != VK_SUCCESS) {
				fprintf(stderr, "vkEndCommandBuffer returned error\n");
				return 1;
			}
		}
		return 0;
	};
};

const int WIN_W = 800;
const int WIN_H = 600;

static int mainLoop(GLFWwindow * window, language::Instance& inst) {
	if (!inst.devs_size()) {
		fprintf(stderr, "BUG: no devices created\n");
		return 1;
	}
	SimplePipeline simple(inst, language::GRAPHICS);
	if (simple.ctorError(window)) {
		return 1;
	}

	command::Semaphore imageAvailableSemaphore(simple.dev);
	if (imageAvailableSemaphore.ctorError(simple.dev)) {
		return 1;
	}
	command::PresentSemaphore renderSemaphore(simple.dev);
	if (renderSemaphore.ctorError()) {
		return 1;
	}

	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		uint32_t next_image_i;
		if (vkAcquireNextImageKHR(simple.dev.dev, simple.dev.swapChain,
				std::numeric_limits<uint64_t>::max(),
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
		submitInfo.pCommandBuffers = &simple.buf.at(next_image_i);

		VkSemaphore signalSemaphores[] = {renderSemaphore.vk};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		VkResult v = vkQueueSubmit(simple.cpool.q(0), 1, &submitInfo, VK_NULL_HANDLE);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "vkQueueSubmit returned %d\n", v);
			return 1;
		}

		if (renderSemaphore.present(next_image_i)) {
			return 1;
		}
	}

	VkResult v = vkDeviceWaitIdle(simple.dev.dev);
	if (v != VK_SUCCESS) {
		fprintf(stderr, "vkDeviceWaitIdle returned %d\n", v);
		return 1;
	}
	return 0;
}

// Wrap glfwCreateWindowSurface for type safety.
static VkResult createWindowSurface(language::Instance& inst, void * window) {
	return glfwCreateWindowSurface(inst.vk, (GLFWwindow *) window,
		inst.pAllocator, &inst.surface);
}

static int runLanguage(GLFWwindow * window) {
	unsigned int glfwExtensionCount = 0;
	const char ** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	language::Instance inst;
	if (inst.ctorError(glfwExtensions, glfwExtensionCount,
			createWindowSurface, window)) {
		return 1;
	}
	int r = inst.open({WIN_W, WIN_H});
	if (r) {
		return r;
	}
	return mainLoop(window, inst);
}

static int runGLFW() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
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
