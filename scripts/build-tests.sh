#!/bin/sh
# build-tests.sh [workdir] — build the on-device test binaries (Android aarch64, API 29).
#   out/tests/   38 PanProbe Vulkan tests from zenithblue-oss/panvk-kbase-android
#                (geometry, tessellation, xfb, robustness2, depth bounds, DXVK/VKD3D
#                requirement probes, ...) plus this repo's stress/lwg/ahb/compositor tests
#   out/probes/  small capability and behaviour probes from tests/probes
# Needs ANDROID_NDK_ROOT, glslangValidator, xxd, and prepare-tree.sh's workdir.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${1:-$ROOT/work}"
ZB="$WORK/zenithblue"
CC="${ANDROID_NDK_ROOT:?}/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang"
INC="-I$WORK/mesa/include -I$WORK/mesa/include/android_stub"
OUT="$ROOT/out"; mkdir -p "$OUT/tests" "$OUT/probes" "$OUT/gen"

# zenithblue PanProbe tests (MIT), built as standalone executables.
C="$ZB/apps/panvk-test/app/src/main/cpp"
grep -E "^add_vk_test" "$C/CMakeLists.txt" | sed -E 's/add_vk_test\(([^ ]+) ([^)]+)\)/\1 \2/' |
while read n p; do
  extra=""; [ "$n" = bc_decode ] && extra="-I$C/compat"
  "$CC" -include "$C/pt_compat.h" -w -O2 $INC -I"$(dirname "$ZB/$p")" $extra "$ZB/$p" \
        -o "$OUT/tests/$n" -ldl -lm -llog
done
for t in ahb ahb_linear ahb_render ahb_vendor ahb_vendor_linear; do
  "$CC" -O2 -w $INC "$ZB/tests/ahb/$t.c" -o "$OUT/tests/ahb_$t" -ldl -landroid -lnativewindow -llog
done

# Shaders for this repo's tests.
spv() { glslangValidator -V "$1" -o "$OUT/gen/$2.spv" >/dev/null; (cd "$OUT/gen" && xxd -i "$2.spv" > "$2_spv.h"); }
spv "$ROOT/tests/stress/s.vert" s.vert; spv "$ROOT/tests/stress/s.frag" s.frag
cat "$OUT/gen/s.vert_spv.h" "$OUT/gen/s.frag_spv.h" > "$OUT/gen/spv.h"
spv "$ROOT/tests/lwg/r.comp" r; spv "$ROOT/tests/compositor/c.comp" c
spv "$ROOT/tests/stress/vpull.vert" vpull.vert

"$CC" -O2 -w $INC -I"$OUT/gen" -I"$ROOT/tests/stress" "$ROOT/tests/stress/stress.c" -o "$OUT/tests/stress" -ldl -lm
"$CC" -O2 -w $INC -I"$OUT/gen" -I"$ROOT/tests/stress" "$ROOT/tests/stress/idraw.c" -o "$OUT/tests/idraw" -ldl -lm
"$CC" -O2 -w $INC -I"$OUT/gen" -I"$ROOT/tests/stress" "$ROOT/tests/stress/vpull.c" -o "$OUT/tests/vpull" -ldl -lm
"$CC" -O2 -w $INC -I"$OUT/gen" -I"$ROOT/tests/stress" "$ROOT/tests/stress/memspeed.c" -o "$OUT/tests/memspeed" -ldl -lm
"$CC" -O1 -w $INC -I"$OUT/gen" "$ROOT/tests/lwg/lwg.c" -o "$OUT/tests/lwg" -ldl
"$CC" -O1 -w $INC -I"$OUT/gen" -DUSAGE_DST=0x333 "$ROOT/tests/compositor/comp.c" -o "$OUT/tests/compositor" -ldl -landroid -lnativewindow
A="$ZB/tests/ahb"
"$CC" -O1 -w $INC -I"$A" "$ROOT/tests/ahb/ahb2.c" -o "$OUT/tests/ahb2" -ldl -landroid -lnativewindow
"$CC" -O1 -w $INC -I"$A" "$ROOT/tests/ahb/ahb2d.c" -o "$OUT/tests/ahb2d" -ldl -landroid -lnativewindow
"$CC" -O1 -w $INC -I"$A" -DW=1080 -DH=2340 -DAFMT=1 -DVFMT=VK_FORMAT_R8G8B8A8_UNORM -DANB_USAGE=0x333 \
      "$ROOT/tests/ahb/anb.c" -o "$OUT/tests/anb_fullscreen" -ldl -landroid -lnativewindow

for f in "$ROOT"/tests/probes/*.c; do
  n="$(basename "$f" .c)"
  "$CC" -O1 -w $INC "$f" -o "$OUT/probes/$n" -ldl -landroid -lnativewindow
done
echo "OK out=$OUT"
