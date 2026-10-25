#!/bin/sh
# pack.sh <libvulkan_panfrost.so> <version> [outdir] — Winlator / AdrenoTools driver zip
set -eu
SO="$1"; VER="$2"; OUT="${3:-.}"
T="$(mktemp -d)"
cp "$SO" "$T/libvulkan_panfrost.so"
cat > "$T/meta.json" <<JSON
{
  "schemaVersion": 1,
  "name": "PanVK G76 Kirin 990 $VER",
  "description": "Mesa PanVK over the stock mali_kbase/JM driver for Mali-G76 MC16 (Huawei P40 / Kirin 990). Bifrost geometry/tessellation/transform feedback/robustness2 emulation, large compute workgroups. Testing build.",
  "author": "soulexit",
  "packageVersion": "$VER",
  "vendor": "Mesa",
  "driverVersion": "Mesa 26.3.0-devel / PanVK-kbase G76-k990 $VER",
  "minApi": 29,
  "libraryName": "libvulkan_panfrost.so"
}
JSON
mkdir -p "$OUT"
ZIP="$(cd "$OUT" && pwd)/panvk-g76-kirin990-$VER.zip"
rm -f "$ZIP"
(cd "$T" && zip -q -r "$ZIP" .)
rm -rf "$T"
sha256sum "$ZIP"
