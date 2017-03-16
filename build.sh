#!/bin/bash
#
# Script to prepare and build v0lum3.
#
# Copyright (c) David Hubbard 2016. Licensed under GPLv3.

set -e

cd $( dirname $0 )

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
  rm -f x86_64
  ln -s . x86_64
)

# The entire tree under vendor/lib is in .gitignore.
# VulkanSamples does not ship with a .pc file, so throw one in there.
PC=vendor/lib/pkgconfig/vulkan.pc
mkdir -p vendor/lib/pkgconfig
cat <<EOF >"${PC}"
prefix=${PWD}/vendor/VulkanSamples
includedir=\${prefix}/include
glm_includedir=\${prefix}/libs
loaderdir=${PWD}/vendor/lib

Name: Vulkan
Description: High Performance Graphics API
Version: 1.0.21
URL: http://www.khronos.org/
Requires.private: glfw3
Libs: -L\${loaderdir} -lvulkan -Wl,-rpath=\${loaderdir}
Libs.private: -lrt -lm -ldl
Cflags: -I\${includedir} -I\${glm_includedir}
EOF

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
  if [ -f ../VulkanSamples_*.patch.txt ] && [ ! -f v0lum3-build-script-patches-done ]; then
    git apply ../VulkanSamples_*.patch.txt
    touch v0lum3-build-script-patches-done
  fi
)

# No need to build SPIRV-Headers here. It is used by SPIRV-Tools in source form.
for submod in glfw glslang SPIRV-Tools VulkanSamples; do
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
          cmake_it Release
          ;;
        "VulkanSamples")
          cmake_it Debug -DBUILD_WSI_MIR_SUPPORT=0 -DBUILD_WSI_WAYLAND_SUPPORT=0
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

  case "${submod}" in
    "glfw")
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

      # build libpng in parallel
      if [ ! -f vendor/libpng/libpng-build-success ]; then
      (
        cd vendor/libpng
        if ! grep -q PNG_SETJMP_SUPPORTED pngusr.dfa; then
          cat <<EOF >>pngusr.dfa
@#undef PNG_SETJMP_SUPPORTED
@#define PNG_NO_SETJMP
@#undef PNG_SIMPLIFIED_READ_SUPPORTED
@#define PNG_NO_SIMPLIFIED_READ
@#undef PNG_SIMPLIFIED_WRITE_SUPPORTED
@#define PNG_NO_SIMPLIFIED_WRITE
EOF
          # contrib/libtests/timepng.c depends on setjmp but does not check if
          # PNG_SETJMP_SUPPORTED is defined. Patch it out.
          sed -i -e 's/\(defined(PNG_STDIO_SUPPORTED)\)/\1 \&\& 0/' \
            contrib/libtests/timepng.c
        fi
        ./autogen.sh
        ./configure --prefix="${PREFIX}" --disable-shared --enable-static
        make -j$NPROC
        make install
        touch libpng-build-success
      ) > /tmp/libpng-build.log 2>&1 &
      echo $! > /tmp/libpng-build.pid
      fi
      ;;
  esac
done

# update vulkan.pc with C_DEFINES generate by cmake
VSB=vendor/VulkanSamples/build
FLAGS="${VSB}/loader/CMakeFiles/vulkan.dir/flags.make"
if ! grep -q ^C_DEFINES "${FLAGS}"; then
  echo "Fatal: did VulkanSamples build?"
  echo "  ${FLAGS}: C_DEFINES not found"
  exit 1
fi
CFLAGS=$(sed -e 's/^C_DEFINES = //p;d' "${FLAGS}")

sed -e "s|^\\(Cflags:.*\\)$|\\1 ${CFLAGS}|" "${PC}" \
	> /tmp/vulkan.pc.tmp
if diff /tmp/vulkan.pc.tmp "${PC}" >/dev/null 2>&1; then
  echo "Fatal: unable to patch Cflags: line in ${PC}"
  rm /tmp/vulkan.pc.tmp
  exit 1
fi
mv /tmp/vulkan.pc.tmp "${PC}"

LIBPNG_BUILD_PID=$(cat /tmp/libpng-build.pid)
if [ -n "$LIBPNG_BUILD_PID" ]; then
  echo "still building libpng..."
  wait $LIBPNG_BUILD_PID || true
  rm /tmp/libpng-build.pid
fi
if [ ! -f vendor/libpng/libpng-build-success ]; then
  echo ""
  echo "Failed to build vendor/libpng. See /tmp/libpng-build.log"
  exit 1
fi

echo ""
echo "Success. Things to try next:"
echo "  ${VSB}/demos/vulkaninfo 2>&1 | head -n40"
echo "  make -j$NPROC"
