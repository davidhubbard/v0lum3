/* Copyright (c) David Hubbard 2017. Licensed under the GPLv3.
 *
 * This file is not built under all configs. It is only built if
 * "use_spirv_cross_reflection" is enabled, since it adds a
 * dependency on spirv_cross.
 */
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vendor/spirv_cross/spirv_glsl.hpp>
#include "science.h"
#include <map>

#ifndef USE_SPIRV_CROSS_REFLECTION
#error USE_SPIRV_CROSS_REFLECTION should be defined if this file is being built.
#endif

using namespace command;
using namespace std;

namespace science {

struct ShaderBytes {
  ShaderBytes(const void* bytes, uint32_t len)
    : bytes{(const uint32_t *) bytes,
            (const uint32_t *) ((const char*) bytes + len)} {}
  vector<uint32_t> bytes;
};

struct ShaderLibraryInternal {
  ShaderLibraryInternal(ShaderLibrary* self) : self{*self} {}

  int addShaderBytes(shared_ptr<Shader> shader, const void* bytes,
                     uint32_t len) {
    auto result = shaderBytes.emplace(make_pair(shader, ShaderBytes{bytes, len}));
    if (!result.second) {
      fprintf(stderr, "ShaderLibrary::load: shader already exists\n");
      return 1;
    }
    return 0;
  }

