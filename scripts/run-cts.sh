#!/bin/sh
# run-cts.sh <driver.so> <caselist> <outdir> [run.py options] — run dEQP-VK cases on the
# phone over adb against a driver .so, without root and without the Android loader.
# The CTS from build-cts.sh must already be in /data/local/tmp/cts. Results go to
# <outdir>/results.csv; tests/cts/summary.py <outdir>/results.csv groups the failures.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SO="$1"; LIST="$2"; OUTDIR="$3"; shift 3
NAME="$(basename "$SO" .so)-$(date +%s).so"
adb push "$SO" "/data/local/tmp/cts/$NAME" >/dev/null
# One worker: two dEQP processes sharing the JM GPU can trip kbase's hard-stop timer.
python3 "$ROOT/tests/cts/run.py" "$LIST" "$OUTDIR" -j 1 --icd "$NAME" "$@"
python3 "$ROOT/tests/cts/summary.py" "$OUTDIR/results.csv" 3 | head -40
