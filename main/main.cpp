/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 */
#define GLFW_INCLUDE_VULKAN

#include <lib/command/command.h>
#include <GLFW/glfw3.h>
#include <lib/language/VkPtr.h>
#include <lib/language/VkInit.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "objsyms.h"

#include <array>
#include <chrono>

struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		return attributeDescriptions;
	}
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0
};

class SimplePipeline {
public:
	language::Device& dev;
	VkSurfaceKHR surface;
	command::RenderPass * pass;
	command::CommandPool cpool;
	command::CommandBuilder builder;

	SimplePipeline(language::Instance& inst, language::SurfaceSupport queueFamily)
		: dev(inst.at(0))
		, surface(inst.surface)
		, pass(nullptr)
		, cpool(dev, queueFamily)
		, builder(cpool)
	{
		startTime = std::chrono::high_resolution_clock::now();
	};

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

		if (buildUniform()) {
			return 1;
		}

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

	std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
	unsigned frameCount = 0;
	int timeDelta = 0;

	void updateUniformBuffer() {
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;
		frameCount++;
		if (time > 1.0) {
			fprintf(stderr, "%d fps\n", frameCount);
			startTime = currentTime;
			frameCount = 0;
			timeDelta++;
			if (timeDelta > 3) {
				timeDelta = 0;
			}
		}
		time += timeDelta;

		UniformBufferObject ubo = {};
		ubo.model = glm::rotate(glm::mat4(), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), dev.aspectRatio(), 0.1f, 10.0f);

		// convert from OpenGL where clip coordinates +Y is up
		// to Vulkan where clip coordinates +Y is down. The other OpenGL/Vulkan
		// coordinate change is GLM_FORCE_DEPTH_ZERO_TO_ONE. For more information:
		// https://github.com/LunarG/VulkanSamples/commit/0dd3617
		// https://forums.khronos.org/showthread.php/13152-Understand-Vulkan-Clipping
		// https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
		// https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#vertexpostproc-clipping
		ubo.proj[1][1] *= -1;

