#!/bin/sh
# build-android.sh [workdir] [version] — cross-build libvulkan_panfrost.so (Android
# aarch64, API 29) from the tree made by prepare-tree.sh, stripped, then pack it.
# Needs: Android NDK (ANDROID_NDK_ROOT), meson, ninja, python3-mako/yaml, and for the
# host code generators LLVM + libclc + SPIRV-LLVM-Translator (see zenithblue's
# scripts/bootstrap-host-tools.sh).
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${1:-$ROOT/work}"
VERSION="${2:-1.0.0-winlator}"
NDK="${ANDROID_NDK_ROOT:?set ANDROID_NDK_ROOT}"
NDK_BIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin"
ZB="$WORK/zenithblue"
HT="$WORK/build/host-tools"
[ -x "$HT/bin/mesa_clc" ] || sh "$ZB/scripts/bootstrap-host-tools.sh" --mesa "$WORK/mesa" --out "$HT"
DEPS="$ZB/work/android-deps"
[ -f "$DEPS/lib/pkgconfig/libdrm.pc" ] || ANDROID_NDK_ROOT="$NDK" sh "$ZB/scripts/build-android-deps.sh" --api 29
B="$WORK/build/android"
mkdir -p "$WORK/build"
cat > "$WORK/build/android.cross.ini" <<INI
[binaries]
c = 'aarch64-linux-android29-clang'
cpp = 'aarch64-linux-android29-clang++'
ar = 'llvm-ar'
strip = 'llvm-strip'
pkg-config = 'pkg-config'

[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'

[properties]
pkg_config_libdir = ['$DEPS/lib/pkgconfig']

[built-in options]
c_args = ['-DPANVK_KBASE_RELEASE="G76-k990 $VERSION"']
cpp_args = ['-DPANVK_KBASE_RELEASE="G76-k990 $VERSION"']
INI
export PATH="$HT/bin:$NDK_BIN:$PATH"
rm -rf "$B"
# Android WSI only: an X11 WSI makes Winlator's wrapper present through Mesa's
# X11 swapchain, which fails vkAcquireNextImageKHR in Winlator.
meson setup "$B" "$WORK/mesa" --cross-file "$WORK/build/android.cross.ini" \
  -Dbuildtype=release -Db_ndebug=true -Dplatforms=android -Dandroid-stub=true \
  -Dandroid-strict=false -Dgallium-drivers= -Dvulkan-drivers=panfrost \
  -Dpanfrost-kmds=kbase -Dmesa-clc=system -Dprecomp-compiler=system \
  -Dxlib-lease=disabled -Degl=disabled -Dgles1=disabled -Dgles2=disabled \
  -Dopengl=false -Dglx=disabled -Dgbm=disabled -Dlibunwind=disabled \
  -Dzstd=disabled -Dcpp_link_args=-static-libstdc++
ninja -C "$B"
mkdir -p "$WORK/dist"
"$NDK_BIN/llvm-strip" -o "$WORK/dist/libvulkan_panfrost.so" "$B/src/panfrost/vulkan/libvulkan_panfrost.so"
sh "$ROOT/scripts/pack.sh" "$WORK/dist/libvulkan_panfrost.so" "$VERSION" "$WORK/dist"
