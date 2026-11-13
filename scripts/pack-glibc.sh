#!/bin/sh
# pack-glibc.sh <glibc-x11.so> <glibc-x11-wayland.so> <android.so> <version> [outdir]
#
# Packs the glibc builds of the driver (scripts/setup-glibc27.sh, Debian 10 sysroot, needs
# glibc 2.28 or newer) for apps that load Vulkan drivers inside a glibc Linux:
#
#   soulexit-panvk-<ver>-glibc.wcp      Winlator glibc forks (longjunyu2, moze30, afeimod...):
#                                       Contents -> install. A "Turnip" content pack; the
#                                       library takes Turnip's file names so the app trusts it,
#                                       and the ICD points to it by a relative path, so any
#                                       package name works. X11 only.
#   soulexit-panvk-<ver>-droiddeck.zip  DroidDeck: GPU drivers -> Import .zip. A driver pair:
#                                       android/ = the normal (bionic) driver for the display,
#                                       linux/ = the glibc driver (X11 + Wayland) for Steam/games.
set -eu
X11="$1"; WL="$2"; AND="$3"; VER="$4"; OUT="${5:-.}"
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
MESA_VER="$(strings "$X11" | sed -n 's/.*(Mesa \([0-9][0-9.]*-[a-z]*\) (git-.*/\1/p' | head -1)"
API_VER="$(strings "$X11" | grep -m1 -o '^1\.4\.[0-9]*$' || echo 1.4.303)"

# --- Winlator glibc .wcp (xz tar) ---
T="$(mktemp -d)"
mkdir -p "$T/panvk"
cp "$X11" "$T/panvk/libvulkan_freedreno.so"
cat > "$T/panvk/freedreno_icd.aarch64.json" <<JSON
{
    "ICD": {
        "api_version": "$API_VER",
        "library_path": "../../../lib/libvulkan_freedreno.so"
    },
    "file_format_version": "1.0.0"
}
JSON
cat > "$T/profile.json" <<JSON
{
  "type": "Turnip",
  "versionName": "soulexit-panvk-$VER",
  "versionCode": 1,
  "description": "soulexit's PanVK $VER for Mali Bifrost (G52, G72, G76), glibc build. Mesa ${MESA_VER:-unknown}. Installed as Turnip: pick it as the container's Graphics Driver. https://github.com/soulexit/panvk-bifrost-soulexit",
  "files": [
    {
      "source": "panvk/libvulkan_freedreno.so",
      "target": "\${libdir}/libvulkan_freedreno.so"
    },
    {
      "source": "panvk/freedreno_icd.aarch64.json",
      "target": "\${sharedir}/vulkan/icd.d/freedreno_icd.aarch64.json"
    }
  ]
}
JSON
WCP="$OUT/soulexit-panvk-$VER-glibc.wcp"
(cd "$T" && tar -c --owner=0 --group=0 profile.json panvk | xz -9 -T0 > "$WCP")
rm -r "$T"

# --- DroidDeck pair zip ---
T="$(mktemp -d)"
mkdir -p "$T/android" "$T/linux"
cp "$AND" "$T/android/libvulkan_panfrost.so"
cat > "$T/android/meta.json" <<JSON
{
  "schemaVersion": 1,
  "name": "soulexit-panvk-$VER",
  "description": "PanVK Vulkan driver for Mali Bifrost GPUs (G52, G72, G76) on the stock kernel driver. Testing build.",
  "author": "soulexit",
  "packageVersion": "$VER",
  "vendor": "Mesa",
  "driverVersion": "soulexit PanVK $VER (Mesa ${MESA_VER:-unknown})",
  "minApi": 29,
  "libraryName": "libvulkan_panfrost.so"
}
JSON
cp "$WL" "$T/linux/libvulkan_panfrost.so"
cat > "$T/linux/meta.json" <<JSON
{
  "kind": "linux-vulkan-icd",
  "name": "soulexit PanVK $VER (Linux, glibc)",
  "driver": "panvk",
  "driverVersion": "soulexit PanVK $VER (Mesa ${MESA_VER:-unknown})",
  "libraryName": "libvulkan_panfrost.so",
  "libc": "glibc",
  "minGlibc": "2.28",
  "source": "https://github.com/soulexit/panvk-kirin990"
}
JSON
cat > "$T/manifest.json" <<JSON
{
  "package": "soulexit PanVK (Mali Bifrost)",
  "version": "$VER",
  "tag": "soulexit-panvk-$VER",
  "drivers": {
    "android": { "path": "android" },
    "linux": { "path": "linux" }
  }
}
JSON
ZIP="$OUT/soulexit-panvk-$VER-droiddeck.zip"
rm -f "$ZIP"
(cd "$T" && zip -q -X -r "$ZIP" manifest.json android linux)
rm -r "$T"
sha256sum "$WCP" "$ZIP"
