#!/bin/bash
#
# Script to prepare and build v0lum3.
#
# Copyright (c) David Hubbard 2016. Licensed under GPLv3.

set -e

cd $( dirname $0 )

# see if depot_tools are installed
if ! which gn >/dev/null 2>&1; then
  echo "Please install depot_tools and add it to your PATH."
  echo "See http://www.chromium.org/developers/how-tos/install-depot-tools"
  exit 1
fi

# See if submodules are up to date.
N=$( /bin/ls -1 vendor/glfw | wc -l )
if [ "$N" -eq "0" ]; then
  echo "git submodule update --init --recursive"
  git submodule update --init --recursive --depth 51
fi

# patch glfw to use libvulkan.so instead of libvulkan.so.1
# and remove its version of vulkan.h
sed -i -e 's/\(_glfw_dlopen("libvulkan.so\).1")/\1")/g' vendor/glfw/src/vulkan.c
rm -r vendor/glfw/deps/vulkan

# patch skia to use libvulkan.so instead of lib/libvulkan.so
# and to depend on VulkanSamples (outside the skia tree)
sed -i -e 's/\(skia_vulkan_sdk = \)getenv("VULKAN_SDK")/\1root_out_dir/' \
	-e 's:\(lib_dirs [+]= \[ "$skia_vulkan_sdk/\)lib/" \]:\1" ]:' \
	-e '/":arm64",/i\    "//vendor/VulkanSamples:vulkan_headers",' \
	vendor/skia/BUILD.gn
# patch skia to honor {{output_dir}} in toolchain("gcc_like")
# this is needed by //vendor/VulkanSamples because libVkLayer_*.so must be
# installed with the layer json files, and that crowds {{root_out_dir}}.
patch --no-backup-if-mismatch -p1 <<EOF
--- a/vendor/skia/gn/BUILD.gn
+++ b/vendor/skia/gn/BUILD.gn
@@ -698,8 +698,9 @@ toolchain("gcc_like") {
 
     command = "\$cc_wrapper \$cxx -shared {{ldflags}} {{inputs}} {{solibs}} {{libs}} \$rpath -o {{output}}"
     outputs = [
-      "{{root_out_dir}}/\$soname",
+      "{{output_dir}}/\$soname",
     ]
+    default_output_dir = "{{root_out_dir}}"
     output_prefix = "lib"
     default_output_extension = ".so"
     description = "link {{output}}"
EOF

echo ""
echo "vendor/skia/tools/git-sync-deps"
(
  cd vendor/skia
  python tools/git-sync-deps
)

# All first-level files under gn must exactly match vendor/skia/gn (because
# chromium's gn does not have a one-size-fits all definition, a project with
# a submodule that uses gn -- like vendor/skia -- must keep all rules in
# lock-step between all submodules!)
#
# Note that .gn and gn/vendor are unique to v0lum3 and not part of skia.
cp vendor/skia/gn/* gn/
( cd vendor/skia && tar cf - buildtools/* ) | tar xf -

# install fetch-gn and use it to download a python script in the buildtools dir
cp vendor/skia/bin/fetch-gn gn/

mkdir bin # fetch-gn is from vendor/skia where there is a bin dir
gn/fetch-gn
rm -r bin # remove bin dir

echo ""
# vendor/skia/BUILD.gn assumes it is the source root, an assumption that can
# be simulated except when it attempts its third_party integration.
# This hack fixes third_party for vendor/skia.
ln -s vendor/skia/third_party third_party
gn gen out/Debug
ninja -C out/Debug

