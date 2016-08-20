# Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
include $(TOP)lib/make/head.inc

TARGET_BIN=v
PKG_CONFIG_PKGS+=vulkan
LIBS+=$(TOP)lib/language/liblanguage.a
LIBS+=$(TOP)lib/command/libcommand.a
SRCS=main.cpp
HDRS=
HDRS+=$(foreach f,language.h VkPtr.h VkEnum.h,$(TOP)lib/language/$(f))
HDRS+=$(TOP)lib/command/command.h
HDRS+=$(foreach f,include/GLFW/glfw3.h VulkanSamples/include/vulkan/vulkan.h,$(TOP)vendor/$(f))
GLSL+=main.vert
GLSL+=main.frag

include $(TOP)lib/make/tail.inc
