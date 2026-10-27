#!/bin/sh
# run.sh <shader.vert.spv> [extra env...]: compile a captured vertex shader on a host
# Mesa build (debug + ASan, -Dtools=drm-shim) pretending to be a G76, e.g. with
# BIFROST_MESA_DEBUG=shaders,shaderdb. On the phone: SPVCOMP_BENCH=1 spvcomp <icd> v.spv f.spv.
B=${HOST_BUILD:?set HOST_BUILD to a host Mesa build with -Dtools=drm-shim}
D=$(cd "$(dirname "$0")" && pwd)
V=$(readlink -f "$1"); shift
cd /tmp && env ASAN_OPTIONS=detect_leaks=0 \
  LD_PRELOAD="$(gcc -print-file-name=libasan.so) $B/src/panfrost/drm-shim/libpanfrost_noop_drm_shim.so" \
  PAN_GPU_ID=7211 PAN_I_WANT_A_BROKEN_VULKAN_DRIVER=1 PANVK_V7_IDVS=1 MESA_SHADER_CACHE_DISABLE=true "$@" \
  "$D/spvcomp" $B/src/panfrost/vulkan/libvulkan_panfrost.so "$V"
