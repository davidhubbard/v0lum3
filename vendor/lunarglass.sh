#!/bin/bash
#
# This script will build and install LunarGLASS

set -e

cd $( dirname "$0" )

git clone https://github.com/LunarG/LunarGLASS
PREFIX="${PWD}"

(
  cd LunarGLASS
  mkdir -p Core/LLVM
  (
    cd Core/LLVM
    wget http://llvm.org/releases/3.4/llvm-3.4.src.tar.gz
    tar --gzip -xf llvm-3.4.src.tar.gz
    git checkout -f .  # put back the LunarGLASS versions of some LLVM files
    cd llvm-3.4
    mkdir -p build
    cd build
    ../configure --enable-terminfo=no --enable-curses=no
    REQUIRES_RTTI=1 make -j $(nproc)
    make install DESTDIR="${PWD}"/install
  )

  cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release "-DGLSLANGLIBS=${PREFIX}/lib" "-DCMAKE_INSTALL_PREFIX=${PREFIX}"
  cd build
  make -j $(nproc) install
)
