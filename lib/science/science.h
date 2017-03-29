/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 *
 * lib/memory is the 4th-level bindings for the Vulkan graphics library.
 * lib/memory is part of the v0lum3 project.
 * This library is called "science" as a homage to Star Trek First Contact.
 * Like the Vulcan Science Academy, this library is a repository of knowledge
 * as a series of builder classes.
 */

#include <string.h>
#include <unistd.h>
#include <lib/language/language.h>
#include <lib/command/command.h>
#include <lib/memory/memory.h>

#pragma once

namespace science {

// SubresUpdate is a builder pattern for populating a
// VkImageSubresourceRange ... or a
// VkImageSubresourceLayers.
//
// Example usage:
//   VkImageCopy region = {};
//   // Just use Subres(), no need for SubresUpdate() in simple use cases.
//   science::Subres(region.srcSubresource).addColor();
//   science::Subres(region.dstSubresource).addColor();
//   region.srcOffset = {0, 0, 0};
//   region.dstOffset = {0, 0, 0};
//   region.extent = ...;
//
//   command::CommandBuilder builder(cpool);
//   if (builder.beginOneTimeUse() ||
//       builder.copyImage(srcImage.vk, dstImage.vk,
//           std::vector<VkImageCopy>{region})) { ... handle error ... }
//
// SubresUpdate is like Subres except it does not zero out the wrapped type,
// to allow for additional building.
class SubresUpdate {
public:
	// Construct a SubresUpdate that modifies a VkImageSubresourceRange.
	SubresUpdate(VkImageSubresourceRange& range_) : range(&range_) {};
	// Construct a SubresUpdate that modifies a VkImageSubresourceLayers.
	SubresUpdate(VkImageSubresourceLayers& layers_) : layers(&layers_) {};

	// Specify that this Subres applied to a color attachment.
	SubresUpdate& addColor() {
		if (range) {
			range->aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
		}
		if (layers) {
			layers->aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
		}
		return *this;
	};
	// Specify that this Subres applied to a depth attachment.
	SubresUpdate& addDepth() {
		if (range) {
			range->aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		if (layers) {
			layers->aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		return *this;
	};
	// Specify that this Subres applied to a stencil attachment.
	SubresUpdate& addStencil() {
		if (range) {
			range->aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		if (layers) {
			layers->aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		return *this;
	};

	// Specify mip-mapping offset and count.
	// This only applies to VkImageSubresourceRange, and will abort on a
	// VkImageSubresourceLayers.
	SubresUpdate& setMips(uint32_t offset, uint32_t count) {
		if (range) {
			range->baseMipLevel = offset;
			range->levelCount = count;
		}
		if (layers) {
			fprintf(stderr, "Subres: cannot setMips() on VkImageSubresourceLayers. "
				"Try setMip() instead.\n");
			exit(1);
		}
		return *this;
	};

	// Specify mipmap layer.
	// This applies to VkImageSubresourceLayers, and will abort on a
	// VkImageSubresourceRange
	SubresUpdate& setMip(uint32_t level) {
		if (range) {
			fprintf(stderr, "Subres: cannot setMip() on VkImageSubresourceRange. "
				"Try setMips() instead.\n");
			exit(1);
		}
		if (layers) {
			layers->mipLevel = level;
		}
		return *this;
	};

	// Specify layer offset and count. Might be used for stereo displays.
	SubresUpdate& setLayer(uint32_t offset, uint32_t count) {
		if (range) {
			range->baseArrayLayer = offset;
			range->layerCount = count;
		}
		if (layers) {
			layers->baseArrayLayer = offset;
			layers->layerCount = count;
		}
		return *this;
	};

protected:
	VkImageSubresourceRange * range = nullptr;
	VkImageSubresourceLayers * layers = nullptr;
};

// Subres is a builder pattern for populating a
// VkImageSubresourceRange ... or a
// VkImageSubresourceLayers.
//
// Construct it wrapped around an existing VkImageSubresourceRange or
// VkImageSubresourceLayers. It will immediately zero out the wrapped type.
// Then use the mutator methods to build the type.
class Subres : public SubresUpdate {
public:
	// Construct a Subres that modifies a VkImageSubresourceRange.
	Subres(VkImageSubresourceRange& range_) : SubresUpdate(range_) {
		memset(range, 0, sizeof(*range));
		range->baseMipLevel = 0;  // Mipmap level offset (none).
		range->levelCount = 1;  // There is 1 mipmap (no mipmapping).
		range->baseArrayLayer = 0;  // Offset in layerCount layers.
		range->layerCount = 1;  // Might be 2 for stereo displays.
	};
	// Construct a Subres that modifies a VkImageSubresourceLayers.
	Subres(VkImageSubresourceLayers& layers_) : SubresUpdate(layers_) {
		memset(layers, 0, sizeof(*layers));
		layers->mipLevel = 0;  // First mipmap level.
		layers->baseArrayLayer = 0;  // Offset in layerCount layers.
		layers->layerCount = 1;  // Might be 2 for stereo displays.
	};
};


// hasStencil() returns whether a VkFormat includes the stencil buffer or not.
// This is used for example by memory::Image::makeTransition().
inline bool hasStencil(VkFormat format) {
	switch (format) {
	case VK_FORMAT_S8_UINT:
	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return true;
	default:
		return false;
	}
};


// PipeBuilder is a builder for command::Pipeline.
typedef struct PipeBuilder {
	PipeBuilder(language::Device& dev, command::RenderPass& pass)
		: pipeline{pass.addPipeline(dev)}
		, depthImage{dev}
		, depthImageView{dev} {};
	PipeBuilder(PipeBuilder&&) = default;
	PipeBuilder(const PipeBuilder& other) = delete;

	command::Pipeline& pipeline;

	// addDepthImage adds a depth buffer to the pipeline, choosing the first
	// of formatChoices that is available.
	// Because addDepthImage automatically calls recreateSwapChainExtent, you must
	// pass in a valid builder for recreateSwapChainExtent.
	WARN_UNUSED_RESULT int addDepthImage(
		language::Device& dev,
		command::RenderPass& pass,
		command::CommandBuilder& builder,
		const std::vector<VkFormat>& formatChoices);

	// recreateSwapChainExtent rebuilds the pipeline with an updated extent from
	// dev.swapChainExtent. It adds commands to CommandBuilder builder to set up
	// the depth image, which must have already had a begin...() call on it.
	// You must then call end() and submit() before beginning a RenderPass.
	WARN_UNUSED_RESULT int recreateSwapChainExtent(
		language::Device& dev, command::CommandBuilder& builder);

	memory::Image depthImage;
	language::ImageView depthImageView;
} PipeBuilder;

} // namespace science
