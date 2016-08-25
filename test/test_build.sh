#!/bin/bash
#
# This script just runs build.sh and vulkantools.sh and checks the result

watch_progress()
{
  msg="$1"
  file="$2"
  maxlines="$3"

  exec 4>&2
  exec 2>&-
  tail --pid=$BASHPID -F "$file" | awk "{
    printf \"\\r\\x1b[K\" NR \"/$maxlines $msg\" > \"/proc/self/fd/4\"
  }"
}

watch_progress "building" /tmp/build.log $(( 900 + 4600 )) &
watch_pid=$!

echo "===== build.sh:" > /tmp/build.log
../build.sh >> /tmp/build.log 2>&1
R=$?

if [ $R -eq 0 ]; then
  echo "===== vulkantools.sh:" >> /tmp/build.log
  ../vendor/vulkantools.sh >> /tmp/build.log 2>&1
  Q=$?
fi

pkill -P $watch_pid
wait $watch_pid
echo -e "\r\e[Ksuccess."

if [ $R -ne 0 ]; then
  echo "build.sh exit code: $R, log is /tmp/build.sh.log"
  exit $R
fi
if [ ! -f ../vendor/bin/glslangValidator ]; then
  echo "build.sh: something fishy is going on, log is /tmp/build.sh.log"
  exit 1
fi

if [ $Q -ne 0 ]; then
  echo "vulkantools.sh exit code: $Q, log is /tmp/vulkantools.sh.log"
  exit $Q
fi
if [ ! -f ../vendor/bin/vktrace ]; then
  echo "vulkantools.sh: something fishy is going on, log is /tmp/vulkantools.sh.log"
fi