		void* data;
		vkMapMemory(dev.dev, uniformStagingBufferMemory, 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(dev.dev, uniformStagingBufferMemory);

		copyBuffer(uniformStagingBuffer, uniformBuffer, sizeof(ubo));
	};

protected:
	int findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags flags) {
		for (uint32_t i = 0; i < dev.memProps.memoryTypeCount; i++) {
			if ((typeBits & (1 << i)) &&
					(dev.memProps.memoryTypes[i].propertyFlags & flags) == flags) {
				return i;
			}
		}

		fprintf(stderr, "findMemoryType(%x, %x): not found\n", typeBits, flags);
		return -1;
	}

	int createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
			VkMemoryPropertyFlags properties, VkPtr<VkBuffer>& buffer,
			VkPtr<VkDeviceMemory>& bufferMemory)
	{
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		buffer.reset();
		VkResult v = vkCreateBuffer(dev.dev, &bufferInfo, nullptr, &buffer);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "createBuffer: vkCreateBuffer failed: %d (%s)\n", v, string_VkResult(v));
			return 1;
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(dev.dev, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		auto memTypeI = findMemoryType(memRequirements.memoryTypeBits, properties);
		if (memTypeI == -1) {
			return 1;
		}
		allocInfo.memoryTypeIndex = memTypeI;

		bufferMemory.reset();
		if ((v = vkAllocateMemory(dev.dev, &allocInfo, nullptr, &bufferMemory)) != VK_SUCCESS) {
			fprintf(stderr, "createBuffer: vkAllocateMemory failed: %d (%s)\n", v, string_VkResult(v));
			return 1;
		}

		if ((v = vkBindBufferMemory(dev.dev, buffer, bufferMemory, 0)) != VK_SUCCESS) {
			fprintf(stderr, "createBuffer: vkBindBufferMemory failed: %d (%s)\n", v, string_VkResult(v));
			return 1;
		}
		return 0;
	}

	int copyBuffer(VkBuffer src, VkBuffer dst, size_t size) {
		command::CommandBuilder builder(cpool);
		if (builder.beginOneTimeUse() ||
				builder.copyBuffer(src, dst, size) ||
				builder.end() ||
				builder.submit(0)) {
			return 1;
		}
		vkQueueWaitIdle(cpool.q(0));
		return 0;
	}

	VkPtr<VkDescriptorPool> descriptorPool{dev.dev, vkDestroyDescriptorPool};
	VkPtr<VkDescriptorSetLayout> descriptorSetLayout{dev.dev, vkDestroyDescriptorSetLayout};
	VkDescriptorSet descriptorSet;

	VkPtr<VkBuffer> uniformStagingBuffer{dev.dev, vkDestroyBuffer};
	VkPtr<VkDeviceMemory> uniformStagingBufferMemory{dev.dev, vkFreeMemory};
	VkPtr<VkBuffer> uniformBuffer{dev.dev, vkDestroyBuffer};
	VkPtr<VkDeviceMemory> uniformBufferMemory{dev.dev, vkFreeMemory};

	VkPtr<VkBuffer> vertexBuffer{dev.dev, vkDestroyBuffer};
	VkPtr<VkDeviceMemory> vertexBufferMemory{dev.dev, vkFreeMemory};
	VkPtr<VkBuffer> indexBuffer{dev.dev, vkDestroyBuffer};
	VkPtr<VkDeviceMemory> indexBufferMemory{dev.dev, vkFreeMemory};

	int buildUniform()
	{
		// Create descriptorSetLayout
		VkDescriptorSetLayoutBinding uboLayoutBinding = {};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.pImmutableSamplers = nullptr;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &uboLayoutBinding;

		descriptorSetLayout.reset();
		VkResult v = vkCreateDescriptorSetLayout(dev.dev, &layoutInfo, nullptr,
				&descriptorSetLayout);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "vkCreateDescriptorSetLayout failed: %d (%s)\n", v, string_VkResult(v));
			return 1;
		}

		// Create uniformBuffer
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);

		if (createBuffer(bufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				uniformStagingBuffer,
				uniformStagingBufferMemory)) {
			return 1;
		}

		if (createBuffer(bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				uniformBuffer,
				uniformBufferMemory)) {
			return 1;
		}

		// Create descriptorPool
		VkDescriptorPoolSize poolSize = {};
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = 1;

		descriptorPool.reset();
		if ((v = vkCreateDescriptorPool(dev.dev, &poolInfo, nullptr, &descriptorPool)) != VK_SUCCESS) {
			fprintf(stderr, "vkCreateDescriptorPool failed: %d (%s)\n", v, string_VkResult(v));
			return 1;
		}

		// Create descriptorSet
		VkDescriptorSetLayout layouts[] = {descriptorSetLayout};
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = layouts;

		if ((v = vkAllocateDescriptorSets(dev.dev, &allocInfo, &descriptorSet)) != VK_SUCCESS) {
			fprintf(stderr, "vkAllocateDescriptorSets failed: %d (%s)\n", v, string_VkResult(v));
			return 1;
		}

		// Create descriptorWrite
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = uniformBuffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkWriteDescriptorSet descriptorWrite = {};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSet;
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(dev.dev, 1, &descriptorWrite, 0, nullptr);


		// Create vertexBuffer using stagingBuffer
		bufferSize = sizeof(vertices[0]) * vertices.size();

		VkPtr<VkBuffer> stagingBuffer{dev.dev, vkDestroyBuffer};
		VkPtr<VkDeviceMemory> stagingBufferMemory{dev.dev, vkFreeMemory};
		if (createBuffer(bufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				stagingBuffer,
				stagingBufferMemory)) {
			return 1;
		}

		void* data;
		if ((v = vkMapMemory(dev.dev, stagingBufferMemory, 0, bufferSize, 0, &data)) != VK_SUCCESS) {
			fprintf(stderr, "vkMapMemory(vertex) failed: %d (%s)\n", v, string_VkResult(v));
			return 1;
		}
		memcpy(data, vertices.data(), (size_t) bufferSize);
		vkUnmapMemory(dev.dev, stagingBufferMemory);

		if (createBuffer(bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				vertexBuffer,
				vertexBufferMemory)) {
			return 1;
		}

		if (copyBuffer(stagingBuffer, vertexBuffer, (size_t) bufferSize)) {
			return 1;
		}

		// Create indexBuffer using stagingBuffer
		bufferSize = sizeof(indices[0]) * indices.size();

		stagingBufferMemory.reset();
		stagingBuffer.reset();
		if (createBuffer(bufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				stagingBuffer,
				stagingBufferMemory)) {
			return 1;
		}

		if ((v = vkMapMemory(dev.dev, stagingBufferMemory, 0, bufferSize, 0, &data)) != VK_SUCCESS) {
			return 1;
		}
		memcpy(data, indices.data(), (size_t) bufferSize);
		vkUnmapMemory(dev.dev, stagingBufferMemory);

		if (createBuffer(bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				indexBuffer,
				indexBufferMemory)) {
			return 1;
		}

		if (copyBuffer(stagingBuffer, indexBuffer, (size_t) bufferSize)) {
			return 1;
		}
		return 0;
	}

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

		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();

		pipe0params.vertsci.vertexBindingDescriptionCount = 1;
		pipe0params.vertsci.vertexAttributeDescriptionCount = attributeDescriptions.size();
		pipe0params.vertsci.pVertexBindingDescriptions = &bindingDescription;
		pipe0params.vertsci.pVertexAttributeDescriptions = attributeDescriptions.data();

		pipe0params.setLayouts.push_back(descriptorSetLayout);

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

		if (builder.resize(dev.framebufs.size())) {
			return 1;
		}

		for (size_t i = 0; i < dev.framebufs.size(); i++) {
			builder.use(i);

			pass->passBeginInfo.framebuffer = dev.framebufs.at(i).vk;
			pass->passBeginInfo.renderArea.extent = dev.swapChainExtent;

			VkBuffer vertexBuffers[] = {vertexBuffer};
			VkDeviceSize offsets[] = {0};

			if (builder.beginSimultaneousUse() ||
					builder.beginPrimaryPass(*pass) ||
					builder.bindGraphicsPipelineAndDescriptors(pass->pipelines.at(0),
						0, 1, &descriptorSet) ||
					builder.bindVertexBuffers(0,
						sizeof(vertexBuffers)/sizeof(vertexBuffers[0]), vertexBuffers,
						offsets) ||
					builder.bindAndDraw(indices, indexBuffer, 0 /*indexBufOffset*/) ||
					builder.draw(3, 1, 0, 0) ||
					builder.endRenderPass() ||
					builder.end()) {
				return 1;
			}
		}
		return 0;
	};
};

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
		simple.updateUniformBuffer();

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
		submitInfo.pCommandBuffers = &simple.builder.bufs.at(next_image_i);

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

static int runLanguage(GLFWwindow * window, VkExtent2D size) {
	unsigned int glfwExtensionCount = 0;
	const char ** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	language::Instance inst;
	if (inst.ctorError(glfwExtensions, glfwExtensionCount,
			createWindowSurface, window)) {
		return 1;
	}
	int r = inst.open(size);
	if (r) {
		return r;
	}
	return mainLoop(window, inst);
}

static int runGLFW() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	VkExtent2D size{800, 600};
	GLFWwindow * window = glfwCreateWindow(size.width, size.height,
		"Vulkan window", nullptr, nullptr);
	int r = runLanguage(window, size);
	glfwDestroyWindow(window);
	glfwTerminate();
	return r;
}

int main(int argc, char ** argv) {
	(void) argc;
	(void) argv;
	return runGLFW();
}
