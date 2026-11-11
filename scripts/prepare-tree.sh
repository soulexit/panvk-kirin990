#!/bin/sh
# prepare-tree.sh [workdir] — reproduce the driver source tree (1.3.0 and newer):
#   Mesa a379b735 (main, 27.0.0-devel)
#   + patches/base/: the zenithblue-oss/panvk-kbase-android series (RC5-universal-112:
#     csf-v11 <= 229, jm-v7-wip <= 0119) merged onto that Mesa, as one diff
#   + the Kirin 990 / Winlator patches in patches/00*.patch.
# Releases up to 1.2.9 used Mesa 5a07217f and patches/mesa-26.3/ (see git history).
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${1:-$ROOT/work}"
MESA_COMMIT=a379b735aa859a628b6a88783d4eec07b22e061a
ZB_COMMIT=917b9b3b5ff62d7f17303c13fa9cc924c0f308a3
mkdir -p "$WORK"
# zenithblue's repository is still needed for its host-tools bootstrap script.
if [ ! -d "$WORK/zenithblue/.git" ]; then
  git clone -q https://github.com/zenithblue-oss/panvk-kbase-android "$WORK/zenithblue"
fi
git -C "$WORK/zenithblue" checkout -q "$ZB_COMMIT"
if [ ! -d "$WORK/mesa/.git" ]; then
  git init -q "$WORK/mesa"
  git -C "$WORK/mesa" remote add origin https://gitlab.freedesktop.org/mesa/mesa.git
fi
git -C "$WORK/mesa" fetch -q --depth 1 origin "$MESA_COMMIT"
git -C "$WORK/mesa" checkout -q --detach FETCH_HEAD
export GIT_AUTHOR_NAME="${GIT_AUTHOR_NAME:-build}" GIT_AUTHOR_EMAIL="${GIT_AUTHOR_EMAIL:-build@localhost}"
export GIT_COMMITTER_NAME="$GIT_AUTHOR_NAME" GIT_COMMITTER_EMAIL="$GIT_AUTHOR_EMAIL"
git -C "$WORK/mesa" apply --index --whitespace=nowarn \
   "$ROOT"/patches/base/zenithblue-jm-v7-series-on-mesa-a379b735.patch
git -C "$WORK/mesa" commit -q -m "zenithblue jm-v7 series on Mesa a379b735"
git -C "$WORK/mesa" am -q "$ROOT"/patches/00*.patch

# Opt-in compatibility for Huawei Mali-G76 (Kirin 990) on HarmonyOS 4.2.
# Default builds, other GPUs and the ordinary 1.3.0 release remain unchanged.
# Run: PANVK_H42_G76_COMPAT=1 sh scripts/prepare-tree.sh work
if [ "${PANVK_H42_G76_COMPAT:-0}" = "1" ]; then
  python3 "$ROOT/harmony42/apply_atom72.py" \
    "$WORK/mesa/src/panfrost/lib/kmod/kbase_kmod.c"
  python3 "$ROOT/harmony42/apply_rotation_compat.py" \
    "$WORK/mesa/src/panfrost/vulkan/panvk_physical_device.c"
  echo "H42: applied Huawei G76 Atom72 and swapchain rotation compatibility"
fi

echo "OK tree=$WORK/mesa head=$(git -C "$WORK/mesa" rev-parse --short HEAD) tree-id=$(git -C "$WORK/mesa" rev-parse --short HEAD^{tree})"
