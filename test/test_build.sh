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
    if (NR % 10) next
    printf \"\\r\\x1b[K\" NR \"K/$maxlines $msg\" > \"/proc/self/fd/4\"
  }"
}

watch_progress "running build.sh" /tmp/build.sh.log 232400 &
watch_pid=$!

../build.sh > /tmp/build.sh.log 2>&1
R=$?

pkill -P $watch_pid
wait $watch_pid
echo ""

if [ $R -ne 0 ]; then
  echo "build.sh exit code: $R, log is /tmp/build.sh.log"
fi

watch_progress "running vulkantools.sh" /tmp/vulkantools.sh.log 232400 &
watch_pid=$!

../vendor/vulkantools.sh > /tmp/vulkantools.sh.log 2>&1
R=$?

pkill -P $watch_pid
wait $watch_pid
echo ""

if [ $R -ne 0]; then
  echo "vulkantools.sh exit code: $R, log is /tmp/vulkantools.sh.log"
fi
