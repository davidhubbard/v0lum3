/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 *
 * lib/command is the 3rd-level bindings for the Vulkan graphics library.
 * lib/command is part of the v0lum3 project.
 * This library is called "command" as a homage to Star Trek First Contact.
 * Like the Vulcan High Command, this library sends out the commands.
 */

#include <lib/language/language.h>
#include <lib/language/VkInit.h>
#include <string>

#pragma once

namespace command {

// Shader represents a "dumb buffer" containing the output of glslangVerifier.
// To add a Shader to a Pipeline:
// 1. Call RenderPass::addShader(), observing the warning that the Shader&
//    reference becoming invalid. addShader() takes care of the steps below.
//
// Or the more manual process:
// 1. Call RenderPass::shaders.emplace_back(dev).
// 2. Manipulate *(RenderPass::shaders.end() - 1), such as by calling loadSPV().
// 3. Create at least one PipelineStage that refers to the index of the shader
//    in RenderPass::shaders. Set PipelineStage.entryPointName to the "main()"
//    entrypoint in the shader.
// 4. Call PipelineCreateInfo::stages.push_back(PipelineStage) to add the
//    PipelineStage to the pipeline.
//
// Shader is stored on Device dev, so it should be kept tightly bound to the
// Pipeline under construction.
typedef struct Shader {
	Shader(language::Device& dev_)
		: dev(&dev_)
		, vk{dev_.dev, vkDestroyShaderModule}
	{
		vk.allocator = dev_.dev.allocator;
	}
	Shader(Shader&&) = default;
	Shader(const Shader& other) = delete;

	WARN_UNUSED_RESULT int loadSPV(const void * spvBegin, const void * spvEnd);
	WARN_UNUSED_RESULT int loadSPV(const std::vector<char>& spv) {
		return loadSPV(&spv.begin()[0], &spv.end()[0]);
	}
	WARN_UNUSED_RESULT int loadSPV(const char * filename);
	WARN_UNUSED_RESULT int loadSPV(std::string filename) {
		return loadSPV(filename.c_str());
	}

	language::Device* dev;
	VkPtr<VkShaderModule> vk;
} Shader;

// PipelineStage is the entrypoint to run a Shader as one of the programmable
// pipeline stages. (See the description of Pipeline, below.)
//
// PipelineStage::entryPoint sets what function is "main()" in the Shader.
// A library of useful code can be built as a single large shader with several
// entryPointNames -- glslangVerifier can build many source files as one unit.
//
// Or, keep it simple: set entryPointName = "main" on all your shaders to make
// them feel like "C".
//
// TODO: Find out if Vulkan errors out if two PipelineStages are added for the
// same stage. For example, two VK_SHADER_STAGE_VERTEX_BIT.
typedef struct PipelineStage {
	PipelineStage() { VkOverwrite(sci); };
	PipelineStage(PipelineStage&&) = default;
	PipelineStage(const PipelineStage&) = default;

	size_t shader_i;
	std::string entryPointName;

	// Write code to initialize sci.flag, but do not initialize
	// sci.module and sci.pName. They will be written by Pipeline::init().
	VkPipelineShaderStageCreateInfo sci;
} PipelineStage;

// Forward declaration of RenderPass for Pipeline and PipelineCreateInfo.
struct RenderPass;

// Vulkan defines a render pipeline in terms of the following stages:
// 1. Input assembly: fixed function, reads input data.
// 2. Vertex shader: programmable, operates on input vertices, uniforms, etc.
// 3. Tesselation shader: programmable, reads the vertex shader's output and
//    produces a different number of vertices.
// 4. Geometry shader: programmable. Most GPUs cannot do geometry shading with
//    reasonable performance (the notable exception is Intel).
// 5. Rasterizer: fixed function, draws triangles / lines / points.
// 6. Fragment shader: programmable, operates on each "fragment" (each pixel).
// 7. Color blend: fixed function, writes fragments to the frame buffer.
//
// Use Pipeline in the following order:
// 1. Instantiate a Pipeline with a Device dev.
// 2. Instantiate a PipelineCreateInfo which is used to create the Pipeline,
//    and then deleted.
// 3. Call Pipeline.shaders.emplace_back(dev) -- yes, same dev.
//    Add the shaders to Pipeline, and add the PipelineStage instances to
//    PipelineCreateInfo.stages.
// 4. Make any other desired changes to PipelineCreateInfo.
// 5. Call Pipeline.init(PipelineCreateInfo).
typedef struct PipelineCreateInfo {
	PipelineCreateInfo(language::Device& dev, RenderPass& renderPass);
	PipelineCreateInfo(PipelineCreateInfo&&) = default;
	PipelineCreateInfo(const PipelineCreateInfo& other) = default;

	language::Device& dev;
	RenderPass& renderPass;
	std::vector<PipelineStage> stages;

	// Helper function to instantiate a Shader, add a PipelineStage to stages,
	// add a link to the shader in renderPass, then return a reference to it.
	//
	// To use the Shader for more than one Pipeline: call addShader() on
	// the first pipeline, then make copies of *(stages.end() - 1),
	// by calling pipelineCreateInfo2.stages.emplace_back(
	//   *(pipelineCreateInfo1.stages.end() - 1));
	//
	// WARNING: Returned reference Shader& is invalidated by the next
	// operation on the shaders vector.
	Shader& addShader(VkShaderStageFlagBits stageBits,
		std::string entryPointName);

	// Helper function to create a blend state of "just write these pixels."
	static VkPipelineColorBlendAttachmentState withDisabledAlpha();

	// Helper function to create a blend state "do normal RGBA alpha blending."
	static VkPipelineColorBlendAttachmentState withEnabledAlpha();

	// Optionally modify these structures before calling Pipeline::init().
	VkPipelineVertexInputStateCreateInfo vertsci;
	VkPipelineInputAssemblyStateCreateInfo asci;

	// Optionally modify these structures before calling Pipeline::init().
	// viewports and scissors will be written to viewsci by Pipeline::init().
	std::vector<VkViewport> viewports;
	std::vector<VkRect2D> scissors;
	VkPipelineViewportStateCreateInfo viewsci;

	// Optionally modify these structures before calling Pipeline::init().
	VkPipelineRasterizationStateCreateInfo rastersci;
	VkPipelineMultisampleStateCreateInfo multisci;
	VkPipelineDepthStencilStateCreateInfo depthsci;

	// Use vkCreateDescriptorSetLayout to create layouts, which then
	// auto-generates VkPipelineLayoutCreateInfo.
	// TODO: Add pushConstants.
	std::vector<VkDescriptorSetLayout> setLayouts;

	// Optionally modify these structures before calling Pipeline::init().
	// perFramebufColorBlend will be written to cbsci by Pipeline::init().
	std::vector<VkPipelineColorBlendAttachmentState> perFramebufColorBlend;
	VkPipelineColorBlendStateCreateInfo cbsci;

	// Optionally modify these structures before calling init().
	// colorAttaches will be written to subpassDesc by init().
	std::vector<VkAttachmentReference> colorAttaches;
	VkSubpassDescription subpassDesc;
} PipelineCreateInfo;

// Pipeline stores the created Pipeline after RenderPass::init() has completed.
typedef struct Pipeline {
	Pipeline(language::Device& dev);
	Pipeline(Pipeline&&) = default;
	Pipeline(const Pipeline& other) = delete;

	VkPtr<VkPipelineLayout> pipelineLayout;
	VkPtr<VkPipeline> vk;

protected:
	friend struct RenderPass;

	// init() sets up shaders (and references to them in pci.stages), and
	// creates a VkPipeline. The parent renderPass and this Pipeline's
	// index in it are passed in as parameters. This method should be
	// called by RenderPass::init().
	WARN_UNUSED_RESULT virtual int init(RenderPass& renderPass, size_t subpass_i,
		PipelineCreateInfo& pci);
} Pipeline;

// RenderPass is the main object to set up and control presenting pixels to the
// screen.
//
// To create a RenderPass:
// 1. Instantiate the RenderPass:     RenderPass renderPass(dev);
// 2. Instantiate PipelineCreateInfo: PipelineCreateInfo p(dev, renderPass);
// 3. Modify p as needed.
// 4. Instantiate and load the SPV binary code into the shaders:
//    auto& vs = p.addShader(VK_SHADER_STAGE_VERTEX_BIT, "main");
//    vs.loadSPV(...);
//    // vs becomes invalid when .addShader() is called again.
//    auto& fs = p.addShader(VK_SHADER_STAGE_FRAGMENT_BIT, "main");
//    fs.loadSPV(...);
// 5. Instantiate the pipeline:       renderPass.pipelines.emplace_back(dev);
// 6. Init the renderPass, passing in one PipelineCreateInfo for each
//    pipeline instantiated in the previous step.
//        if (renderPass.init(std::vector<PipelineCreateInfo>(p))) { ... }
typedef struct RenderPass {
	RenderPass(language::Device& dev);
	RenderPass(RenderPass&&) = default;
	RenderPass(const RenderPass&) = delete;
	virtual ~RenderPass() = default;

	std::vector<Shader> shaders;
	std::vector<Pipeline> pipelines;

	// Optionally modify these structures before calling init().
	// colorAttaches will be written to rpci by init().
	std::vector<VkAttachmentDescription> colorAttaches;
	VkRenderPassCreateInfo rpci;

	// Override this function to customize the subpass dependencies.
	// The default just executes subpasses serially (in order).
	WARN_UNUSED_RESULT virtual int getSubpassDeps(size_t subpass_i,
		const std::vector<PipelineCreateInfo>& pcis,
		std::vector<VkSubpassDependency>& subpassdeps);

	// init() initializes the pipelines with the supplied vector of
	// PipelineCreateInfo objects.
	WARN_UNUSED_RESULT int init(std::vector<PipelineCreateInfo> pcis);

	VkPtr<VkRenderPass> vk;
	// passBeginInfo is populated by init() for convenience. Customize as needed.
	// Note that passBeginInfo.frameBuffer MUST be updated each frame, and
	// passBeginInfo.renderArea.extent MUST be updated any time the window is
	// resized or anything similar.
	VkRenderPassBeginInfo passBeginInfo;
	// passBeginClearColor is referenced in passBeginInfo.
	VkClearValue passBeginClearColor;
} RenderPass;

// Semaphore represents a GPU-only synchronization operation vs. Fence, below.
// Semaphores can be waited on in any queue vs. Events which must be waited on
// within a single queue.
typedef struct Semaphore {
	Semaphore(language::Device& dev) : vk{dev.dev, vkDestroySemaphore}
	{
		vk.allocator = dev.dev.allocator;
	};
	// Two-stage constructor: check the return code of ctorError().
	WARN_UNUSED_RESULT int ctorError(language::Device& dev);

	VkPtr<VkSemaphore> vk;
} Semaphore;

// PresentSemaphore is a special Semaphore that adds the present() method.
class PresentSemaphore : public Semaphore {
public:
	language::Device& dev;
	VkQueue q;
public:
	PresentSemaphore(language::Device& dev) : Semaphore(dev), dev(dev) {};
	PresentSemaphore(PresentSemaphore&&) = default;
	PresentSemaphore(const PresentSemaphore&) = delete;

