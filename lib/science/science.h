/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 *
 * lib/memory is the 4th-level bindings for the Vulkan graphics library.
 * lib/memory is part of the v0lum3 project.
 * This library is called "science" as a homage to Star Trek First Contact.
 * Like the Vulcan Science Academy, this library is a repository of knowledge
 * about how to connect everything together.
 */

#include <string.h>
#include <lib/language/language.h>

namespace science {

//
// subres...(VkImageSubresourceRange& range) shortcuts:
// Populate a VkImageSubresourceRange quickly with a preset.
//
// subres...(VkImageSubresourceLayers& layers) shortcuts:
// Populate a VkImageSubresourceLayers quickly with a preset.
//

inline void subres1MipAndColor(VkImageSubresourceRange& range) {
	memset(&range, 0, sizeof(range));
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	range.baseMipLevel = 0;  // Mipmap level offset (none).
	range.levelCount = 1;  // There is 1 mipmap (no mipmapping).
	range.baseArrayLayer = 0;  // Offset in layerCount layers.
	range.layerCount = 1;  // Might be 2 for stereo displays.
};

inline void subres1MipAndColorDepth(VkImageSubresourceRange& range) {
	memset(&range, 0, sizeof(range));
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;

	range.baseMipLevel = 0;  // Mipmap level offset (none).
	range.levelCount = 1;  // There is 1 mipmap (no mipmapping).
	range.baseArrayLayer = 0;  // Offset in layerCount layers.
	range.layerCount = 1;  // Might be 2 for stereo displays.
};

inline void subres1MipAndDepth(VkImageSubresourceRange& range) {
	memset(&range, 0, sizeof(range));
	range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

	range.baseMipLevel = 0;  // Mipmap level offset (none).
	range.levelCount = 1;  // There is 1 mipmap (no mipmapping).
	range.baseArrayLayer = 0;  // Offset in layerCount layers.
	range.layerCount = 1;  // Might be 2 for stereo displays.
};

inline void subres1MipAndColorDepthStencil(VkImageSubresourceRange& range) {
	memset(&range, 0, sizeof(range));
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT |
		VK_IMAGE_ASPECT_STENCIL_BIT;

	range.baseMipLevel = 0;  // Mipmap level offset (none).
	range.levelCount = 1;  // There is 1 mipmap (no mipmapping).
	range.baseArrayLayer = 0;  // Offset in layerCount layers.
	range.layerCount = 1;  // Might be 2 for stereo displays.
};

inline void subres1MipAndDepthStencil(VkImageSubresourceRange& range) {
	memset(&range, 0, sizeof(range));
	range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

	range.baseMipLevel = 0;  // Mipmap level offset (none).
	range.levelCount = 1;  // There is 1 mipmap (no mipmapping).
	range.baseArrayLayer = 0;  // Offset in layerCount layers.
	range.layerCount = 1;  // Might be 2 for stereo displays.
};

inline void subres1MipAndStencil(VkImageSubresourceRange& range) {
	memset(&range, 0, sizeof(range));
	range.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

	range.baseMipLevel = 0;  // Mipmap level offset (none).
	range.levelCount = 1;  // There is 1 mipmap (no mipmapping).
	range.baseArrayLayer = 0;  // Offset in layerCount layers.
	range.layerCount = 1;  // Might be 2 for stereo displays.
};

inline void subres1MipAndColor(VkImageSubresourceLayers& layers) {
	memset(&layers, 0, sizeof(layers));
	layers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	layers.mipLevel = 0;  // First mipmap level.
	layers.baseArrayLayer = 0;  // Offset in layerCount layers.
	layers.layerCount = 1;  // Might be 2 for stereo displays.
};

inline void subres1MipAndColorDepth(VkImageSubresourceLayers& layers) {
	memset(&layers, 0, sizeof(layers));
	layers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;

	layers.mipLevel = 0;  // First mipmap level.
	layers.baseArrayLayer = 0;  // Offset in layerCount layers.
	layers.layerCount = 1;  // Might be 2 for stereo displays.
};

inline void subres1MipAndDepth(VkImageSubresourceLayers& layers) {
	memset(&layers, 0, sizeof(layers));
	layers.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

	layers.mipLevel = 0;  // First mipmap level.
	layers.baseArrayLayer = 0;  // Offset in layerCount layers.
	layers.layerCount = 1;  // Might be 2 for stereo displays.
};

inline void subres1MipAndColorDepthStencil(VkImageSubresourceLayers& layers) {
	memset(&layers, 0, sizeof(layers));
	layers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT |
		VK_IMAGE_ASPECT_STENCIL_BIT;

	layers.mipLevel = 0;  // First mipmap level.
	layers.baseArrayLayer = 0;  // Offset in layerCount layers.
	layers.layerCount = 1;  // Might be 2 for stereo displays.
};

inline void subres1MipAndDepthStencil(VkImageSubresourceLayers& layers) {
	memset(&layers, 0, sizeof(layers));
	layers.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

	layers.mipLevel = 0;  // First mipmap level.
	layers.baseArrayLayer = 0;  // Offset in layerCount layers.
	layers.layerCount = 1;  // Might be 2 for stereo displays.
};

inline void subres1MipAndStencil(VkImageSubresourceLayers& layers) {
	memset(&layers, 0, sizeof(layers));
	layers.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

	layers.mipLevel = 0;  // First mipmap level.
	layers.baseArrayLayer = 0;  // Offset in layerCount layers.
	layers.layerCount = 1;  // Might be 2 for stereo displays.
};


} // namespace science
