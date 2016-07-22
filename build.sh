#!/bin/bash
#
# Script to prepare and build v0lum3.
#
# Copyright (c) David Hubbard 2016. Licensed under GPLv3.

set -e

# See if submodules are up to date.
N=$( /bin/ls -1 vendor/glfw | wc -l )
if [ "$N" -eq "0" ]; then
  echo "git submodule update --init --recursive # (hint: just do git clone --recursive before running this)"
  git submodule update --init --recursive
fi

# A bash subshell is created with parens like this.
# We use subshells to keep 'cd' commands clean.
# The alternative is to try to cd ../../../.. in exactly the right places, which is messy.
(
  mkdir -p vendor/lib
  cd vendor/lib
  # Convince VulkanSamples/CMakeLists.txt that SPIRV-Tools is installed correctly.
  ln -sf . x86_64
)

# The entire tree under vendor/lib is in .gitignore.
# VulkanSamples does not ship with a .pc file, so throw one in there.
if [ ! -f vendor/lib/pkgconfig/vulkan.pc ]; then
  mkdir -p vendor/lib/pkgconfig
  cat <<EOF >vendor/lib/pkgconfig/vulkan.pc
prefix=${PWD}/vendor/VulkanSamples
includedir=\${prefix}/include
glm_includedir=\${prefix}/libs
loaderdir=\${prefix}/build/loader

Name: Vulkan
Description: High Performance Graphics API
Version: 1.0.21
URL: http://www.khronos.org/
Requires.private: glfw3
Libs: -L\${loaderdir} -lvulkan -Wl,-rpath=\${loaderdir}
Libs.private: -lrt -lm -ldl
Cflags: -I\${includedir} -I\${glm_includedir}
EOF
fi

# A nice benefit of installing a new vulkan.pc is that pkg-config --exists vulkan
# will run before even building VulkanSamples. On the other hand if the build does not succeed,
# pkg-config will be lying about the presence of vulkan libs!
if ! pkg-config --exists vulkan; then
  echo "WARNING: lib/pkgconfig/vulkan.pc is not in \$PKG_CONFIG_PATH"
  echo ""
  echo "WARNING: Please type: \"export PKG_CONFIG_PATH=${PWD}/vendor/lib/pkgconfig\""
  echo ""
  echo "WARNING: Recommend adding the above line to your .bashrc / .cshrc / .zshrc etc."
  exit 1
fi

# Get set up to build all the bits and bobs needed for Vulkan.
PREFIX="${PWD}/vendor"
NPROC=$( nproc )

cmake_it() {
  CMD="cmake -D CMAKE_INSTALL_PREFIX=${PREFIX} -D CMAKE_BUILD_TYPE="
  # while [] printf '%q'; shift is a workaround: $* will split the args but then $CMD
  # will break if any of the args contain bashisms (space, backslash, ampersand, etc.).
  while [ -n "$1" ]; do
    CMD="$CMD$( printf '%q' $1 ) "
    shift
  done
  echo ""
  echo -e "\e[1m$CMD\e[0m .."
  $CMD ..
}

(
  cd vendor/VulkanSamples
  git apply ../VulkanSamples_clean_up_logging.patch.txt
)

# No need to build SPIRV-Headers here. It is used by SPIRV-Tools in source form.
for submod in glslang SPIRV-Tools glfw VulkanSamples; do
  mkdir -p "vendor/${submod}/build"
  (
    cd "vendor/${submod}/build"
    if [ ! -f CMakeCache.txt ]; then
      CMC="cmake -D CMAKE_INSTALL_PREFIX=${PREFIX} -D CMAKE_BUILD_TYPE"
      case "${submod}" in
        "SPIRV-Tools")
          cmake_it Release -D SPIRV-Headers_SOURCE_DIR=${PREFIX}/SPIRV-Headers
          ;;
        "glslang")
          (
            cd ..
            git apply ../VulkanSamples/glslang_revert_a5c33d.patch.txt
          )
          cmake_it Release
          ;;
        *)
          cmake_it Debug
          ;;
      esac
    fi
    # Though tempting to write a makefile that does sub-makes for each of the targets,
    # calling make immediately is simplest in terms of debugging build issues.
    make -j$NPROC install
  )
done

echo ""
echo "Success. Things to try next:"
echo "  vendor/VulkanSamples/build/demos/vulkaninfo 2>&1 | head -n40"
echo "  make -C main"