	// Two-stage constructor: check the return code of ctorError().
	WARN_UNUSED_RESULT int ctorError();

	// present() submits the given swapChain image_i to Device dev's screen
	// using the correct language::PRESENT queue and synchronization.
	WARN_UNUSED_RESULT int present(uint32_t image_i);
};

// Fence represents a GPU-to-CPU synchronization. Fences are the only sync
// primitive which the CPU can wait on.
typedef struct Fence {
	Fence(language::Device& dev) : vk{dev.dev, vkDestroyFence}
	{
		vk.allocator = dev.dev.allocator;
	};
	// Two-stage constructor: check the return code of ctorError().
	WARN_UNUSED_RESULT int ctorError(language::Device& dev);

	VkPtr<VkFence> vk;
} Fence;

// Event represents a GPU-only synchronization operation, and must be waited on
// and set (signalled) within a single queue. Events can also be set (signalled)
// from the CPU.
typedef struct Event {
	Event(language::Device& dev) : vk{dev.dev, vkDestroyEvent}
	{
		vk.allocator = dev.dev.allocator;
	};
	// Two-stage constructor: check the return code of ctorError().
	WARN_UNUSED_RESULT int ctorError(language::Device& dev);

	VkPtr<VkEvent> vk;
} Event;

// CommandPool holds a reference to the VkCommandPool from which commands are
// allocated. Create a CommandPool instance in each thread that submits
// commands to qfam_i.
class CommandPool {
protected:
	language::QueueFamily * qf_ = nullptr;
	VkDevice vkdev;
public:
	CommandPool(language::Device& dev, language::SurfaceSupport queueFamily)
		: vkdev(dev.dev)
		, queueFamily(queueFamily)
		, vk{dev.dev, vkDestroyCommandPool}
	{
		vk.allocator = dev.dev.allocator;
	};
	CommandPool(CommandPool&&) = default;
	CommandPool(const CommandPool&) = delete;

