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
# script only installs the unique parts into this directory (which, after you
# exporting PKG_CONFIG_PATH should "just work". Even vkreplay and vktraceviewer
# run without hacking your LD_LIBRARY_PATH.)

set -e

cd $( dirname "$0" )

if ! pkg-config --cflags --libs xcb-keysyms > /dev/null 2>&1; then
  echo "Please install xcb-keysyms:"
  echo "  apt-get install libxcb-keysyms1-dev"
  echo "  rpm -qi xcb=util-keysyms-devel"
  echo "  pacman -S xcb-util-keysyms"
  echo "  emerge x11-libs/xcb-util-keysyms"
  exit 1
fi

git clone https://github.com/LunarG/VulkanTools
PREFIX="${PWD}"

if [ -d jsoncpp ]; then
  echo "Directory jsoncpp already exists. Skipping..."
else
  JSONCPP_REVISION=$(cat VulkanTools/external_revisions/jsoncpp_revision)
  git clone https://github.com/open-source-parsers/jsoncpp.git
  (
    cd jsoncpp
    git checkout ${JSONCPP_REVISION}
    python amalgamate.py
  )
fi

(
  cd VulkanTools
  git checkout 39b5bd2

  #
  # install VkLayer files that are VulkanTools layers
  # (VulkanTools is built on Vulkan-LoaderAndValidationLayers and re-builds dups of those layers)
  #
  # use sed because the api_version line may change (has changed before)
  # and it broke the patch...so this is a bad candidate for a patchfile.
  sed -i -e 's/"library_path": "..\/vktrace\/libVkLayer_vktrace_layer.so"/"library_path": ".\/libVkLayer_vktrace_layer.so"/' \
      vktrace/vktrace_layer/linux/VkLayer_vktrace_layer.json
  # use patch for the rest
  patch -p1 <<EOF
diff --git a/layersvt/CMakeLists.txt b/layersvt/CMakeLists.txt
index e6e143d..c1311c0 100644
--- a/layersvt/CMakeLists.txt
+++ b/layersvt/CMakeLists.txt
@@ -79,6 +79,11 @@ else()
              add_dependencies(\${config_file}-json \${config_file})
         endforeach(config_file)
     endif()
+    foreach (config_file \${LAYER_JSON_FILES})
+        install (FILES \${CMAKE_CURRENT_SOURCE_DIR}/linux/\${config_file}.json
+            DESTINATION etc/vulkan/explicit_layer.d
+            )
+    endforeach(config_file)
 endif()
 
 if (WIN32)
@@ -98,8 +103,11 @@ else()
     add_library(VkLayer_\${target} SHARED \${ARGN})
     target_link_Libraries(VkLayer_\${target} VkLayer_utilsvt)
     add_dependencies(VkLayer_\${target} generate_helper_files generate_api_cpp generate_api_h VkLayer_utilsvt)
-    set_target_properties(VkLayer_\${target} PROPERTIES LINK_FLAGS "-Wl,-Bsymbolic")
+    set_target_properties(VkLayer_\${target}
+        PROPERTIES LINK_FLAGS "-Wl,-Bsymbolic"
+            INSTALL_RPATH \${CMAKE_INSTALL_SYSCONFDIR}/vulkan/explicit_layer.d)
     install(TARGETS VkLayer_\${target} DESTINATION \${PROJECT_BINARY_DIR}/install_staging)
+    install(TARGETS VkLayer_\${target} DESTINATION etc/vulkan/explicit_layer.d)
     endmacro()
 endif()
 
diff --git a/vktrace/vktrace_layer/CMakeLists.txt b/vktrace/vktrace_layer/CMakeLists.txt
index a99b536..6fae99c 100644
--- a/vktrace/vktrace_layer/CMakeLists.txt
+++ b/vktrace/vktrace_layer/CMakeLists.txt
@@ -79,15 +79,16 @@ include_directories(
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
             FILE(TO_NATIVE_PATH \${PROJECT_BINARY_DIR}/../../layersvt/\$<CONFIGURATION>/VkLayer_vktrace_layer.json dst_json)
         else()
@@ -139,4 +140,9 @@ endif()
 
 build_options_finalize()
 
-set_target_properties(VkLayer_vktrace_layer PROPERTIES LINKER_LANGUAGE C)
+set_target_properties(VkLayer_vktrace_layer
+    PROPERTIES LINKER_LANGUAGE C
+    INSTALL_RPATH \${CMAKE_INSTALL_SYSCONFDIR}/vulkan/explicit_layer.d )
+
+install (FILES \${src_json} DESTINATION etc/vulkan/explicit_layer.d)
+install(TARGETS VkLayer_vktrace_layer DESTINATION etc/vulkan/explicit_layer.d)
EOF






  #
  # install binaries
  #
  patch -p1 <<EOF
diff --git a/vktrace/vktrace_replay/CMakeLists.txt b/vktrace/vktrace_replay/CMakeLists.txt
index 5bc1807..fff729a 100644
--- a/vktrace/vktrace_replay/CMakeLists.txt
+++ b/vktrace/vktrace_replay/CMakeLists.txt
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
diff --git a/vktrace/vktrace_trace/CMakeLists.txt b/vktrace/vktrace_trace/CMakeLists.txt
index 63ba576..6cb921e 100644
--- a/vktrace/vktrace_trace/CMakeLists.txt
+++ b/vktrace/vktrace_trace/CMakeLists.txt
@@ -24,4 +24,8 @@ target_link_libraries(\${PROJECT_NAME}
     vktrace_common
 )
 
+if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
+  install(TARGETS \${PROJECT_NAME} DESTINATION bin)
+endif()
+
 build_options_finalize()
diff --git a/vktrace/vktrace_viewer/CMakeLists.txt b/vktrace/vktrace_viewer/CMakeLists.txt
index 5dfa5f8..58d2392 100644
--- a/vktrace/vktrace_viewer/CMakeLists.txt
+++ b/vktrace/vktrace_viewer/CMakeLists.txt
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


  cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_INSTALL_PREFIX=${PREFIX}" -DBUILD_VKTRACEVIEWER=On
  cd build
  make -j $(nproc) install
)