  map<shared_ptr<Shader>, ShaderBytes> shaderBytes;
  uint32_t allStageBits{0};
  vector<VkDescriptorSetLayoutBinding> layoutBindings;
  ShaderLibrary& self;
};

shared_ptr<Shader> ShaderLibrary::load(const void* spvBegin,
                                       const void* spvEnd) {
  if (!_i) {
    _i = new ShaderLibraryInternal(this);
  }

  auto shader = shared_ptr<Shader>(new Shader(pool.dev));
  if (shader->loadSPV(spvBegin, spvEnd) ||
      _i->addShaderBytes(shader, spvBegin,
                         (const char*)spvEnd - (const char*)spvBegin)) {
    // Failed. Return a null shared_ptr.
    return shared_ptr<Shader>();
  }
  return shader;
}

shared_ptr<Shader> ShaderLibrary::load(const char* filename) {
  if (!_i) {
    _i = new ShaderLibraryInternal(this);
  }

  int infile = open(filename, O_RDONLY);
  if (infile < 0) {
    fprintf(stderr, "ShaderLibrary::load: open(%s) failed: %d %s\n", filename,
            errno, strerror(errno));
    // Failed. Return a null shared_ptr.
    return shared_ptr<Shader>();
  }
  struct stat s;
  if (fstat(infile, &s) == -1) {
    fprintf(stderr, "ShaderLibrary::load: fstat(%s) failed: %d %s\n", filename,
            errno, strerror(errno));
    // Failed. Return a null shared_ptr.
    return shared_ptr<Shader>();
  }
  char* map =
      (char*)mmap(0, s.st_size, PROT_READ, MAP_SHARED, infile, 0 /*offset*/);
  if (!map) {
    fprintf(stderr, "ShaderLibrary::load: mmap(%s) failed: %d %s\n", filename,
            errno, strerror(errno));
    close(infile);
    // Failed. Return a null shared_ptr.
    return shared_ptr<Shader>();
  }

  auto shader = shared_ptr<Shader>(new Shader(pool.dev));
  int r = shader->loadSPV(map, map + s.st_size);
  if (!r && _i->addShaderBytes(shader, map, s.st_size)) {
    r = 1;
  }
  if (munmap(map, s.st_size) < 0) {
    fprintf(stderr, "ShaderLibrary::load: munmap(%s) failed: %d %s\n", filename,
            errno, strerror(errno));
    close(infile);
    // Failed. Return a null shared_ptr.
    return shared_ptr<Shader>();
  }
  if (close(infile) < 0) {
    fprintf(stderr, "ShaderLibrary::load: close(%s) failed: %d %s\n", filename,
            errno, strerror(errno));
    // Failed. Return a null shared_ptr.
    return shared_ptr<Shader>();
  }
  if (r) {
    // shader->loadSPV() or _i->addShaderBytes() failed. Return a null
    // shared_ptr.
    return shared_ptr<Shader>();
  }
  return shader;
}

int ShaderLibrary::stage(RenderPass& renderPass, PipeBuilder& pipe,
                         VkShaderStageFlagBits stageBits,
                         shared_ptr<Shader> shader,
                         string entryPointName /*= "main"*/) {
  if (!_i) {
    _i = new ShaderLibraryInternal(this);
  }
  _i->allStageBits |= stageBits;
  return pipe.pipeline.info.addShader(shader, pool.dev, renderPass, stageBits,
                                      entryPointName);
}

int ShaderLibrary::binding(int bindPoint, memory::Sampler& sampler) {
  if (!_i) {
    _i = new ShaderLibraryInternal(this);
  }

  VkDescriptorSetLayoutBinding VkInit(layoutBinding);
  layoutBinding.binding = bindPoint;
  layoutBinding.descriptorCount = 1;
  layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  layoutBinding.pImmutableSamplers = nullptr;
  //layoutBinding.stageFlags is set when all layoutBindings are assembled.
  _i->layoutBindings.emplace_back(layoutBinding);
  return 0;
}

int ShaderLibrary::binding(int bindPoint, memory::UniformBuffer& uniform) {
  if (!_i) {
    _i = new ShaderLibraryInternal(this);
  }

  VkDescriptorSetLayoutBinding VkInit(layoutBinding);
  layoutBinding.binding = bindPoint;
  layoutBinding.descriptorCount = 1;
  layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  layoutBinding.pImmutableSamplers = nullptr;
  //layoutBinding.stageFlags is set when all layoutBindings are assembled.
  _i->layoutBindings.emplace_back(layoutBinding);
  return 0;
}

int ShaderLibrary::getLayout(VkDescriptorSetLayout* pDescriptorSetLayout) {
  if (!_i) {
    fprintf(stderr, "BUG: getLayout with no bindings\n");
    return 1;
  }
  for (auto& binding : _i->layoutBindings) {
    // This could be more efficient if binding bits could be broken down
    // by stage.
    binding.stageFlags = static_cast<VkShaderStageFlagBits>(_i->allStageBits);
  }

  VkDescriptorSetLayoutCreateInfo VkInit(layoutInfo);
  layoutInfo.bindingCount = _i->layoutBindings.size();
  layoutInfo.pBindings = _i->layoutBindings.data();

  VkResult v = vkCreateDescriptorSetLayout(
            pool.dev.dev, &layoutInfo, pool.dev.dev.allocator, pDescriptorSetLayout);
  if (v != VK_SUCCESS) {
    fprintf(stderr, "vkCreateDescriptorSetLayout failed: %d (%s)\n", v,
            string_VkResult(v));
    return 1;
  }
  return 0;
}

int ShaderLibrary::bindGraphicsPipeline(CommandBuilder& command,
                                        PipeBuilder& pipe,
                                        VkDescriptorSet descriptorSet) {
  return command.bindGraphicsPipelineAndDescriptors(pipe.pipeline, 0, 1,
                                                    &descriptorSet);
}

#if 0
    // Decompile the shader and dump info.
    spirv_cross::CompilerGLSL compiler(std::vector<uint32_t>{
        spv_main_vert,
        spv_main_vert + sizeof(spv_main_vert) / sizeof(spv_main_vert[0])});
    spirv_cross::ShaderResources resources = compiler.get_shader_resources();
    for (size_t i = 0; i < resources.stage_inputs.size(); i++) {
      fprintf(stderr, "stage_inputs[%zu]:\n", i);
      auto& res = resources.stage_inputs.at(i);
      fprintf(stderr, "  id=%u type_id=%u base_type_id=%u\n", res.id,
              res.type_id, res.base_type_id);
      fprintf(stderr, "  name=\"%s\"\n", res.name.c_str());
    }
#endif

ShaderLibrary::~ShaderLibrary() {
  if (_i) {
    delete _i;
  }
}

}  // namespace science
