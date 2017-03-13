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

	// TODO: Since uniforms are not yet working, please leave this member alone
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
	Semaphore(language::Device& dev) : vk{dev.dev, vkDestroySemaphore}
	{
		vk.allocator = dev.dev.allocator;
	};
	// Two-stage constructor: check the return code of ctorError().
	WARN_UNUSED_RESULT int ctorError(language::Device& dev);

	VkPtr<VkSemaphore> vk;
} Semaphore;

// Fence represents a GPU-to-CPU synchronization operation vs. Semaphore.
typedef struct Fence {
	Fence(language::Device& dev) : vk{dev.dev, vkDestroyFence}
	{
		vk.allocator = dev.dev.allocator;
	};
	// Two-stage constructor: check the return code of ctorError().
	WARN_UNUSED_RESULT int ctorError(language::Device& dev);

	VkPtr<VkFence> vk;
} Fence;

// CommandPool holds a reference to the VkCommandPool from which commands are
// allocated. Create a CommandPool instance in each thread that submits
// commands to qfam_i.
class CommandPool {
protected:
	language::QueueFamily * qf_ = nullptr;
public:
	CommandPool(language::Device& dev, language::SurfaceSupport queueFamily)
		: queueFamily(queueFamily)
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

	const language::SurfaceSupport queueFamily;
	VkQueue q(size_t i) {
		return qf_->queues.at(i);
	};
	VkPtr<VkCommandPool> vk;
};

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
	int present(uint32_t image_i);
};

}  // namespace command
