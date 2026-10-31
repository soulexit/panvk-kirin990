#!/bin/sh
# build.sh [outdir] — libGL_memfd.so for Winlator's OpenGL (Zink) on Android 10.
# Its dependency is Winlator's libGL.so.1 by absolute path, so it is linked against a
# stub with that soname. Needs ANDROID_NDK_ROOT.
set -eu
DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:-$DIR/../../out}"; mkdir -p "$OUT"
CC="${ANDROID_NDK_ROOT:?}/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android29-clang"
REAL=/data/data/com.winlator.cmod/files/imagefs/usr/lib/libGL.so.1
T="$(mktemp -d)"
echo 'void libgl_stub(void){}' > "$T/stub.c"
"$CC" -shared -fPIC -Wl,-soname,"$REAL" "$T/stub.c" -o "$T/libGLstub.so"
"$CC" -O2 -shared -fPIC -Wl,-soname,libGL_memfd.so -Wl,--no-as-needed \
      "$DIR/libGL_memfd.c" "$T/libGLstub.so" -o "$OUT/libGL_memfd.so"
rm -rf "$T"
echo "$OUT/libGL_memfd.so"