	// Two-stage constructor: check the return code of ctorError().
	WARN_UNUSED_RESULT int ctorError(language::Device& dev,
		VkCommandPoolCreateFlags flags =
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	VkQueue q(size_t i) {
		return qf_->queues.at(i);
	};

	// free releases any VkCommandBuffer in buf. Command Buffers are automatically
	// freed when the CommandPool is destroyed, so free() is really only needed
	// when dynamically replacing an existing set of CommandBuffers.
	void free(std::vector<VkCommandBuffer>& buf) {
		if (!buf.size()) return;
		vkFreeCommandBuffers(vkdev, vk, buf.size(), buf.data());
	};

	// alloc calls vkAllocateCommandBuffers to populate buf with empty buffers.
	// Specify the VkCommandBufferLevel for a secondary command buffer.
	WARN_UNUSED_RESULT int alloc(std::vector<VkCommandBuffer>& buf,
		VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	// reset deallocates all command buffers in the pool (very quickly).
	WARN_UNUSED_RESULT int reset(
		VkCommandPoolResetFlagBits flags =
			VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT)
	{
		VkResult v;
		if ((v = vkResetCommandPool(vkdev, vk, flags)) != VK_SUCCESS) {
			fprintf(stderr, "vkResetCommandPool failed: %d (%s)\n", v, string_VkResult(v));
			return 1;
		}
		return 0;
	};

	const language::SurfaceSupport queueFamily;
	VkPtr<VkCommandPool> vk;
};

// CommandBuilder holds a vector of VkCommandBuffer, designed to simplify
// recording, executing, and reusing a VkCommandBuffer. A vector of
// VkCommandBuffers is more useful because one buffer may be executing while
// your application is recording into another (or similar designs). The
// CommandBuilder::use() method selects which VkCommandBuffer gets recorded
// or "built."
// https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/html/vkspec.html#commandbuffers-lifecycle
class CommandBuilder {
protected:
	CommandPool& cpool;
	bool isAllocated = false;
	size_t bufInUse = 0;
	VkCommandBuffer buf = VK_NULL_HANDLE;

	int alloc() {
		if (cpool.alloc(bufs)) {
			return 1;
		}
		isAllocated = true;
		use(bufInUse);
		return 0;
	};

public:
	CommandBuilder(CommandPool& cpool_, size_t initialSize = 1)
		: cpool(cpool_)
		, bufs(initialSize) {};
	~CommandBuilder()
	{
		cpool.free(bufs);
	};

	std::vector<VkCommandBuffer> bufs;

	// resize updates the vector size and reallocates the VkCommandBuffers.
	WARN_UNUSED_RESULT int resize(size_t bufsSize) {
		if (isAllocated) {
			// free any VkCommandBuffer in buf. Command Buffers are automatically
			// freed when the CommandPool is destroyed, so free() is really only
			// needed when dynamically replacing an existing set of CommandBuffers.
			cpool.free(bufs);
		}
		bufs.resize(bufsSize);
		return alloc();
	};

	// use selects which index in the vector bufs gets recorded or "built."
	// The first VkCommandBuffer is selected by default, to simplify cases where
	// only one CommandBuffer is needed.
	void use(size_t i) {
		bufInUse = i;
		buf = bufs.at(i);
	};

	// submit calls vkQueueSubmit using commandPoolQueueI.
	// Note vkQueueSubmit is a high overhead operation; submitting multiple
	// command buffers and even multiple VkSubmitInfo batches is recommended.
	WARN_UNUSED_RESULT int submit(size_t commandPoolQueueI) {
		VkSubmitInfo VkInit(submitInfo);
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &buf;

		VkResult v;
		if ((v = vkQueueSubmit(cpool.q(commandPoolQueueI), 1, &submitInfo,
				VK_NULL_HANDLE /*optional VkFence to sigmal*/)) != VK_SUCCESS) {
			fprintf(stderr, "vkQueueSubmit failed: %d (%s)\n", v, string_VkResult(v));
			return 1;
		}
		return 0;
	};

	// reset deallocates and clears the current VkCommandBuffer. Note that in
	// most cases, begin() calls vkBeginCommandBuffer() which implicitly resets
	// the buffer and clears any old data it may have had.
	WARN_UNUSED_RESULT int reset(
		VkCommandBufferResetFlagBits flags =
			VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT)
	{
		VkResult v;
		if ((v = vkResetCommandBuffer(buf, flags)) != VK_SUCCESS) {
			fprintf(stderr, "vkResetCommandBuffer failed: %d (%s)\n", v, string_VkResult(v));
			return 1;
		}
	};

	WARN_UNUSED_RESULT int begin(VkCommandBufferUsageFlagBits usageFlags) {
		if (!isAllocated && alloc()) {
			return 1;
		};
		VkCommandBufferBeginInfo VkInit(cbbi);
		cbbi.flags = usageFlags;
		VkResult v = vkBeginCommandBuffer(buf, &cbbi);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "vkBeginCommandBuffer failed: %d (%s)\n", v, string_VkResult(v));
			return 1;
		}
		return 0;
	};

