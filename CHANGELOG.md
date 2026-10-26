# Changelog

## 1.1.0-winlator (testing) — 2026-10-26

Advertised extensions checked with the Khronos Vulkan CTS (dEQP-VK 1.4.7.0) on the P40;
see [docs/CTS.md](docs/CTS.md).

- HOST_COHERENT memory is coherent again when `memoryMapPlaced` is enabled
  (`VK_EXT_map_memory_placed`, used by Wine WoW64 for 32-bit games): dma-heap/ION
  buffers were always CPU-cached on a GPU that does not snoop the CPU caches.
  API smoke tests 2/8 → 8/8, api/info failures 610 → 18.
- Cached dma-buf memory: flush/invalidate through `DMA_BUF_IOCTL_SYNC`; zero-initialised
  memory (`VK_EXT_zero_initialize_device_memory`) correct on every memory type.
- Vertex-shader storage images / texel buffers no longer hang the GPU (wrong attribute
  buffer index offset; also in upstream Mesa).
- Null image descriptors (robustness2 `nullDescriptor`): size/level/sample queries
  return 0 and format-less loads read 0.
- DRM-modifier images (`VK_EXT_image_drm_format_modifier`): no crash when every listed
  modifier is AFBC.
- Global priority LOW accepted; `VK_KHR_maintenance7` update-after-bind totals fixed.
- `VK_EXT_multisampled_render_to_single_sampled` no longer advertised on JM (broken,
  unused by DXVK/VKD3D-Proton): 166 → 165 device extensions.
- CTS tooling: `scripts/build-cts.sh`, `scripts/run-cts.sh`, `tests/cts/`.

## 1.0.0-winlator (testing) — 2026-10-25

First Winlator build on the zenithblue panvk-kbase-android base.

- Bifrost geometry shaders, tessellation, transform feedback, robustness2/null
  descriptors, depth bounds, pipeline statistics (zenithblue series).
- Kirin 990 kernel: pre-11.20 `MEM_IMPORT`, Huawei gralloc handles, linear swapchain
  buffers; Samsung gralloc linear fallback.
- Winlator: Android-only WSI; SYNC_FD semaphores on JM (fixes acquire hang).
- G76: tiler overlap mode off by default (tiler OOM on large frames).
- Compute workgroups above the hardware limit (NieR:Automata 960-invocation shader).
- Optional diagnostics through `/sdcard/Download/panvk_env.txt`; off by default.
