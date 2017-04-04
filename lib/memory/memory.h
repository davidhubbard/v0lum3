/* Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
 *
 * lib/memory is the 4th-level bindings for the Vulkan graphics library.
 * lib/memory is part of the v0lum3 project.
 * This library is called "memory" as a homage to Star Trek First Contact.
 * Like a Vulcan's Memory, this library remembers everything.
 */

#include <lib/command/command.h>
#include <lib/language/VkInit.h>
#include <lib/language/language.h>

#pragma once

namespace memory {

struct MemoryRequirements;

// DeviceMemory represents a raw chunk of bytes that can be accessed by the
// device. Because GPUs are in everything now, the memory may not be physically
// "on the device," but all that is hidden by the device driver to make it
// seem like it is.
//
// DeviceMemory is not very useful on its own. But alloc() can be fed a
// MemoryRequirements object with an in-place constructor, like this:
//   DeviceMemory mem(dev);
//   mem.alloc({dev, img}, p);  // MemoryRequirements constructed from an Image.
//
//   DeviceMemory mem(dev);
//   mem.alloc({dev, buf}, p);  // MemoryRequirements constructed from a Buffer.
//
// By using the overloaded constructors in MemoryRequirements,
// DeviceMemory::alloc() is kept simple.
typedef struct DeviceMemory {
  DeviceMemory(language::Device& dev) : vk{dev.dev, vkFreeMemory} {}

  // alloc() calls vkAllocateMemory() and returns non-zero on error.
  // Note: if you use Image, Buffer, etc. below, alloc() is automatically called
  // for you by Image::ctorError(), Buffer::ctorError(), etc.
  WARN_UNUSED_RESULT int alloc(MemoryRequirements req,
                               VkMemoryPropertyFlags props);

  // mmap() calls vkMapMemory() and returns non-zero on error.
  // NOTE: The vkMapMemory spec currently says "flags is reserved for future
  // use." You probably can ignore the flags parameter.
  WARN_UNUSED_RESULT int mmap(language::Device& dev, void** pData,
                              VkDeviceSize offset = 0,
                              VkDeviceSize size = VK_WHOLE_SIZE,
                              VkMemoryMapFlags flags = 0);

  // munmap() calls vkUnmapMemory().
  void munmap(language::Device& dev);

  VkPtr<VkDeviceMemory> vk;
} DeviceMemory;

// Image represents a VkImage.
typedef struct Image {
  Image(language::Device& dev) : vk{dev.dev, vkDestroyImage}, mem(dev) {
    VkOverwrite(info);
    info.imageType = VK_IMAGE_TYPE_2D;
    // You must set info.extent.width, info.extent.height, and
    // info.extent.depth. For a 2D image, set depth = 1. For a 1D image,
    // set height = 1 and depth = 1.
    info.mipLevels = 1;
    info.arrayLayers = 1;
    // You must set info.format
    // You probably want tiling = VK_IMAGE_TILING_OPTIMAL most of the time:
    info.tiling = VK_IMAGE_TILING_LINEAR;
    info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    // You must set info.usage
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // ctorError() sets currentLayout = info.initialLayout.
    currentLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
  }
  Image(Image&&) = default;
  Image(const Image&) = delete;

  // ctorError must be called after filling in this->info to construct the
  // Image. Note that bindMemory() should be called after ctorError().
  //
  // Your application may not need to call ctorError directly.
  // Sampler::ctorError(), below, automatically sets up an image for a shader
  // sampler, and science::Pipeline() automatically sets up an image for a depth
  // buffer.
  WARN_UNUSED_RESULT int ctorError(language::Device& dev,
                                   VkMemoryPropertyFlags props);

  WARN_UNUSED_RESULT int ctorDeviceLocal(language::Device& dev) {
    return ctorError(dev, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }

  WARN_UNUSED_RESULT int ctorHostVisible(language::Device& dev) {
    info.tiling = VK_IMAGE_TILING_LINEAR;
    info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    currentLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    return ctorError(dev, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  }

  WARN_UNUSED_RESULT int ctorHostCoherent(language::Device& dev) {
    info.tiling = VK_IMAGE_TILING_LINEAR;
    info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    currentLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    return ctorError(dev, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }

  // bindMemory() calls vkBindImageMemory which binds this->mem.
  // Note: do not call bindMemory() until some time after ctorError().
  WARN_UNUSED_RESULT int bindMemory(language::Device& dev,
                                    VkDeviceSize offset = 0);

  // makeTransition() returns a VkImageMemoryBarrier, store it in a
  // CommandBuilder::BarrierSet and call CommandBuilder::barrier() with it.
  // makeTransition() sets the returned VkImageMemoryBarrier field
  // "VkImage image" to VK_NULL_HANDLE on error.
  // Note: if you add the VkImageMemoryBarrier to a BarrierSet,
  // CommandBuilder::barrier() always checks the "image" field, so you can just
  // submit it and barrier() will catch any errors.
  //
  // Note: when appropriate, you need to manually change the currentLayout.
  // Updating is nuanced: if you update currentLayout immediately, then if the
  // CommandBuilder aborts ensure you roll back to the previous currentLayout.
  // On the other hand, waiting for the CommandBuilder submit and syncing
  // the host to the GPU before updating currentLayout is probably overkill.
  //
  // Modify the barrier if your application needs more advanced transitions.
  // Example: move to a different queueFamily with:
  //   // Use image.currentLayout since the layout is not changing.
  //   VkImageMemoryBarrier imageB = image.makeTransition(image.currentLayout);
  //   imageB.srcQueueFamilyIndex = oldQueueFamilyIndex;
  //   imageB.dstQueueFamilyIndex = newQueueFamilyIndex;
  //   ... proceed to use imageB like normal ...
  VkImageMemoryBarrier makeTransition(VkImageLayout newLayout);

  VkImageCreateInfo info;
  VkImageLayout currentLayout;
  VkPtr<VkImage> vk;  // populated after ctorError().
  DeviceMemory mem;   // ctorError() calls mem.alloc() for you.

 protected:
  int makeTransitionAccessMasks(VkImageMemoryBarrier& imageB);
} Image;

// Buffer represents a VkBuffer.
typedef struct Buffer {
  Buffer(language::Device& dev) : vk{dev.dev, vkDestroyBuffer}, mem(dev) {
    VkOverwrite(info);
    // You must set info.size.
    // You must set info.usage.
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }
  Buffer(Buffer&&) = default;
  Buffer(const Buffer&) = delete;

  // ctorError() must be called after filling in this->info to construct the
  // Buffer. Note that bindMemory() should be called after ctorError().
  //
  // Some aliases of ctorError() are defined below, which may make your
  // application less verbose. These are not all the possible combinations.
  WARN_UNUSED_RESULT int ctorError(language::Device& dev,
                                   VkMemoryPropertyFlags props);

  // ctorDeviceLocal() adds TRANSFER_DST to usage, but you should set
  // its primary uses (for example, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
  // VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
  // or all three).
  WARN_UNUSED_RESULT int ctorDeviceLocal(language::Device& dev) {
    info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return ctorError(dev, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }

  WARN_UNUSED_RESULT int ctorHostVisible(language::Device& dev) {
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    return ctorError(dev, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  }

  WARN_UNUSED_RESULT int ctorHostCoherent(language::Device& dev) {
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    return ctorError(dev, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  }

  // bindMemory() calls vkBindBufferMemory which binds this->mem.
  // Note: do not call bindMemory() until some time after ctorError().
  WARN_UNUSED_RESULT int bindMemory(language::Device& dev,
                                    VkDeviceSize offset = 0);

  VkBufferCreateInfo info;
  VkPtr<VkBuffer> vk;  // populated after ctorError().
  DeviceMemory mem;    // ctorError() calls mem.alloc() for you.
} Buffer;

// MemoryRequirements automatically gets the VkMemoryRequirements from
// the Device, and has helper methods for finding the VkMemoryAllocateInfo.
typedef struct MemoryRequirements {
  // Automatically get MemoryRequirements of a VkImage.
  MemoryRequirements(language::Device& dev, VkImage img);
  // Automatically get MemoryRequirements of an Image.
  MemoryRequirements(language::Device& dev, Image& img);

  // Automatically get MemoryRequirements of a VkBuffer.
  MemoryRequirements(language::Device& dev, VkBuffer buf);
  // Automatically get MemoryRequirements of a Buffer.
  MemoryRequirements(language::Device& dev, Buffer& buf);

  // indexOf() returns -1 if the props cannot be found.
  int indexOf(VkMemoryPropertyFlags props) const;

  // ofProps() returns nullptr on error. Otherwise, it populates vkalloc with
  // the requirements in vk, and returns a pointer.
  VkMemoryAllocateInfo* ofProps(VkMemoryPropertyFlags props);

  VkMemoryRequirements vk;
  VkMemoryAllocateInfo vkalloc;
  language::Device& dev;
} MemoryRequirements;

// Sampler contains an Image, the ImageView, and the VkSampler, and has
// convenience methods for passing the VkSampler to descriptor sets and shaders.
typedef struct Sampler {
  // Construct a Sampler with info set to defaults (set to NEAREST mode,
  // which looks very blocky / pixellated).
  Sampler(language::Device& dev)
      : image{dev}, imageView{dev}, vk{dev.dev, vkDestroySampler} {
    VkOverwrite(info);
    // info.magFilter = VK_FILTER_NEAREST;
    // info.minFilter = VK_FILTER_NEAREST;
    // info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.anisotropyEnable = VK_TRUE;
    info.maxAnisotropy = dev.physProp.limits.maxSamplerAnisotropy;
    info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;
    info.compareEnable = VK_FALSE;
    info.compareOp = VK_COMPARE_OP_ALWAYS;
  }

  // TODO: add a ctorError() method to construct vk, the Image (and bind it),
  // and the ImageView. Then just return. The caller can use the (host-visible)
  // Image. The caller must manually transition the image to a shader layout.

  // ctorError() constructs vk, the Image, and ImageView. It enqueues calls on
  // command::CommandBuilder 'builder' to do layout transitions and copyImage().
  // The Image.info.{extent,format} are set to src.info.{extent,format}.
  // Also src.currentLayout is modified, which does not actually happen until
  // the command builder is submitted.
  WARN_UNUSED_RESULT int ctorError(language::Device& dev,
                                   command::CommandBuilder& builder,
                                   Image& src);

  // toDescriptor is a convenience method to add this Sampler to a descriptor
  // set.
  void toDescriptor(VkDescriptorImageInfo* imageInfo) {
    imageInfo->imageLayout = image.currentLayout;
    imageInfo->imageView = imageView.vk;
    imageInfo->sampler = vk;
  }

  Image image;
  language::ImageView imageView;
  VkSamplerCreateInfo info;
  VkPtr<VkSampler> vk;
} Sampler;

// DescriptorPool represents memory reserved for a DescriptorSet (or many).
// The assumption is that your application knows in advance the max number of
// DescriptorSet instances that will exist.
//
// It is also assumed your application knows the max number of each descriptor
// (VkDescriptorType) that will make up the DescriptorSet or sets.
typedef struct DescriptorPool {
  DescriptorPool(language::Device& dev)
      : dev(dev), vk{dev.dev, vkDestroyDescriptorPool} {
    vk.allocator = dev.dev.allocator;
  }

  // ctorError calls vkCreateDescriptorPool to enable creating
  // a DescriptorSet from this pool.
  //
  // maxSets is how many DescriptorSet instances can be created.
  //
  // maxDescriptors is how many of each VkDescriptorType to create:
  // add 1 VkDescriptorType (in any order) for every descriptor of
  // that type.
  WARN_UNUSED_RESULT int ctorError(
      uint32_t maxSets, std::vector<VkDescriptorType> maxDescriptors);

  WARN_UNUSED_RESULT int reset() {
    VkResult v = vkResetDescriptorPool(dev.dev, vk, 0 /*flags is reserved*/);
    if (v != VK_SUCCESS) {
      fprintf(stderr, "vkResetDescriptorPool failed: %d (%s)\n", v,
              string_VkResult(v));
      return 1;
    }
    return 0;
  }

  language::Device& dev;
  VkPtr<VkDescriptorPool> vk;
} DescriptorPool;

// DescriptorSetLayout represents a group of VkDescriptorSetLayoutBinding
// objects. This is useful when several groups are being assembled into a
// DescriptorSet.
//
// It may be simpler to use science::ShaderLibrary.
typedef struct DescriptorSetLayout {
  DescriptorSetLayout(language::Device& dev)
      : vk{dev.dev, vkDestroyDescriptorSetLayout} {
    vk.allocator = dev.dev.allocator;
  }

  WARN_UNUSED_RESULT int ctorError(
      language::Device& dev,
      const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    VkDescriptorSetLayoutCreateInfo VkInit(info);
    info.bindingCount = bindings.size();
    info.pBindings = bindings.data();

    VkResult v =
        vkCreateDescriptorSetLayout(dev.dev, &info, dev.dev.allocator, &vk);
    if (v != VK_SUCCESS) {
      fprintf(stderr, "vkCreateDescriptorSetLayout failed: %d (%s)\n", v,
              string_VkResult(v));
      return 1;
    }
    return 0;
  }

  VkPtr<VkDescriptorSetLayout> vk;
} DescriptorSetLayout;

// DescriptorSet represents a set of bindings (which represent buffers) that
// the host application must bind (provide) for the shader. If the
// DescriptorSet does not match the layout defined in the shader, Vulkan will
// report an error.
//
// A DescriptorSet is allocated to match a DescriptorSetLayout and retains a
// reference to the DescriptorPool from which it was allocated.
//
// Notes:
// 1. When it is allocated, it does not contain a valid type or buffer! Use
//    VkWriteDescriptorSet to populate the DescriptorSet with type and buffer.
// 2. During Pipeline initialization, VkDescriptorSetLayout objects are linked
//    to the shader to assemble a valid pipeline (see PipelineCreateInfo).
// 3. During a RenderPass, binding a DescriptorSet to the shader provides the
//    shader with its inputs and outputs.
typedef struct DescriptorSet {
  DescriptorSet(DescriptorPool& pool) : pool(pool) {}
  virtual ~DescriptorSet();

  WARN_UNUSED_RESULT int ctorError(DescriptorSetLayout& layout) {
    return ctorError(layout.vk);
  }

  WARN_UNUSED_RESULT int ctorError(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo VkInit(info);
    info.descriptorPool = pool.vk;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;

    VkResult v = vkAllocateDescriptorSets(pool.dev.dev, &info, &vk);
    if (v != VK_SUCCESS) {
      fprintf(stderr,
              "vkAllocateDescriptorSets failed: %d (%s)\n"
              "The Vulkan spec suggests:\n"
              "1. Ignore the exact error code returned.\n"
              "2. Try creating a new DescriptorPool.\n"
              "3. Retry DescriptorSet::ctorError().\n"
              "4. If that fails, abort.\n",
              v, string_VkResult(v));
      return 1;
    }
    return 0;
  }

  DescriptorPool& pool;
  VkDescriptorSet vk;
} DescriptorSet;

}  // namespace memory
