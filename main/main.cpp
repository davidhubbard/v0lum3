/* Copyright (c) David Hubbard 2017. Licensed under the GPLv3.
 */
#define GLFW_INCLUDE_VULKAN

#include <lib/language/language.h>
#include <lib/command/command.h>
#include <lib/memory/memory.h>
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
#include "main/main.vert.h"
#include "main/main.frag.h"

#include "SkRefCnt.h"
#include "SkCanvas.h"
#include "SkData.h"
#include "SkImage.h"
#include "SkImageInfo.h"

#include <array>
#include <chrono>

// TODO: change vsync on the fly (and it must work the same at init time)
// TODO: show how to use SDL, xcb
// Use a utility class *outside* lib/language to:
//   TODO: show how to do double buffering, triple buffering
//   TODO: show how to render directly on-screen (assuming the wm allows it)
//   TODO: switch VK_PRESENT_MODE_MAILBOX_KHR on the fly
//   TODO: permit customization of the enabled instance layers.
//
// TODO: show how to do GPU compute
// TODO: passes, subpasses, secondary command buffers, and subpass dependencies
// TODO: test on android
// TODO: test on windows

const char * img_filename;

struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

const std::vector<Vertex> vertices = {
	{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
	{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
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

	int updateUniformBuffer() {
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1000.0f;
		frameCount++;
		if (time > 1.0) {
			fprintf(stderr, "%d fps\n", frameCount);
			startTime = currentTime;
			frameCount = 0;
			timeDelta++;
			timeDelta &= 3;
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
		if (uniformStagingBufferMemory.mmap(dev, &data)) {
			fprintf(stderr, "updateUniformBuffer.mmap failed\n");
			return 1;
		}
		memcpy(data, &ubo, sizeof(ubo));
		uniformStagingBufferMemory.munmap(dev);
		return copyBuffer(uniformStagingBuffer, uniformBuffer, sizeof(ubo));
	};

protected:
	int createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
			VkPtr<VkBuffer>& buffer,
			memory::DeviceMemory& bufferMemory)
	{
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		buffer.reset();
		VkResult v = vkCreateBuffer(dev.dev, &bufferInfo, dev.dev.allocator, &buffer);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "createBuffer: vkCreateBuffer failed: %d (%s)\n", v, string_VkResult(v));
			return 1;
		}

		if (bufferMemory.alloc({dev, buffer})) {
			return 1;
		}

		if ((v = vkBindBufferMemory(dev.dev, buffer, bufferMemory.vk, 0)) != VK_SUCCESS) {
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
	memory::DeviceMemory uniformStagingBufferMemory{dev, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
	VkPtr<VkBuffer> uniformBuffer{dev.dev, vkDestroyBuffer};
	memory::DeviceMemory uniformBufferMemory{dev, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};

	memory::Image textureImage{dev, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
	language::ImageView textureImageView{dev};
	VkPtr<VkSampler> textureSampler{dev.dev, vkDestroySampler};

	VkPtr<VkBuffer> vertexBuffer{dev.dev, vkDestroyBuffer};
	memory::DeviceMemory vertexBufferMemory{dev, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
	VkPtr<VkBuffer> indexBuffer{dev.dev, vkDestroyBuffer};
	memory::DeviceMemory indexBufferMemory{dev, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};

	int bindImage(
			memory::Image& image,
			VkExtent3D extent,
			VkFormat format,
			VkImageTiling tiling,
			VkImageUsageFlags usage)
	{
		image.info.extent = extent;
		image.info.format = format;
		image.info.tiling = tiling;
		image.info.usage = usage;
		if (image.ctorError(dev) || image.bindMemory(dev)) {
			return 1;
		}
		return 0;
	}

	int transitionImageLayout(
			VkImage image,
			VkFormat format,
			VkImageLayout oldLayout,
			VkImageLayout newLayout) {
		VkImageMemoryBarrier imageB = {};
		imageB.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageB.oldLayout = oldLayout;
		imageB.newLayout = newLayout;
		imageB.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageB.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageB.image = image;
		imageB.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageB.subresourceRange.baseMipLevel = 0;
		imageB.subresourceRange.levelCount = 1;
		imageB.subresourceRange.baseArrayLayer = 0;
		imageB.subresourceRange.layerCount = 1;

		if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
			imageB.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			imageB.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		} else if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			imageB.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			imageB.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			imageB.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageB.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		} else {
			fprintf(stderr, "unsupported layout transition\n");
			return 1;
		}

		command::CommandBuilder::BarrierSet bset;
		bset.img.push_back(imageB);

		command::CommandBuilder builder(cpool);
		if (builder.beginOneTimeUse() ||
				builder.barrier(bset,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) ||
				builder.end() ||
				builder.submit(0)) {
			return 1;
		}
		vkQueueWaitIdle(cpool.q(0));
		return 0;
	}

	int copyImage(
			VkImage srcImage, VkImage dstImage,
			uint32_t width, uint32_t height) {
		VkImageSubresourceLayers subResource = {};
		subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subResource.baseArrayLayer = 0;
		subResource.mipLevel = 0;
		subResource.layerCount = 1;

		VkImageCopy region = {};
		region.srcSubresource = subResource;
		region.dstSubresource = subResource;
		region.srcOffset = {0, 0, 0};
		region.dstOffset = {0, 0, 0};
		region.extent.width = width;
		region.extent.height = height;
		region.extent.depth = 1;

		command::CommandBuilder builder(cpool);
		if (builder.beginOneTimeUse() ||
				builder.copyImage(srcImage, dstImage, std::vector<VkImageCopy>{region}) ||
				builder.end() ||
				builder.submit(0)) {
			return 1;
		}
		vkQueueWaitIdle(cpool.q(0));
		return 0;
	}

	int buildUniform()
	{
		VkResult v;

		// Create uniformBuffer
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);

		if (createBuffer(bufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				uniformStagingBuffer,
				uniformStagingBufferMemory)) {
			return 1;
		}

		if (createBuffer(bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				uniformBuffer,
				uniformBufferMemory)) {
			return 1;
		}


		// Create textureImage
		// TODO: keep track of
		// https://skia.googlesource.com/skia/+/master/src/gpu/vk/GrVkImage.h
		// as an alternate way to create a textureImage.
		//
		// TODO: use SkCodec instead of SkImage

		sk_sp<SkData> data = SkData::MakeFromFileName(img_filename);
		if (!data) {
			fprintf(stderr, "   unable to read image \"%s\"\n", img_filename);
			return 1;
		}
		sk_sp<SkImage> img = SkImage::MakeFromEncoded(data);
		if (!img) {
			// TODO: get exact error message from skia.
			fprintf(stderr, "   unable to decode image \"%s\"\n", img_filename);
			return 1;
		}
		VkExtent3D extent = { 1, 1, 1 };
		extent.width = img->width();
		extent.height = img->height();

		memory::Image stagingImage(dev,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		if (bindImage(
				stagingImage,
				extent,
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_IMAGE_TILING_LINEAR,
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
			fprintf(stderr, "bindImage(stagingImage) failed\n");
			return 1;
		}

		VkImageSubresource subresource = {};
		subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresource.mipLevel = 0;
		subresource.arrayLayer = 0;

		VkSubresourceLayout stagingImageLayout;
		vkGetImageSubresourceLayout(dev.dev, stagingImage.vk, &subresource, &stagingImageLayout);

		void* mappedMem;
		if (stagingImage.mem.mmap(dev, &mappedMem)) {
			fprintf(stderr, "stagingImageMemory.mmap() failed\n");
			return 1;
		}
		SkImageInfo dstInfo = SkImageInfo::Make(
			img->width(),
			img->height(),
			kRGBA_8888_SkColorType,
			kPremul_SkAlphaType);
		if (!img->readPixels(dstInfo, mappedMem, stagingImageLayout.rowPitch, 0, 0)) {
			fprintf(stderr, "SkImage::readPixels() failed\n");
			stagingImage.mem.munmap(dev);
			return 1;
		}
		stagingImage.mem.munmap(dev);

		if (bindImage(
				textureImage,
				extent,
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) {
			fprintf(stderr, "bindImage(textureImage) failed\n");
			return 1;
		}

		transitionImageLayout(stagingImage.vk, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		transitionImageLayout(textureImage.vk, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		copyImage(stagingImage.vk, textureImage.vk, extent.width, extent.height);

		transitionImageLayout(textureImage.vk, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		if (textureImageView.ctorError(dev, textureImage.vk, VK_FORMAT_R8G8B8A8_UNORM))
		{
			return 1;
		}

		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = 16;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		textureSampler.reset();
		if (vkCreateSampler(dev.dev, &samplerInfo, dev.dev.allocator, &textureSampler) != VK_SUCCESS) {
			fprintf(stderr, "failed to create texture sampler!\n");
			return 1;
		}


		// Create descriptorSetLayout
		VkDescriptorSetLayoutBinding uboLayoutBinding = {};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.pImmutableSamplers = nullptr;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		std::array<VkDescriptorSetLayoutBinding, 2> bindArray
			= {uboLayoutBinding, samplerLayoutBinding};
		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = bindArray.size();
		layoutInfo.pBindings = bindArray.data();

		descriptorSetLayout.reset();
		if ((v = vkCreateDescriptorSetLayout(
				dev.dev,
				&layoutInfo,
				dev.dev.allocator,
				&descriptorSetLayout)) != VK_SUCCESS) {
			fprintf(stderr, "vkCreateDescriptorSetLayout failed: %d (%s)\n", v, string_VkResult(v));
			return 1;
		}

		// Create descriptorPool
		std::array<VkDescriptorPoolSize, 2> poolSizes = {};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = 1;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = poolSizes.size();
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = 1;

		descriptorPool.reset();
		if ((v = vkCreateDescriptorPool(dev.dev, &poolInfo, dev.dev.allocator, &descriptorPool)) != VK_SUCCESS) {
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

		// Create descriptorWrites
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = uniformBuffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSet;
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;

		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = textureImageView.vk;
		imageInfo.sampler = textureSampler;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSet;
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(dev.dev, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);


		// Create vertexBuffer using stagingBuffer
		bufferSize = sizeof(vertices[0]) * vertices.size();

		VkPtr<VkBuffer> stagingBuffer{dev.dev, vkDestroyBuffer};
		memory::DeviceMemory stagingBufferMemory{dev, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
		if (createBuffer(bufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				stagingBuffer,
				stagingBufferMemory)) {
			return 1;
		}

		if (stagingBufferMemory.mmap(dev, &mappedMem)) {
			fprintf(stderr, "stagingBufferMemory(vertex).mmap() failed\n");
			return 1;
		}
		memcpy(mappedMem, vertices.data(), (size_t) bufferSize);
		stagingBufferMemory.munmap(dev);

		if (createBuffer(bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				vertexBuffer,
				vertexBufferMemory)) {
			return 1;
		}

		if (copyBuffer(stagingBuffer, vertexBuffer, (size_t) bufferSize)) {
			return 1;
		}

		// Create indexBuffer using stagingBuffer
		bufferSize = sizeof(indices[0]) * indices.size();

		if (createBuffer(bufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				stagingBuffer,
				stagingBufferMemory)) {
			return 1;
		}

		if (stagingBufferMemory.mmap(dev, &mappedMem)) {
			fprintf(stderr, "stagingBufferMemory(indices).mmap() failed\n");
			return 1;
		}
		memcpy(mappedMem, indices.data(), (size_t) bufferSize);
		stagingBufferMemory.munmap(dev);

		if (createBuffer(bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
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
				.loadSPV(spv_main_vert, sizeof(spv_main_vert))) {
			return 1;
		}
		if (pipe0params.addShader(VK_SHADER_STAGE_FRAGMENT_BIT, "main")
				.loadSPV(spv_main_frag, sizeof(spv_main_frag))) {
			return 1;
		}

		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();

		pipe0params.vertsci.vertexBindingDescriptionCount = 1;
		pipe0params.vertsci.vertexAttributeDescriptionCount = attributeDescriptions.size();
		pipe0params.vertsci.pVertexBindingDescriptions = &bindingDescription;
		pipe0params.vertsci.pVertexAttributeDescriptions = attributeDescriptions.data();

		pipe0params.setLayouts.push_back(descriptorSetLayout);

		fprintf(stderr, "renderPass.init: "
			"main.vert.spv (0x%zx bytes) main.frag.spv (0x%zx bytes)"
			" wxh=%dx%d\n",
			sizeof(spv_main_vert),
			sizeof(spv_main_frag),
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
		if (simple.updateUniformBuffer()) {
			return 1;
		}

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
	fprintf(stderr, "require %d extensions\n", glfwExtensionCount);
	for (unsigned i = 0; i < glfwExtensionCount; i++)
		fprintf(stderr, "  \"%s\"\n", glfwExtensions[i]);
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

static void printGLFWerr(int code, const char * msg) {
	fprintf(stderr, "glfw error %x: %s\n", code, msg);
}

static int runGLFW() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	VkExtent2D size{800, 600};
	GLFWwindow * window = glfwCreateWindow(size.width, size.height,
		"Vulkan window", nullptr, nullptr);
	glfwSetErrorCallback(printGLFWerr);
	int r = runLanguage(window, size);
	glfwDestroyWindow(window);
	glfwTerminate();
	return r;
}

int main(int argc, char ** argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s filename\n", argv[0]);
		return 1;
	}
	img_filename = argv[1];
	return runGLFW();
}
