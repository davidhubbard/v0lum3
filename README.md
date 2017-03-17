# v0lum3

v0lum3 is a GPL3 library to create voxel worlds using Vulkan

# build.sh

The edit-recompile-run process is done using
[`ninja`](https://chromium.googlesource.com/chromium/src/tools/gn/), like this:
```
vi main/main.cpp
ninja -C out/Debug
out/Debug/v
```

But after a clean checkout, please run `build.sh` first. Of course, it will
immediately ask you to install
[depot_tools](http://www.chromium.org/developers/how-tos/install-depot-tools).
This is a one-time setup step:

```
# do not start in another git repo!
# do this in your home dir or something
# so first type 'cd' to go to your home dir:
cd
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
# maybe edit your .bashrc to permanently setup the PATH:
export PATH=$PWD/depot_tools:$PATH
```

Since `depot_tools` does not auto-update, you may want to check for updates
manually (say, in 6 months) by typing:

```
gsutil.py update
```

Now you are ready for `build.sh`:

```
cd v0lum3
./build.sh
```

(See [an example](#buildsh-example) below)

It builds the following libraries with their dependencies:

* [VulkanSamples](https://github.com/LunarG/VulkanSamples) which has
  [LoaderAndValidationLayers](https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers)
  built into it

* [skia](https://skia.org). This is why `depot_tools`,
  [`ninja` and `gn`](https://chromium.googlesource.com/chromium/src/tools/gn/)
  are necessary.

# Fetching updates from upstream

To pull the latest commits from github, e.g. to prepare a pull request, you
will need to do a few cleanup steps to avoid having merge conflicts:

1. `( cd vendor/glfw && git checkout -- . )`
2. `( cd vendor/skia && git checkout -- . )`
3. Get the latest commits with `git fetch` + `git merge` (or some people use
   `git pull`)
4. Rebuild the project with `build.sh` (this will re-patch glfw and skia)

# `build.sh` Example

Here is a somewhat outdated example showing what `build.sh` does:

```
$ cd
~ $ mkdir src
~ $ cd src
src $ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
Cloning into 'depot_tools'...
remote: [snip]
Checking connectivity... done.
src $ export PATH=$PWD/depot_tools:$PATH
src $ git clone https://github.com/davidhubbard/v0lum3
Cloning into 'v0lum3'...
remote: Counting objects: 1008, done.
remote: Compressing objects: 100% (629/629), done.
remote: Total 1008 (delta 579), reused 593 (delta 358)
Receiving objects: 100% (1008/1008), 1.65 MiB | 0 bytes/s, done.
Resolving deltas: 100% (579/579), done.
Checking connectivity... done.
src $ cd v0lum3
v0lum3 $ ./update.sh
```
The script first checks out all the submodules.
```
git submodule update --init --recursive
Submodule 'vendor/VulkanSamples' (https://github.com/LunarG/VulkanSamples.git) registered for path 'vendor/VulkanSamples'
Submodule 'vendor/glfw' (https://github.com/glfw/glfw.git) registered for path 'vendor/glfw'
Submodule 'vendor/glslang' (https://github.com/KhronosGroup/glslang.git) registered for path 'vendor/glslang'
Submodule 'vendor/skia' (https://github.com/google/skia) registered for path 'vendor/skia'
Cloning into 'vendor/VulkanSamples'...
remote: Counting objects: 60790, done.
remote: Total 60790 (delta 0), reused 0 (delta 0), pack-reused 60790
Receiving objects: 100% (60790/60790), 56.64 MiB | 16.80 MiB/s, done.
Resolving deltas: 100% (45817/45817), done.
Checking connectivity... done.
Submodule path 'vendor/VulkanSamples': checked out '9fb1b977604cb36f211fa241212afe8aed79412f'
Cloning into 'vendor/glfw'...
remote: Counting objects: 20499, done.
remote: Compressing objects: 100% (38/38), done.
remote: Total 20499 (delta 17), reused 0 (delta 0), pack-reused 20461
Receiving objects: 100% (20499/20499), 9.39 MiB | 12.39 MiB/s, done.
Resolving deltas: 100% (14172/14172), done.
Checking connectivity... done.
Submodule path 'vendor/glfw': checked out '798d7c6d68bb17480fcda6074b1d5a946a05ed52'
Cloning into 'vendor/glslang'...
remote: Counting objects: 23002, done.
remote: Compressing objects: 100% (37/37), done.
remote: Total 23002 (delta 12), reused 0 (delta 0), pack-reused 22965
Receiving objects: 100% (23002/23002), 33.49 MiB | 16.03 MiB/s, done.
Resolving deltas: 100% (19217/19217), done.
Checking connectivity... done.
Submodule path 'vendor/glslang': checked out '8f674e821e1e5f628474b21d7fe21af2e86b5fb4'
Cloning into 'vendor/skia'...
remote: Counting objects: 280231, done.
remote: Compressing objects: 100% (20/20), done.
remote: Total 280231 (delta 6), reused 1 (delta 1), pack-reused 280210
Receiving objects: 100% (280231/280231), 205.79 MiB | 13.90 MiB/s, done.
Resolving deltas: 100% (228855/228855), done.
Checking connectivity... done.
Submodule path 'vendor/skia': checked out '86271240068a8c4eef24dcd4454dff4d2dc64faf'
```
Then it asks `skia` to check out its submodules.
```
vendor/skia/tools/git-sync-deps
third_party/externals/piex           @ 8f540f64b6c170a16fb7e6e52d61819705c1522a
third_party/externals/dng_sdk        @ 96443b262250c390b0caefbf3eed8463ba35ecae
common                               @ 9737551d7a52c3db3262db5856e6bcd62c462b92
buildtools                           @ 55ad626b08ef971fd82a62b7abb325359542952b
third_party/externals/expat          @ android-6.0.1_r55
third_party/externals/spirv-headers  @ 2d6ba39368a781edd82eff5df2b6bc614e892329
third_party/externals/zlib           @ 4576304a4b9835aa8646c9735b079e1d96858633
third_party/externals/spirv-tools    @ 1fb8c37b5718118b49eec59dc383cfa3f98643c0
third_party/externals/jsoncpp        @ 1.0.0
third_party/externals/harfbuzz       @ 1.4.2
third_party/externals/imgui          @ 6384eee34f08cb7eab8d835043e1738e4adcdf75
third_party/externals/libjpeg-turbo  @ 6de58e0d28014caf2fc1370145f22fd6d65f63e3
third_party/externals/freetype       @ 447a0b62634802d8acdb56008cff5ff4e50be244
third_party/externals/libwebp        @ v0.6.0
third_party/externals/microhttpd     @ 748945ec6f1c67b7efc934ab0808e1d32f2fb98d
third_party/externals/angle2         @ 57f17473791703ac82add77c3d77d13d8e2dfbc4
third_party/externals/sfntly         @ b18b09b6114b9b7fe6fc2f96d8b15e8a72f66916
third_party/externals/sdl            @ 9b526d28cb2d7f0ccff0613c94bb52abc8f53b6f
third_party/externals/icu            @ ec9c1133693148470ffe2e5e53576998e3650c1d
```
Then it runs `gn gen` to generate build.ninja files.
```
Done. Made 93 targets from 43 files in 36ms
```
Then it runs `ninja` to build the application.
```
ninja: Entering directory `out/Debug'
[63/2479] compile ../../third_party/externals/dng_sdk/source/dng_safe_arithmetic.cpp
```
When it is done building skia submodules, it builds skia proper.
```
[783/2479] compile ../../vendor/skia/bench/HairlinePathBench.cpp
```
When all done it says:
```
[2479/2479] stamp obj/root.stamp
v0lum3 $
```

# Adopting build.sh to your project

`build.sh` builds the project but it is also super useful for anyone who
wants to quickly create a clean Vulkan dev environment:

1. Clone this repo

2. Throw away everything under `main`.

3. Write your own application by customizing `BUILD.gn` in the top level
   directory.

## Where can I get skpmaker?

`skpmaker` just generates a single filled rectangle. Other, easier ways to
produce sample skp files are:

* The skia.org docs for SkCanvas have a
  [number of examples](https://skia.org/user/api/skcanvas). Launch one in
  under fiddle.skia.org and download the skp by clicking the "skp" link.
* The skia.org Tips & FAQ includes how to
  [capture a `.skp` file on a web page](https://skia.org/user/tips#skp-capture).

Or apply this patch to `vendor/skia/BUILD.gn`:
```
--- a/BUILD.gn
+++ b/BUILD.gn
@@ -1186,6 +1186,16 @@ if (skia_enable_tools) {
     ]
   }
 
+  test_app("skpmaker") {
+    sources = [
+      "tools/skpmaker.cpp",
+    ]
+    deps = [
+      ":flags",
+      ":skia",
+    ]
+  }
+
   if (is_linux || is_win || is_mac) {
     test_app("SampleApp") {
       sources = [
```

## Why not use gclient?

The `gn` developers have voiced some support for use outside of chromium.
gclient does not seem to have such an indication, though there are many
skia clients that use it anyway.

## What about vulkantools.sh and lunarglass.sh?

Oh yeah, if you want to run:

* `vktrace`

* `vkreplay`

* `vktraceviewer` (requires Qt-svg)

Then type `vendor/vulkantools.sh` and those will be built and installed
in `vendor/bin` next to `glslangValidator` and friends.

For [LunarGLASS](https://github.com/LunarG/LunarGLASS) type
`vendor/lunarglass.sh` and it will be built in `vendor/LunarGLASS`.

## What about the Vulkan Conformance Test Suite (CTS)?

To run the [Vulkan CTS](https://github.com/KhronosGroup/VK-GL-CTS/)
type `vendor/vulkancts.sh` and it will be built in `vendor/cts`.
After a successful build, `vendor/vulkancts.sh` prints instructions
on running the CTS.

A full run takes about 2 hours. To avoid running all cases again,
locate the test log and look for results that are not
`StatusCode="Pass"` or `StatusCode="NotSupported"`.

Then put just these interesting test cases in a separate text file,
so that the CTS does not take as long to run. Specify a text file of
test cases with the `--deqp-caselist-file=` parameter. (See
`vendor/vulkancts.sh` for more information.)

## Why VulkanSamples and not Vulkan-LoaderAndValidationLayers?

Two reasons:

1. Samples! VulkanSamples means you can examine / modify / run
   the samples written by LunarG, but you don't have to fiddle with them
   to get them to run first.

2. Slightly slower update rate than syncing directly to
   [LVL](https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers)
   HEAD makes it less work to maintain as a submodule.

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
