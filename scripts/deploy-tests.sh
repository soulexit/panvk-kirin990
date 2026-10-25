#!/bin/sh
# deploy-tests.sh <libvulkan_panfrost.so> [adb serial] — push the driver and the tests to
# /data/local/tmp (no root needed: /dev/mali0 is world-accessible on these phones).
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
A="adb${2:+ -s $2}"
$A shell mkdir -p /data/local/tmp/drv /data/local/tmp/tests /data/local/tmp/probes
$A push "$1" /data/local/tmp/drv/libvulkan_panfrost.so >/dev/null
$A push "$ROOT/out/tests/." /data/local/tmp/tests/ >/dev/null
$A push "$ROOT/out/probes/." /data/local/tmp/probes/ >/dev/null
$A push "$ROOT/scripts/run-tests.sh" /data/local/tmp/ >/dev/null
$A shell 'chmod 755 /data/local/tmp/tests/* /data/local/tmp/probes/* /data/local/tmp/run-tests.sh'
echo "run: $A shell sh /data/local/tmp/run-tests.sh /data/local/tmp/drv"
