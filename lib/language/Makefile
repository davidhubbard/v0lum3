# Copyright (c) David Hubbard 2016. Licensed under the GPLv3.
include $(TOP)lib/make/head.inc

TARGET_LIB=liblanguage.a
PKGCONFIG_PKGS+=vulkan

HDRS+=language.h
HDRS+=VkPtr.h
HDRS+=VkInit.h
HDRS+=VkEnum.h

SRCS+=deviceinit.cpp
SRCS+=language.cpp
SRCS+=requestqfams.cpp
SRCS+=swapchain.cpp
SRCS+=VkEnum.cpp

include $(TOP)lib/make/tail.inc
