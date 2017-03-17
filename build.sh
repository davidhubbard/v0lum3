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
  git submodule update --init --recursive
fi

# patch glfw to use libvulkan.so instead of libvulkan.so.1
sed -i -e 's/\(_glfw_dlopen("libvulkan.so\).1")/\1")/g' vendor/glfw/src/vulkan.c

# patch skia to use libvulkan.so instead of lib/libvulkan.so
sed -i -e 's/\(skia_vulkan_sdk = \)getenv("VULKAN_SDK")/\1root_out_dir/' \
	-e 's:\(lib_dirs [+]= \[ "$skia_vulkan_sdk/\)lib/" \]:\1" ]:' \
	vendor/skia/BUILD.gn

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

