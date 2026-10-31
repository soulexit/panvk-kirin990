# ARMSX2 (PS2 emulator)

Tested with ARMSX2 nightly (`com.armsx2.nightly`) on the Huawei P40, from 1.2.5 on.

## Install

ARMSX2's driver import only accepts a library named `libvulkan_freedreno.so`, so it has its
own zip: install `panvk-g76-kirin990-<version>-armsx2.zip` (do not unzip it) from ARMSX2's
GPU driver setting, and select it. The driver is the same as in the main zip.

Driver options go in `/sdcard/Android/data/com.armsx2.nightly/files/panvk_env.txt` (same
format, see the README's *Diagnostics*). Leave it absent for normal use.

## Settings

- Renderer: Vulkan.
- Huawei's **performance mode** may raise the GPU clock; the governor otherwise keeps it at
  166–250 MHz for ARMSX2.

## Results

| Test (P40) | 1.2.4 | 1.2.5 | Huawei's driver |
|---|---|---|---|
| Kingdom Hearts (in game) | 5–8 FPS | **27–30 FPS**, no visual issues seen | 35–41 FPS |

## Notes

- 1.2.4 was slow because PCSX2 reads the render target back before almost every draw; without
  rasterization-order attachment access each read-back ended the render pass. 1.2.5 exposes
  `VK_EXT_rasterization_order_attachment_access` on the G76, so the passes stay whole.
- ARMSX2 waits for the GPU twice per frame (PCSX2 keeps three command buffers in flight); the
  remaining gap is GPU time per pass. See [FINDINGS §11](../FINDINGS.md).
- Uniform-buffer constants are pushed into registers for this app by default
  (`PANVK_V7_PUSH_UBO=0` turns it off).
