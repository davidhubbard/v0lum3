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

set -e

cd $( dirname "$0" )

git clone https://github.com/LunarG/VulkanTools
LUNARGLASS_REVISION=$(cat VulkanTools/LunarGLASS_revision)
PREFIX="${PWD}"

if [ -d LunarGLASS ]; then
  echo "Directory LunarGLASS already exists. Skipping..."
else
  git clone https://github.com/LunarG/LunarGLASS

  (
    cd LunarGLASS
    mkdir -p Core/LLVM
    (
      cd Core/LLVM
      wget http://llvm.org/releases/3.4/llvm-3.4.src.tar.gz
      tar --gzip -xf llvm-3.4.src.tar.gz
      git checkout -f .  # put back the LunarGLASS versions of some LLVM files
      git checkout $LUNARGLASS_REVISION
    )
  )
  cp -R VulkanTools/LunarGLASS/* LunarGLASS

  (
    cd LunarGLASS/Core/LLVM/llvm-3.4
    mkdir -p build
    cd build
    ../configure --enable-terminfo=no --enable-curses=no
    REQUIRES_RTTI=1 make -j $(nproc)
    make install DESTDIR="${PWD}"/install
  )

  (
    cd LunarGLASS
    cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release "-DGLSLANGLIBS=${PREFIX}/lib" "-DCMAKE_INSTALL_PREFIX=${PREFIX}"
    cd build
    make -j $(nproc) install
  )
fi

(
  cd VulkanTools
  sed -i -e 's,component(LUNARGLASS_PREFIX external/LunarGLASS ABSOLUTE,component(LUNARGLASS_PREFIX ../LunarGLASS ABSOLUTE,' CMakeLists.txt


  #
  # install VkLayer files that are VulkanTools layers
  # (VulkanTools is built on Vulkan-LoaderAndValidationLayers and re-builds dups of those layers)
  #
  # use sed because the api_version line may change (has changed before)
  # and it broke the patch...so this is a bad candidate for a patchfile.
  sed -i -e 's/"library_path": "..\/vktrace\/libVkLayer_vktrace_layer.so"/"library_path": ".\/libVkLayer_vktrace_layer.so"/' \
      vktrace/src/vktrace_layer/linux/VkLayer_vktrace_layer.json
  # use patch for the rest
  patch -p1 <<EOF
diff --git a/layersvt/CMakeLists.txt b/layersvt/CMakeLists.txt
index 32ee1c4..b6414c3 100644
--- a/layersvt/CMakeLists.txt
+++ b/layersvt/CMakeLists.txt
@@ -65,6 +65,11 @@ else()
                 )
         endforeach(config_file)
     endif()
+    foreach (config_file \${LAYER_JSON_FILES})
+        install (FILES \${CMAKE_CURRENT_SOURCE_DIR}/linux/\${config_file}.json
+            DESTINATION etc/vulkan/explicit_layer.d
+            )
+    endforeach(config_file)
 endif()
 
 if (WIN32)
@@ -83,8 +88,11 @@ else()
     add_library(VkLayer_\${target} SHARED \${ARGN})
     target_link_Libraries(VkLayer_\${target} VkLayer_utilsvt)
     add_dependencies(VkLayer_\${target} generate_vt_helpers)
-    set_target_properties(VkLayer_\${target} PROPERTIES LINK_FLAGS "-Wl,-Bsymbolic")
+    set_target_properties(VkLayer_\${target}
+        PROPERTIES LINK_FLAGS "-Wl,-Bsymbolic"
+            INSTALL_RPATH \${CMAKE_INSTALL_PREFIX}/etc/vulkan/explicit_layer.d)
     install(TARGETS VkLayer_\${target} DESTINATION \${PROJECT_BINARY_DIR}/install_staging)
+    install(TARGETS VkLayer_\${target} DESTINATION etc/vulkan/explicit_layer.d)
     endmacro()
 endif()
 
diff --git a/vktrace/src/vktrace_layer/CMakeLists.txt b/vktrace/src/vktrace_layer/CMakeLists.txt
index c2f5d35..b6f5c1d 100644
--- a/vktrace/src/vktrace_layer/CMakeLists.txt
+++ b/vktrace/src/vktrace_layer/CMakeLists.txt
@@ -86,15 +86,16 @@ include_directories(
 # copy/link layer json file into build/layersvt directory
 if (NOT WIN32)
     # extra setup for out-of-tree builds
+    FILE(TO_NATIVE_PATH \${CMAKE_CURRENT_SOURCE_DIR}/linux/VkLayer_vktrace_layer.json src_json)
     if (NOT (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR))
         add_custom_target(vktrace_layer-json ALL
-            COMMAND ln -sf \${CMAKE_CURRENT_SOURCE_DIR}/linux/VkLayer_vktrace_layer.json \${CMAKE_RUNTIME_OUTPUT_DIRECTORY}../layersvt/
+            COMMAND ln -sf \${src_json} \${CMAKE_RUNTIME_OUTPUT_DIRECTORY}../layersvt/
             VERBATIM
             )
     endif()
 else()
+    FILE(TO_NATIVE_PATH \${CMAKE_CURRENT_SOURCE_DIR}/windows/VkLayer_vktrace_layer.json src_json)
     if (NOT (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR))
-        FILE(TO_NATIVE_PATH \${CMAKE_CURRENT_SOURCE_DIR}/windows/VkLayer_vktrace_layer.json src_json)
         if (CMAKE_GENERATOR MATCHES "^Visual Studio.*")
             FILE(TO_NATIVE_PATH \${PROJECT_BINARY_DIR}/../../../layersvt/\$<CONFIGURATION>/VkLayer_vktrace_layer.json dst_json)
         else()
@@ -134,4 +135,9 @@ target_link_libraries(\${PROJECT_NAME}
 
 build_options_finalize()
 
-set_target_properties(VkLayer_vktrace_layer PROPERTIES LINKER_LANGUAGE C)
+set_target_properties(VkLayer_vktrace_layer
+    PROPERTIES LINKER_LANGUAGE C
+    INSTALL_RPATH \${CMAKE_INSTALL_PREFIX}/etc/vulkan/explicit_layer.d )
+
+install (FILES \${src_json} DESTINATION etc/vulkan/explicit_layer.d)
+install(TARGETS VkLayer_vktrace_layer DESTINATION etc/vulkan/explicit_layer.d)
diff --git a/layers/CMakeLists.txt b/layers/CMakeLists.txt
index 0ccfc87..04f0cb7 100644
--- a/layers/CMakeLists.txt
+++ b/layers/CMakeLists.txt
@@ -71,7 +71,7 @@ if(UNIX)
             VERBATIM
             DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/linux/${config_file}.json
             )
-        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/staging-json/${config_file}.json DESTINATION /etc/vulkan/explicit_layer.d)
+        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/staging-json/${config_file}.json DESTINATION etc/vulkan/explicit_layer.d)
     endforeach(config_file)
 endif()
 
EOF







  #
  # install binaries
  #
  patch -p1 <<EOF
diff --git a/vktrace/src/vktrace_replay/CMakeLists.txt b/vktrace/src/vktrace_replay/CMakeLists.txt
index 5bc1807..fff729a 100644
--- a/vktrace/src/vktrace_replay/CMakeLists.txt
+++ b/vktrace/src/vktrace_replay/CMakeLists.txt
@@ -28,4 +28,10 @@ target_link_libraries(\${PROJECT_NAME}
     \${LIBRARIES}
 )
 
+if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
+  set_target_properties(\${PROJECT_NAME} PROPERTIES
+      INSTALL_RPATH "\\\$ORIGIN/../VulkanSamples/build/loader")
+  install(TARGETS \${PROJECT_NAME} DESTINATION bin)
+endif()
+
 build_options_finalize()
diff --git a/vktrace/src/vktrace_trace/CMakeLists.txt b/vktrace/src/vktrace_trace/CMakeLists.txt
index 63ba576..6cb921e 100644
--- a/vktrace/src/vktrace_trace/CMakeLists.txt
+++ b/vktrace/src/vktrace_trace/CMakeLists.txt
@@ -24,4 +24,8 @@ target_link_libraries(\${PROJECT_NAME}
     vktrace_common
 )
 
+if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
+  install(TARGETS \${PROJECT_NAME} DESTINATION bin)
+endif()
+
 build_options_finalize()
diff --git a/vktrace/src/vktrace_viewer/CMakeLists.txt b/vktrace/src/vktrace_viewer/CMakeLists.txt
index 5dfa5f8..58d2392 100644
--- a/vktrace/src/vktrace_viewer/CMakeLists.txt
+++ b/vktrace/src/vktrace_viewer/CMakeLists.txt
@@ -162,6 +162,10 @@ if (MSVC)
                     COMMENT "Copying vktraceviewer Qt5 dependencies to \${COPY_DEST}"
                     VERBATIM)
 
+else()
+  set_target_properties(\${PROJECT_NAME} PROPERTIES
+      INSTALL_RPATH "\\\$ORIGIN/../VulkanSamples/build/loader")
+  install(TARGETS \${PROJECT_NAME} DESTINATION bin)
 endif()
 
 build_options_finalize()
EOF


  cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_INSTALL_PREFIX=${PREFIX}" -DBUILD_WSI_XLIB_SUPPORT=Off -DBUILD_VKTRACEVIEWER=On
  cd build
  make -j $(nproc) install
)