	WARN_UNUSED_RESULT int beginOneTimeUse() {
		return begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	};
	WARN_UNUSED_RESULT int beginSimultaneousUse() {
		return begin(VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
	};


	WARN_UNUSED_RESULT int end() {
		if (!isAllocated && alloc()) {
			return 1;
		};
		VkResult v = vkEndCommandBuffer(buf);
		if (v != VK_SUCCESS) {
			fprintf(stderr, "vkEndCommandBuffer failed: %d (%s)\n", v, string_VkResult(v));
			return 1;
		}
		return 0;
	};


	WARN_UNUSED_RESULT int executeCommands(uint32_t secondaryCmdsCount,
			VkCommandBuffer * pSecondaryCmds) {
		vkCmdExecuteCommands(buf, secondaryCmdsCount, pSecondaryCmds);
		return 0;
	};


	WARN_UNUSED_RESULT int pushConstants(Pipeline& pipe,
			VkShaderStageFlags stageFlags,
			uint32_t offset,
			uint32_t size,
			const void* pValues) {
		vkCmdPushConstants(buf, pipe.pipelineLayout, stageFlags,
			offset, size, pValues);
		return 0;
	};


	WARN_UNUSED_RESULT int fillBuffer(VkBuffer dst, VkDeviceSize dstOffset,
			VkDeviceSize size, uint32_t data) {
		vkCmdFillBuffer(buf, dst, dstOffset, size, data);
		return 0;
	};


	WARN_UNUSED_RESULT int copyBuffer(VkBuffer src, VkBuffer dst,
			std::vector<VkBufferCopy>& regions) {
		if (regions.size() == 0) {
			fprintf(stderr, "copyBuffer with empty regions\n");
			return 1;
		}
		if (!isAllocated && alloc()) {
			return 1;
		};
		vkCmdCopyBuffer(buf, src, dst, regions.size(), regions.data());
		return 0;
	};
	WARN_UNUSED_RESULT int copyBuffer(VkBuffer src, VkBuffer dst, size_t size) {
		VkBufferCopy region = {};
		region.size = size;
		std::vector<VkBufferCopy> regions{region};
		return copyBuffer(src, dst, regions);
	};


	WARN_UNUSED_RESULT int copyBufferToImage(
			VkBuffer src,
			VkImage dst,
			VkImageLayout dstLayout,
			std::vector<VkBufferImageCopy>& regions) {
		vkCmdCopyBufferToImage(buf, src, dst, dstLayout,
			regions.size(), regions.data());
		return 0;
	};


	WARN_UNUSED_RESULT int copyImageToBuffer(
			VkImage src,
			VkImageLayout srcLayout,
			VkBuffer dst,
			std::vector<VkBufferImageCopy>& regions) {
		vkCmdCopyImageToBuffer(buf, src, srcLayout, dst, regions.size(), regions.data());
		return 0;
	};


	WARN_UNUSED_RESULT int copyImage(
			VkImage src,
			VkImageLayout srcLayout,
			VkImage dst,
			VkImageLayout dstLayout,
			std::vector<VkImageCopy>& regions) {
		vkCmdCopyImage(buf, src, srcLayout, dst, dstLayout,
			regions.size(), regions.data());
		return 0;
	};


	WARN_UNUSED_RESULT int blitImage(
			VkImage src,
			VkImageLayout srcLayout,
			VkImage dst,
			VkImageLayout dstLayout,
			std::vector<VkImageBlit>& regions,
			VkFilter filter) {
		vkCmdBlitImage(buf, src, srcLayout, dst, dstLayout,
			regions.size(), regions.data(), filter);
		return 0;
	};

	WARN_UNUSED_RESULT int resolveImage(
			VkImage src,
			VkImageLayout srcLayout,
			VkImage dst,
			VkImageLayout dstLayout,
			std::vector<VkImageResolve>& regions) {
		vkCmdResolveImage(buf, src, srcLayout, dst, dstLayout,
			regions.size(), regions.data());
		return 0;
	};


	WARN_UNUSED_RESULT int copyQueryPoolResults(
			VkQueryPool queryPool,
			uint32_t firstQuery,
			uint32_t queryCount,
			VkBuffer dstBuffer,
			VkDeviceSize dstOffset,
			VkDeviceSize stride,
			VkQueryResultFlags flags) {
		vkCmdCopyQueryPoolResults(buf, queryPool, firstQuery, queryCount,
			dstBuffer, dstOffset, stride, flags);
		return 0;
	};

	WARN_UNUSED_RESULT int resetQueryPool(VkQueryPool queryPool,
			uint32_t firstQuery, uint32_t queryCount) {
		vkCmdResetQueryPool(buf, queryPool, firstQuery, queryCount);
		return 0;
	};


	WARN_UNUSED_RESULT int beginQuery(VkQueryPool queryPool, uint32_t query,
			VkQueryControlFlags flags) {
		vkCmdBeginQuery(buf, queryPool, query, flags);
		return 0;
	};

	WARN_UNUSED_RESULT int endQuery(VkQueryPool queryPool, uint32_t query) {
		vkCmdEndQuery(buf, queryPool, query);
		return 0;
	};


	WARN_UNUSED_RESULT int beginRenderPass(RenderPass& pass,
			VkSubpassContents contents) {
		vkCmdBeginRenderPass(buf, &pass.passBeginInfo, contents);
		return 0;
	};

	WARN_UNUSED_RESULT int nextSubpass(VkSubpassContents contents) {
		vkCmdNextSubpass(buf, contents);
		return 0;
	};

	WARN_UNUSED_RESULT int endRenderPass() {
		vkCmdEndRenderPass(buf);
		return 0;
	};


	WARN_UNUSED_RESULT int bindPipeline(VkPipelineBindPoint bindPoint,
			Pipeline& pipe) {
		vkCmdBindPipeline(buf, bindPoint, pipe.vk);
		return 0;
	};


	WARN_UNUSED_RESULT int bindDescriptorSets(
			VkPipelineBindPoint bindPoint,
			VkPipelineLayout layout,
			uint32_t firstSet,
			uint32_t descriptorSetCount,
			const VkDescriptorSet * pDescriptorSets,
			uint32_t dynamicOffsetCount = 0,
			const uint32_t * pDynamicOffsets = nullptr) {
		vkCmdBindDescriptorSets(buf, bindPoint, layout, firstSet,
			descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
		return 0;
	};
	WARN_UNUSED_RESULT int bindGraphicsPipelineAndDescriptors(Pipeline& pipe,
			uint32_t firstSet,
			uint32_t descriptorSetCount,
			const VkDescriptorSet * pDescriptorSets,
			uint32_t dynamicOffsetCount = 0,
			const uint32_t * pDynamicOffsets = nullptr) {
		return bindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipe) ||
			bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipelineLayout,
				firstSet, descriptorSetCount, pDescriptorSets,
				dynamicOffsetCount, pDynamicOffsets);
	};
	WARN_UNUSED_RESULT int bindComputePipelineAndDescriptors(Pipeline& pipe,
			uint32_t firstSet,
			uint32_t descriptorSetCount,
			const VkDescriptorSet * pDescriptorSets,
			uint32_t dynamicOffsetCount = 0,
			const uint32_t * pDynamicOffsets = nullptr) {
		return bindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, pipe) ||
			bindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE, pipe.pipelineLayout,
				firstSet, descriptorSetCount, pDescriptorSets,
				dynamicOffsetCount, pDynamicOffsets);
	};


	WARN_UNUSED_RESULT int bindVertexBuffers(
			uint32_t firstBinding,
			uint32_t bindingCount,
			const VkBuffer * pBuffers,
			const VkDeviceSize * pOffsets) {
		vkCmdBindVertexBuffers(buf, firstBinding, bindingCount, pBuffers, pOffsets);
		return 0;
	};


	WARN_UNUSED_RESULT int bindIndexBuffer(
			VkBuffer indexBuf,
			VkDeviceSize offset,
			VkIndexType indexType) {
		vkCmdBindIndexBuffer(buf, indexBuf, offset, indexType);
		return 0;
	};


	WARN_UNUSED_RESULT int drawIndexed(
			uint32_t indexCount,
			uint32_t instanceCount,
			uint32_t firstIndex,
			int32_t vertexOffset,
			uint32_t firstInstance) {
		vkCmdDrawIndexed(buf, indexCount, instanceCount, firstIndex, vertexOffset,
			firstInstance);
		return 0;
	};

	WARN_UNUSED_RESULT int drawIndexedIndirect(
			VkBuffer buffer,
			VkDeviceSize offset,
			uint32_t drawCount,
			uint32_t stride) {
		vkCmdDrawIndexedIndirect(buf, buffer, offset, drawCount, stride);
		return 0;
	};


	WARN_UNUSED_RESULT int draw(
			uint32_t vertexCount,
			uint32_t instanceCount,
			uint32_t firstVertex,
			uint32_t firstInstance) {
		vkCmdDraw(buf, vertexCount, instanceCount, firstVertex, firstInstance);
		return 0;
	};

	WARN_UNUSED_RESULT int drawIndirect(
			VkBuffer buffer,
			VkDeviceSize offset,
			uint32_t drawCount,
			uint32_t stride) {
		vkCmdDrawIndirect(buf, buffer, offset, drawCount, stride);
		return 0;
	};


	WARN_UNUSED_RESULT int clearAttachments(
			uint32_t attachmentCount,
			const VkClearAttachment * pAttachments,
			uint32_t rectCount,
			const VkClearRect * pRects) {
		vkCmdClearAttachments(buf, attachmentCount, pAttachments,
			rectCount, pRects);
		return 0;
	};


	WARN_UNUSED_RESULT int clearColorImage(
			VkImage image,
			VkImageLayout imageLayout,
			const VkClearColorValue * pColor,
			uint32_t rangeCount,
			const VkImageSubresourceRange * pRanges) {
		vkCmdClearColorImage(buf, image, imageLayout, pColor, rangeCount, pRanges);
		return 0;
	};


	WARN_UNUSED_RESULT int clearDepthStencilImage(
			VkImage image,
			VkImageLayout imageLayout,
			const VkClearDepthStencilValue * pDepthStencil,
			uint32_t rangeCount,
			const VkImageSubresourceRange * pRanges) {
		vkCmdClearDepthStencilImage(buf, image, imageLayout, pDepthStencil,
			rangeCount, pRanges);
		return 0;
	};


	WARN_UNUSED_RESULT int dispatch(uint32_t groupCountX, uint32_t groupCountY,
			uint32_t groupCountZ) {
		vkCmdDispatch(buf, groupCountX, groupCountY, groupCountZ);
		return 0;
	};

	WARN_UNUSED_RESULT int dispatchIndirect(VkBuffer buffer, VkDeviceSize offset)
	{
		vkCmdDispatchIndirect(buf, buffer, offset);
		return 0;
	};
};

}  // namespace command
