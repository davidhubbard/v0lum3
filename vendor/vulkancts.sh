#!/bin/bash
#
# This script will build Vulkan-CTS

set -e

GIT_REPO_URL="https://github.com/KhronosGroup/VK-GL-CTS/"

cd $( dirname "$0" )

if [ -d cts ]; then
  echo "cts directory already exists. Skipping git clone."
else
  git clone "${GIT_REPO_URL}" cts
fi
PREFIX="${PWD}"
CTSCTS="${PREFIX}/cts/external/vulkancts"

(
  cd cts
  python external/fetch_sources.py
  CTSB="${CTSCTS}/build"
  mkdir -p "${CTSB}"
  python external/vulkancts/scripts/build_spirv_binaries.py -b "${CTSB}"
  cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Debug
  cd build
  make -j $(nproc)
)

echo ""
echo " Success."
echo ""
echo " Next steps:"
echo " 1. Choose a list of test cases."
echo "    WARNING: the defaults include *ALL* tests. Your machine will be"
echo "    extremely laggy for ~2 hours while they run."
echo ""
echo "    These are the provided defaults:"
find ${PREFIX}/cts/external -name '*default.txt' | \
  sort | \
  while read a; do
    echo "      $a"
  done

echo ""
echo ""
echo " 2. Run the CTS:"
echo ""
echo "    CTSCTS=\"${CTSCTS}\""
echo "    TESTLIST=\"\${CTSCTS}/mustpass/1.0.3/vk-default.txt\" # or whatever"
echo "    cd \"\${CTSCTS}/modules/vulkan\""
echo "    # specify where vulkan was built (deqp-vk will load libvulkan.so)"
echo "    LD_LIBRARY_PATH=\"${PREFIX}/lib\" \\"
echo "    ./deqp-vk --deqp-caselist-file=\"\${TESTLIST}\" --deqp-log-images=disable --deqp-log-shader-sources=disable --deqp-log-flush=disable"
# --deqp-log-images=disable --deqp-log-shader-sources=disable --deqp-log-flush=disable
# are required by https://github.com/KhronosGroup/VK-GL-CTS/blob/master/external/vulkancts/README.md
echo ""
echo " 3. View the results:"
echo ""
echo "    less \"\${CTSCTS}/modules/vulkan/TestResults.qpa"
echo ""
echo "    (search using the \"/\" key and this regexp:)"
echo "      StatusCode=\"[^P][^o]"
echo ""
echo " More information available at: ${GIT_REPO_URL}"
