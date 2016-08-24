/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 *
 * lib/command is the 3rd-level bindings for the Vulkan graphics library.
 * lib/command is part of the v0lum3 project.
 * This library is called "command" as a homage to Star Trek First Contact.
 * Like the Vulcan High Command, this library sends out the commands.
 *
 * Note: lib/command is designed to be used in source form! The functions
 * are kept short and sweet, so there are very likely some things your app
 * needs that are not exposed by the lib/command API. Clone lib/command
 * to your own repo and make changes.
 *
 * The v0lum3 project is happy to incorporate useful changes back into the
 * upstream code.
 */

#include <lib/language/language.h>
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
	{}
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
	PipelineStage();
	PipelineStage(PipelineStage&&) = default;
	PipelineStage(const PipelineStage&) = default;

	size_t shader_i;
	std::string entryPointName;

	// Write code to initialize sci.flag, but do not initialize
	// sci.module and sci.pName. They will be written by Pipeline::init().
	VkPipelineShaderStageCreateInfo sci;
} PipelineStage;

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
	PipelineCreateInfo(language::Device& dev);
	PipelineCreateInfo(PipelineCreateInfo&&) = default;
	PipelineCreateInfo(const PipelineCreateInfo& other) = default;

	language::Device& dev;
	std::vector<PipelineStage> stages;

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

	// Note: Since uniforms are not yet working, please leave this member alone
	// for now. (Or proceed at your own risk?)
	VkPipelineLayoutCreateInfo plci;

	// Optionally modify these structures before calling Pipeline::init().
	// perFramebufColorBlend will be written to cbsci by Pipeline::init().
	std::vector<VkPipelineColorBlendAttachmentState> perFramebufColorBlend;
	VkPipelineColorBlendStateCreateInfo cbsci;

	// Optionally modify these structures before calling init().
	// colorAttaches will be written to subpassDesc by init().
	std::vector<VkAttachmentReference> colorAttaches;
	VkSubpassDescription subpassDesc;
} PipelineCreateInfo;

// Forward declaration of RenderPass for Pipeline.
struct RenderPass;

// Pipeline stores the created Pipeline after RenderPass::init() has completed.
typedef struct Pipeline {
	Pipeline(language::Device& dev);
	Pipeline(Pipeline&&) = default;
	Pipeline(const Pipeline& other) = delete;

	VkPtr<VkPipelineLayout> pipelineLayout;
	VkPtr<VkPipeline> vk;

	// Uniforms are not implemented yet, and this is probably the wrong way to do
	// it, but it compiles.
	std::vector<VkDescriptorSetLayout> descriptors;

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
// 2. Instantiate PipelineCreateInfo: PipelineCreateInfo p(dev);
// 3. Modify p as needed.
// 4. Instantiate and load the SPV binary code into the shaders:
//    auto& vs = renderPass.addShader(p, VK_SHADER_STAGE_VERTEX_BIT, "main");
//    vs.loadSPV(...);
//    // vs becomes invalid when .addShader() is called again.
//    auto& fs = renderPass.addShader(p, VK_SHADER_STAGE_FRAGMENT_BIT, "main");
//    fs.loadSPV(...);
// 5. Instantiate the pipeline:       renderPass.pipelines.emplace_back(dev);
// 6. Init the renderPass, passing in one PipelineCreateInfo for each
//    pipeline instantiated in the previous step.
//        if (renderPass.init(std::vector<PipelineCreateInfo>(p))) { ... }
typedef struct RenderPass {
	RenderPass(language::Device& dev);
	RenderPass(RenderPass&&) = default;
	RenderPass(const RenderPass&) = delete;

	std::vector<Shader> shaders;
	std::vector<Pipeline> pipelines;

	// Helper function to instantiate a Shader, add it to shaders, add it
	// to PipelineCreateInfo, then return a reference to it.
	//
	// This is also useful if the Shader will be used by more than one
	// Pipeline: addShader() on the first pipeline, then make a copy of the
	// PipelineStage object for all remaining Pipeline.
	//
	// WARNING: Returned reference Shader& is invalidated by the next
	// operation on the shaders vector.
	Shader& addShader(PipelineCreateInfo& pci, VkShaderStageFlagBits stage,
		std::string entryPointName);

	// Optionally modify these structures before calling Pipeline::init().
	// colorAttaches will be written to rpci by Pipeline::init().
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
} RenderPass;

// Semaphore represents a GPU-only synchronization operation vs. Fence, below.
typedef struct Semaphore {
	Semaphore(language::Device& dev) : vk{dev.dev, vkDestroySemaphore} {};
	// Two-stage constructor: check the return code of ctorError().
	WARN_UNUSED_RESULT int ctorError(language::Device& dev);

	VkPtr<VkSemaphore> vk;
} Semaphore;

// Fence represents a GPU-to-CPU synchronization operation vs. Semaphore.
typedef struct Fence {
	Fence(language::Device& dev) : vk{dev.dev, vkDestroyFence} {};
	// Two-stage constructor: check the return code of ctorError().
	WARN_UNUSED_RESULT int ctorError(language::Device& dev);

	VkPtr<VkFence> vk;
} Fence;

// CommandPool holds a reference to the VkCommandPool from which commands are
// allocated. Create a CommandPool instance in each thread that submits
// commands to qfam_i.
typedef struct CommandPool {
	CommandPool(language::Device& dev, size_t qfam_i_)
		: qfam_i(qfam_i_)
		, vk{dev.dev, vkDestroyCommandPool} {};
	CommandPool(CommandPool&&) = default;
	CommandPool(const CommandPool&) = delete;

	// Two-stage constructor: check the return code of ctorError().
	WARN_UNUSED_RESULT int ctorError(language::Device& dev,
		VkCommandPoolCreateFlags flags =
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	const size_t qfam_i;
	VkPtr<VkCommandPool> vk;
} CommandPool;

// CommandSwapchain simplifies setting up the Command Buffers to rotate through
// the swapchain.
typedef struct CommandSwapchain {
} CommandSwapchain;

}  // namespace command
