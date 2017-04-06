/* Copyright (c) David Hubbard 2017. Licensed under the GPLv3.
 */
#include "memory.h"
#include <lib/science/science.h>

namespace memory {

int DeviceMemory::alloc(MemoryRequirements req, VkMemoryPropertyFlags props) {
  // This relies on the fact that req.ofProps() updates req.vkalloc.
  if (!req.ofProps(props)) {
    fprintf(stderr, "DeviceMemory::alloc: indexOf returned not found\n");
    return 1;
  }
  vk.reset();
  VkResult v =
      vkAllocateMemory(req.dev.dev, &req.vkalloc, req.dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "vkAllocateMemory failed: %d (%s)\n", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

int DeviceMemory::mmap(language::Device& dev, void** pData,
                       VkDeviceSize offset /*= 0*/,
                       VkDeviceSize size /*= VK_WHOLE_SIZE*/,
                       VkMemoryMapFlags flags /*= 0*/) {
  VkResult v = vkMapMemory(dev.dev, vk, offset, size, flags, pData);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "vkMapMemory failed: %d (%s)\n", v, string_VkResult(v));
    return 1;
  }
  return 0;
}

void DeviceMemory::munmap(language::Device& dev) { vkUnmapMemory(dev.dev, vk); }

int Image::ctorError(language::Device& dev, VkMemoryPropertyFlags props) {
  if (!info.extent.width || !info.extent.height || !info.extent.depth ||
      !info.format || !info.usage) {
    fprintf(stderr, "Image::ctorError found uninitialized fields\n");
    return 1;
  }

  vk.reset();
  VkResult v = vkCreateImage(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "vkCreateImage failed: %d (%s)\n", v, string_VkResult(v));
    return 1;
  }
  currentLayout = info.initialLayout;

  return mem.alloc({dev, vk}, props);
}

int Image::bindMemory(language::Device& dev, VkDeviceSize offset /*= 0*/) {
  VkResult v = vkBindImageMemory(dev.dev, vk, mem.vk, offset);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "vkBindImageMemory failed: %d (%s)\n", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

int Buffer::ctorError(language::Device& dev, VkMemoryPropertyFlags props) {
  if (!info.size || !info.usage) {
    fprintf(stderr, "Buffer::ctorError found uninitialized fields\n");
    return 1;
  }

  vk.reset();
  VkResult v = vkCreateBuffer(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "vkCreateBuffer failed: %d (%s)\n", v, string_VkResult(v));
    return 1;
  }

  return mem.alloc({dev, vk}, props);
}

