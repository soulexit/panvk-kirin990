# Huawei HarmonyOS 4.2 / Kirin 990: Mali-G76 compatibility

This is an **opt-in, device-scoped** compatibility profile for the stock Huawei
`mali_kbase` job-manager driver. It does not change default PanVK release
builds or the existing `MEM_IMPORT` compatibility patch.

## Symptoms and diagnosis

On a Huawei P40 Pro (Kirin 990, Mali-G76 MC16, HarmonyOS 4.2 / Android 12;
observed GPU ID `0x72110000`, kbase JM uAPI 11.38), the existing
72-byte dependency-only atom probe completed successfully, but PanVK
selected 64-byte atoms for real job submissions. Those submissions returned
`EINVAL` repeatedly, leaving the game running with **audio and controls
but black output**.

The job-manager workaround reuses PanVK's existing 72-byte atom wrapper
(`seq_nr` plus the base atom) and selects it only when the existing probe
selected the incompatible 64-byte layout for the observed G76 ID.

After rendering was restored, rotating the device could crash the emulator.
Independent on-device A/B tests found either hiding
`VK_KHR_swapchain_maintenance1` or disabling swapchain pre-rotation
prevented that crash. The second workaround hides that extension for Huawei
Mali-G76 at device initialization, in **all Vulkan apps**, not only one
emulator. The underlying rotation/WSI teardown issue is not yet fully
root-caused.

## Build the profile

The two patches apply **after** the repository's ordinary pinned Mesa +
PanVK patch series; they fail if the expected source anchors are missing:

```sh
PANVK_H42_G76_COMPAT=1 sh scripts/prepare-tree.sh "$PWD/work"
ANDROID_NDK_ROOT=/path/to/android-ndk sh scripts/build-android.sh \
  "$PWD/work" 1.3.0-h42-g76
```

The normal `prepare-tree.sh` without `PANVK_H42_G76_COMPAT=1` remains
unchanged. Use `scripts/pack.sh` with the `armsx2` final argument to
produce the alternate ARMSX2 package; see the existing build docs.

The existing upstream `patches/0001-*` handles the legacy 32-byte
`MEM_IMPORT` ABI; this contribution does not duplicate or replace it.

### Runtime escape hatches

- `PANVK_H42_ATOM72=0` — use the previous automatic job-atom selection.
- `PANVK_H42_ROTATION_COMPAT=0` — restore the original
  `VK_KHR_swapchain_maintenance1` extension advertisement.
- Existing `PANVK_HIDE_EXTS` behavior remains available.

Both code paths are guarded: the atom workaround requires non-CSF, the
specific observed G76 GPU ID and the 64-byte probe result; the rotation
workaround requires Android, Huawei manufacturer and the matching G76 ID.
Do not assume this behavior is correct on every Mali-G76 device.

## Validation

- **2026-11-11:** the initial 72-byte build restored rendering, based on
  user testing on the affected Huawei device.
- **2026-11-11:** user subsequently confirmed the combined **Atom72 +
  universal rotation** driver works in **BannerHub 6 and ARMSX2**.
- Successful experimental Android ARM64 build, including standard and
  ARMSX2 ZIP packaging: [GitHub Actions #43064282195](https://github.com/dissidius/panvk-kirin990-harmonyos4.2-android12/actions/runs/43064282195).
- Previous Atom72-only build:
  [GitHub Actions #43014633315](https://github.com/dissidius/panvk-kirin990-harmonyos4.2-android12/actions/runs/43014633315).

The experimental build additionally used submission diagnostics; those
diagnostic log statements are not necessary to the compatibility logic
and are intentionally omitted from this upstream-facing change.

This is targeted device validation, **not Vulkan CTS conformance** and not
proof that every emulator, other Android version, or other Huawei GPU
works. These workarounds can be removed if the underlying kernel/WSI
compatibility problems are addressed more generally.
