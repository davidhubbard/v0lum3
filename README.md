# v0lum3

v0lum3 is a GPL3 library to create voxel worlds using Vulkan

(Actually, right now, it just implements vulkan-tutorial.com's triangle app.)

# build.sh

The only really interesting thing in this repo right now is
the `build.sh` script. It builds [VulkanSamples](https://github.com/LunarG/VulkanSamples)
which has [LoaderAndValidationLayers](https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers)
built into it.

It's there to make it easy to build this project but it is also super useful
for anyone who wants to quickly create a clean Vulkan dev environment:

1. Clone this repo

2. Throw away everything but `vendor`, `.gitmodules` and `build.sh`.

3. Profit!

## vulkantools.sh and lunarglass.sh

Oh yeah, if you want to run:

* `vktrace`

* `vkreplay`

* `vktraceviewer` (requires Qt-svg)

Then type `vendor/vulkantools.sh` and those will be built and installed
in `vendor/bin` next to `glslangValidator` and friends.

For [LunarGLASS](https://github.com/LunarG/LunarGLASS) type
`vendor/lunarglass.sh` and it will be built in `vendor/LunarGLASS`.

## Vulkan Conformance Test Suite (CTS)

For the [Vulkan CTS](https://github.com/KhronosGroup/VK-GL-CTS/)
type `vendor/vulkancts.sh` and it will be built in `vendor/cts`.



The CTS takes about 2 hours to run all the default supplied tests. Failing
test cases would ideally be listed in a separate text file. Specify the text
file of test cases with the `--deqp-caselist-file=` parameter. (See
`vendor/vulkancts.sh` for more information.)

## Why VulkanSamples and not Vulkan-LoaderAndValidationLayers?

Two reasons:

1. Samples! VulkanSamples means you can examine / modify / run
   the samples written by LunarG, but you don't have to fiddle with them
   to get them to run first.

2. Slightly slower update rate than syncing directly to
   [LVL](https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers)
   HEAD

## Why not include SaschaWillems (or your favorite set of samples)

Attempting to strike a balance between providing some usable
reference binaries (to prove that Vulkan is "working" at some basic
level, to give you something to start from) but at the same time
leave the repo at a very minimal "blank slate," from which you
can build.

It is a design goal that this repo is easily converted over to using
LVL directly. It is also possible to bypass LVL and write your own
loader, but tests have not shown that to be a significant performance
gain.

## Notes

Should probably take a look at [CUDA Voxelizer 0.1](https://github.com/Forceflow/cuda_voxelizer)
