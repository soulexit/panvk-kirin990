#!/bin/sh
# prepare-tree.sh [workdir] — reproduce the driver source tree:
#   Mesa 5a07217f + zenithblue-oss/panvk-kbase-android series (RC5-universal-112:
#   csf-v11 <= 229, jm-v7-wip <= 0092) + the Kirin 990 / Winlator patches in patches/.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${1:-$ROOT/work}"
MESA_COMMIT=5a07217f034b3e50d8c7c7794f97a2df1742613b
ZB_COMMIT=917b9b3b5ff62d7f17303c13fa9cc924c0f308a3
mkdir -p "$WORK"
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
sh "$WORK/zenithblue/scripts/prepare-rc5-tree.sh" --mesa "$WORK/mesa" \
   --csf-max 229 --wip-max 0092 --base-tag rc5-base
git -C "$WORK/mesa" am -q "$ROOT"/patches/00*.patch
echo "OK tree=$WORK/mesa head=$(git -C "$WORK/mesa" rev-parse --short HEAD)"
