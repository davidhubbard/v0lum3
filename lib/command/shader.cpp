/* Copyright (c) David Hubbard 2017. Licensed under the GPLv3.
 */
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "command.h"

namespace command {

int Shader::loadSPV(const void* spvBegin, const void* spvEnd) {
  VkShaderModuleCreateInfo VkInit(smci);
  smci.codeSize = reinterpret_cast<const char*>(spvEnd) -
                  reinterpret_cast<const char*>(spvBegin);
  if (smci.codeSize % 4 != 0) {
    fprintf(stderr, "LoadSPV(%p, %p) size %zu is invalid\n", spvBegin, spvEnd,
            smci.codeSize);
    return 1;
  }

  smci.pCode = reinterpret_cast<const uint32_t*>(spvBegin);
  VkResult r = vkCreateShaderModule(vkdev, &smci, nullptr, &vk);
  if (r != VK_SUCCESS) {
    fprintf(stderr, "loadSPV(%p, %p) vkCreateShaderModule returned %d (%s)\n",
            spvBegin, spvEnd, r, string_VkResult(r));
    return 1;
  }
  return 0;
}

int Shader::loadSPV(const char* filename) {
  int infile = open(filename, O_RDONLY);
  if (infile < 0) {
    fprintf(stderr, "loadSPV: open(%s) failed: %d %s\n", filename, errno,
            strerror(errno));
    return 1;
  }
  struct stat s;
  if (fstat(infile, &s) == -1) {
    fprintf(stderr, "loadSPV: fstat(%s) failed: %d %s\n", filename, errno,
            strerror(errno));
    return 1;
  }
  char* map =
      (char*)mmap(0, s.st_size, PROT_READ, MAP_SHARED, infile, 0 /*offset*/);
  if (!map) {
    fprintf(stderr, "loadSPV: mmap(%s) failed: %d %s\n", filename, errno,
            strerror(errno));
    close(infile);
    return 1;
  }

  int r = loadSPV(map, map + s.st_size);
  if (munmap(map, s.st_size) < 0) {
    fprintf(stderr, "loadSPV: munmap(%s) failed: %d %s\n", filename, errno,
            strerror(errno));
    return 1;
  }
  if (close(infile) < 0) {
    fprintf(stderr, "loadSPV: close(%s) failed: %d %s\n", filename, errno,
            strerror(errno));
    return 1;
  }
  return r;
}

Shader& PipelineCreateInfo::addShader(language::Device& dev,
                                      RenderPass& renderPass,
                                      VkShaderStageFlagBits stageBits,
                                      std::string entryPointName) {
  renderPass.shaders.emplace_back(dev);
  auto& shader = *(renderPass.shaders.end() - 1);

  stages.emplace_back();
  auto& pipelinestage = *(stages.end() - 1);
  pipelinestage.sci.stage = stageBits;
  pipelinestage.shader_i = renderPass.shaders.size() - 1;
  pipelinestage.entryPointName = entryPointName;

  return shader;
}

}  // namespace command
