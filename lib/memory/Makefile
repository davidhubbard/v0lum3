# Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
include $(TOP)lib/make/head.inc

TARGET_LIB=libmemory.a
PKG_CONFIG_PKGS+=vulkan

HDRS=memory.h
HDRS+=$(foreach f,language.h VkInit.h,$(TOP)lib/language/$(f))

SRCS+=memory.cpp

include $(TOP)lib/make/tail.inc
