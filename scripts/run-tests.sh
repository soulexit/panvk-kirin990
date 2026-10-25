#!/system/bin/sh
# run-tests.sh <driver-dir> [tests...] — on the phone: run each test against the driver,
# 90 s timeout, under a 1.5 GB address-space limit so a runaway test cannot exhaust
# the phone's memory.  Prints "== <test> rc=<exit code> fails=<FAIL lines>".
D=$1; shift
cd /data/local/tmp/tests
T="$*"; [ -z "$T" ] && T=$(ls)
for t in $T; do
  out=$(cd $D && (ulimit -v 1500000; LD_LIBRARY_PATH=$D timeout 90 /data/local/tmp/tests/$t $D/libvulkan_panfrost.so 2>&1))
  rc=$?
  fails=$(echo "$out" | grep -c -E "^FAIL|FAIL ")
  echo "== $t rc=$rc fails=$fails"
  echo "$out" | grep -E "FAIL|CRASH|fault|DEVICE_LOST" | head -4
done
