# v0lum3

v0lum3 is a GPL3 library to create voxel worlds using Vulkan

(Actually, right now, it just implements vulkan-tutorial.com's triangle app.)

# build.sh

The only really interesting thing in this repo right now is
the `build.sh` script. It builds [VulkanSamples](https://github.com/LunarG/VulkanSamples)
which has [LoaderAndValidationLayers](https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers)
built into it.

Its purpose is to make it easy to link against the Vulkan loader, but
it may be useful if you want to quickly create a clean Vulkan dev environment:

1. Clone this repo

2. Throw away everything but the vendor dir and the build.sh script.

3. Profit!
