#!/bin/bash
#
# This script will build and install VulkanTools, which adds:
#   VK_LAYER_LUNARG_vktrace: record and replay your app's vulkan calls.
#           https://github.com/LunarG/VulkanTools/tree/master/vktrace
#   VK_LAYER_LUNARG_screenshot
#
# (There are also some example layers, see
# https://github.com/LunarG/VulkanTools/tree/master/layersvt)
#
# VulkanTools duplicates some of Vulkan-LoaderAndValidationLayers, but this
# script only installs the unique parts into this directory (which, by you
# exporting PKG_CONFIG_PATH, and by patching VulkanSamples to look in this dir,
# should "just work". Even vkreplay and vktraceviewer run without hacking
# your LD_LIBRARY_PATH.)
