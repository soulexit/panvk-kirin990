#!/bin/sh
# build-cts.sh [workdir] — build the Khronos Vulkan CTS (dEQP-VK) as a standalone Android
# arm64 executable (no APK, no root), plus the loader-less shim and the probes in tests/cts.
# Needs: Android NDK (ANDROID_NDK_ROOT), cmake, ninja, python3, git.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${1:-$ROOT/work}"
NDK="${ANDROID_NDK_ROOT:?set ANDROID_NDK_ROOT}"
NDK_BIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"
CC="$NDK_BIN/aarch64-linux-android29-clang"
CTS="$WORK/VK-GL-CTS"
B="$WORK/build/cts-android"
OUT="$WORK/cts-dist"

if [ ! -d "$CTS" ]; then
   git clone -q --depth 1 -b vulkan-cts-1.4.7.0 https://github.com/KhronosGroup/VK-GL-CTS.git "$CTS"
   (cd "$CTS" && git apply "$ROOT/tests/cts/vk-gl-cts-1.4.7.0-android-exe.patch" &&
    python3 external/fetch_sources.py --protocol https)
fi

# DE_OS/DE_CPU are not derived from the NDK toolchain file; DEQP_ANDROID_EXE builds
# deqp-vk as a plain executable with the headless platform from the patch.
cmake -S "$CTS" -B "$B" -G Ninja \
   -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
   -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 \
   -DDE_OS=DE_OS_ANDROID -DDE_CPU=DE_CPU_ARM_64 -DDE_ANDROID_API=29 \
   -DDEQP_TARGET=android -DDEQP_ANDROID_EXE=ON -DCMAKE_BUILD_TYPE=Release \
   -DSELECTED_BUILD_TARGETS=deqp-vk
ninja -C "$B" deqp-vk

mkdir -p "$OUT"
"$NDK_BIN/llvm-strip" -o "$OUT/deqp-vk" "$B/external/vulkancts/modules/vulkan/deqp-vk"
"$CC" -O2 -shared -fPIC \
   -o "$OUT/libvkshim.so" "$ROOT/tests/cts/vkshim.c" -ldl
"$CC" -O1 -shared -fPIC -o "$OUT/libsegv.so" "$ROOT/tests/cts/segv.c" -ldl
for p in xferprobe drmprobe; do
   "$CC" -O1 -w \
      -o "$OUT/$p" "$ROOT/tests/cts/$p.c" -ldl
done
cp -r "$CTS/external/vulkancts/data/vulkan" "$OUT/"
cp -r "$CTS/external/vulkancts/mustpass/main/vk-default" "$OUT/mustpass"
echo "CTS ready in $OUT (push it with: adb push $OUT/. /data/local/tmp/cts/)"