int Buffer::bindMemory(language::Device& dev, VkDeviceSize offset /*= 0*/) {
  VkResult v = vkBindBufferMemory(dev.dev, vk, mem.vk, offset);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "vkBindBufferMemory failed: %d (%s)\n", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

int Buffer::copyFromHost(language::Device& dev, const void* src, size_t len,
                         VkDeviceSize dstOffset /*= 0*/) {
  if (!(info.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
    fprintf(stderr,
            "WARNING: Buffer::copyFromHost on a Buffer where neither "
            "ctorHostVisible nor ctorHostCoherent was used.\n"
            "WARNING: usage = 0x%x",
            info.usage);

    // Dump info.usage bits.
    const char* firstPrefix = " (";
    const char* prefix = firstPrefix;
    for (uint64_t bit = 1; bit < VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
         bit <<= 1) {
      if (((uint64_t)info.usage) & bit) {
        fprintf(stderr, "%s%s", prefix,
                string_VkBufferUsageFlagBits((VkBufferUsageFlagBits)bit));
        prefix = " | ";
      }
    }
    if (prefix != firstPrefix) {
      fprintf(stderr, ")");
    }
    fprintf(stderr, "\n");
    return 1;
  }

  if (dstOffset + len > info.size) {
    fprintf(stderr,
            "BUG: Buffer::copyFromHost(len=0x%lx, dstOffset=0x%lx).\n"
            "BUG: when Buffer.info.size=0x%lx\n",
            len, dstOffset, info.size);
    return 1;
  }

  void* mapped;
  if (mem.mmap(dev, &mapped)) {
    return 1;
  }
  memcpy(((char*)mapped) + dstOffset, src, len);
  mem.munmap(dev);
  return 0;
}

MemoryRequirements::MemoryRequirements(language::Device& dev, VkImage img)
    : dev(dev) {
  VkOverwrite(vkalloc);
  vkGetImageMemoryRequirements(dev.dev, img, &vk);
}

MemoryRequirements::MemoryRequirements(language::Device& dev, Image& img)
    : dev(dev) {
  VkOverwrite(vkalloc);
  vkGetImageMemoryRequirements(dev.dev, img.vk, &vk);
}

MemoryRequirements::MemoryRequirements(language::Device& dev, VkBuffer buf)
    : dev(dev) {
  VkOverwrite(vkalloc);
  vkGetBufferMemoryRequirements(dev.dev, buf, &vk);
}

MemoryRequirements::MemoryRequirements(language::Device& dev, Buffer& buf)
    : dev(dev) {
  VkOverwrite(vkalloc);
  vkGetBufferMemoryRequirements(dev.dev, buf.vk, &vk);
}

int MemoryRequirements::indexOf(VkMemoryPropertyFlags props) const {
  for (uint32_t i = 0; i < dev.memProps.memoryTypeCount; i++) {
    if ((vk.memoryTypeBits & (1 << i)) &&
        (dev.memProps.memoryTypes[i].propertyFlags & props) == props) {
      return i;
    }
  }

  fprintf(stderr, "MemoryRequirements::indexOf(%x): not found in %x\n", props,
          vk.memoryTypeBits);
  return -1;
}

VkMemoryAllocateInfo* MemoryRequirements::ofProps(VkMemoryPropertyFlags props) {
  int i = indexOf(props);
  if (i == -1) {
    return nullptr;
  }

  vkalloc.memoryTypeIndex = i;
  vkalloc.allocationSize = vk.size;

  return &vkalloc;
}

int DescriptorPool::ctorError(uint32_t maxSets,
                              std::vector<VkDescriptorType> maxDescriptors) {
  VkDescriptorPoolCreateInfo VkInit(info);
  info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

  // Vulkan Spec says: "If multiple VkDescriptorPoolSize structures appear in
  // the pPoolSizes array then the pool will be created with enough storage
  // for the total number of descriptors of each type."
  //
  // The Vulkan driver will count the number of times each VkDescriptorType
  // occurs. This just translates the VkDescriptorType into a request for
  // 1 descriptor of that type.
  std::vector<VkDescriptorPoolSize> poolSizes;
  for (auto& dType : maxDescriptors) {
    poolSizes.emplace_back();
    auto& poolSize = *(poolSizes.end() - 1);
    VkOverwrite(poolSize);
    poolSize.type = dType;
    poolSize.descriptorCount = 1;
  }
  info.poolSizeCount = poolSizes.size();
  info.pPoolSizes = poolSizes.data();
  info.maxSets = maxSets;

  vk.reset();
  VkResult v = vkCreateDescriptorPool(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "vkCreateDescriptorPool failed: %d (%s)\n", v,
            string_VkResult(v));
    return 1;
  }
  vk.allocator = dev.dev.allocator;
  return 0;
}

int DescriptorSetLayout::ctorError(
    language::Device& dev,
    const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
  // Save the descriptor types that make up this layout.
  types.clear();
  types.reserve(bindings.size());
  for (auto& binding : bindings) {
    types.emplace_back(binding.descriptorType);
  }

  VkDescriptorSetLayoutCreateInfo VkInit(info);
  info.bindingCount = bindings.size();
  info.pBindings = bindings.data();

  vk.reset();
  VkResult v =
      vkCreateDescriptorSetLayout(dev.dev, &info, dev.dev.allocator, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "vkCreateDescriptorSetLayout failed: %d (%s)\n", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

DescriptorSet::~DescriptorSet() {
  VkResult v = vkFreeDescriptorSets(pool.dev.dev, pool.vk, 1, &vk);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "vkFreeDescriptorSets failed: %d (%s)\n", v,
            string_VkResult(v));
    exit(1);
  }
}

int DescriptorSet::ctorError(const DescriptorSetLayout& layout) {
  types = layout.types;
  const VkDescriptorSetLayout& setLayout = layout.vk;

  VkDescriptorSetAllocateInfo VkInit(info);
  info.descriptorPool = pool.vk;
  info.descriptorSetCount = 1;
  info.pSetLayouts = &setLayout;

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

int DescriptorSet::write(uint32_t binding,
                         const std::vector<VkDescriptorImageInfo> imageInfo,
                         uint32_t arrayI /*= 0*/) {
  if (binding > types.size()) {
    fprintf(stderr,
            "DescriptorSet::write(%u, imageInfo): binding=%u with only %zu "
            "bindings\n",
            binding, binding, types.size());
    return 1;
  }
  switch (types.at(binding)) {
    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      break;
    default:
      fprintf(stderr,
              "DescriptorSet::write(%u, imageInfo): binding=%u has type %s\n",
              binding, binding, string_VkDescriptorType(types.at(binding)));
      return 1;
  }
  VkWriteDescriptorSet VkInit(w);
  w.dstSet = vk;
  w.dstBinding = binding;
  w.dstArrayElement = arrayI;
  w.descriptorType = types.at(binding);
  w.descriptorCount = imageInfo.size();
  w.pImageInfo = imageInfo.data();
  vkUpdateDescriptorSets(pool.dev.dev, 1, &w, 0, nullptr);
  return 0;
}

int DescriptorSet::write(uint32_t binding,
                         const std::vector<VkDescriptorBufferInfo> bufferInfo,
                         uint32_t arrayI /*= 0*/) {
  if (binding > types.size()) {
    fprintf(stderr,
            "DescriptorSet::write(%u, bufferInfo): binding=%u with only %zu "
            "bindings\n",
            binding, binding, types.size());
    return 1;
  }
  switch (types.at(binding)) {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
      break;
    default:
      fprintf(stderr,
              "DescriptorSet::write(%u, bufferInfo): binding=%u has type %s\n",
              binding, binding, string_VkDescriptorType(types.at(binding)));
      return 1;
  }
  VkWriteDescriptorSet VkInit(w);
  w.dstSet = vk;
  w.dstBinding = binding;
  w.dstArrayElement = arrayI;
  w.descriptorType = types.at(binding);
  w.descriptorCount = bufferInfo.size();
  w.pBufferInfo = bufferInfo.data();
  vkUpdateDescriptorSets(pool.dev.dev, 1, &w, 0, nullptr);
  return 0;
}

int DescriptorSet::write(uint32_t binding,
                         const std::vector<VkBufferView> texelBufferViewInfo,
                         uint32_t arrayI /*= 0*/) {
  if (binding > types.size()) {
    fprintf(stderr,
            "DescriptorSet::write(%u, VkBufferView): binding=%u with only %zu "
            "bindings\n",
            binding, binding, types.size());
    return 1;
  }
  switch (types.at(binding)) {
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
      break;
    default:
      fprintf(
          stderr,
          "DescriptorSet::write(%u, VkBufferView): binding=%u has type %s\n",
          binding, binding, string_VkDescriptorType(types.at(binding)));
      return 1;
  }
  VkWriteDescriptorSet VkInit(w);
  w.dstSet = vk;
  w.dstBinding = binding;
  w.dstArrayElement = arrayI;
  w.descriptorType = types.at(binding);
  w.descriptorCount = texelBufferViewInfo.size();
  w.pTexelBufferView = texelBufferViewInfo.data();
  vkUpdateDescriptorSets(pool.dev.dev, 1, &w, 0, nullptr);
  return 0;
}

}  // namespace memory
