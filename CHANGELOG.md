# Changelog

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
