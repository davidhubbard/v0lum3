# Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
include $(TOP)lib/make/head.inc

TARGET_LIB=libcommand.a
PKG_CONFIG_PKGS+=vulkan

HDRS=command.h
HDRS+=$(foreach f,language.h VkInit.h,$(TOP)lib/language/$(f))

SRCS+=command.cpp
SRCS+=fence.cpp
SRCS+=pipeline.cpp

include $(TOP)lib/make/tail.inc
