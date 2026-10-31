#!/bin/sh
# pack.sh <libvulkan_panfrost.so> <version> [outdir] [armsx2] — AdrenoTools driver zip.
# With "armsx2" the library is named libvulkan_freedreno.so, as ARMSX2's importer requires
# (zip panvk-g76-kirin990-<version>-armsx2.zip). A libGL_memfd.so next to the driver
# (tools/winlator-gl/build.sh) goes into the main zip. The name has no spaces: apps unpack
# the zip into a directory of that name, and BOX64_LIBGL takes its path.
set -eu
SO="$1"; VER="$2"; OUT="${3:-.}"; LAYOUT="${4:-}"
LIB=libvulkan_panfrost.so; SUFFIX=""
if [ "$LAYOUT" = armsx2 ]; then LIB=libvulkan_freedreno.so; SUFFIX=-armsx2; fi
T="$(mktemp -d)"
cp "$SO" "$T/$LIB"
GL="$(dirname "$SO")/libGL_memfd.so"
if [ "$LAYOUT" != armsx2 ] && [ -f "$GL" ]; then cp "$GL" "$T/"; fi
cat > "$T/meta.json" <<JSON
{
  "schemaVersion": 1,
  "name": "PanVK-G76-Kirin990-$VER",
  "description": "Mesa PanVK over the stock mali_kbase/JM driver for Mali-G76 MC16 (Huawei P40 / Kirin 990). Bifrost geometry/tessellation/transform feedback/robustness2 emulation, large compute workgroups. Testing build.",
  "author": "soulexit",
  "packageVersion": "$VER",
  "vendor": "Mesa",
  "driverVersion": "Mesa 26.3.0-devel / PanVK-kbase G76-k990 $VER",
  "minApi": 29,
  "libraryName": "$LIB"
}
JSON
mkdir -p "$OUT"
ZIP="$(cd "$OUT" && pwd)/panvk-g76-kirin990-$VER$SUFFIX.zip"
rm -f "$ZIP"
(cd "$T" && zip -q -r "$ZIP" .)
rm -rf "$T"
sha256sum "$ZIP"
