# Vulkan CTS results — Mali-G76 (Huawei P40), 1.1.0-winlator

The driver advertises 166 device extensions in 1.0.0 (169 with the instance extensions
Winlator lists). To check that they actually work, the Khronos Vulkan conformance
tests (dEQP-VK, `vulkan-cts-1.4.7.0`) were run on the phone. 1.1.0 is the result:
it fixes what the tests found and stops advertising one extension that could not be
fixed in time (165 device extensions).

## Method

- **Build.** dEQP-VK cross-built as a plain Android arm64 executable
  (`DEQP_ANDROID_EXE`, API 29) — no APK, no root. The CTS needed three small changes
  for this ([`tests/cts/vk-gl-cts-1.4.7.0-android-exe.patch`](../tests/cts/vk-gl-cts-1.4.7.0-android-exe.patch)):
  `DE_OS`/`DE_CPU` set explicitly, a headless Vulkan-only platform for the executable,
  and `memfd_create` through `syscall()` (Android 10 libc lacks it). The patch also
  adds `CTS_DBG_*` switches used to bisect the device feature chain.
  `scripts/build-cts.sh`.
- **Loading the driver.** Without root the system Vulkan loader only loads the
  vendor driver, so `libvkshim.so` ([`tests/cts/vkshim.c`](../tests/cts/vkshim.c))
  stands in for `libvulkan.so`: it dlopens the PanVK `.so` and forwards
  `vkGetInstanceProcAddr`. `--deqp-vk-library-path` points dEQP-VK at the shim.
- **Running.** [`tests/cts/run.py`](../tests/cts/run.py) drives dEQP-VK over adb in
  batches, streams the per-case results, records a case that kills the process as
  *Crash* (or *Timeout* after 90–120 s without output) and resumes after it. One
  dEQP process at a time: two processes sharing the JM GPU make kbase hard-stop long
  jobs (`event=0x4`), which shows up as false device-loss crashes.
  `scripts/run-cts.sh`, summaries with [`tests/cts/summary.py`](../tests/cts/summary.py).
- **Coverage.**
  1. API + info groups in full (20 227 cases): extension entry points, features,
     properties and limits, format properties, device creation.
  2. A 1/10 sample (every 10th case) of every other group in the default must-pass
     list — 285 856 cases. Left out: groups for features the driver does not
     advertise at all (ray tracing, mesh shaders, video, sparse resources, fragment
     shading rate, shader objects, cooperative vectors, …).
  3. Every failing case re-run on its own with the final build.
  4. Regression check: every 5th case that passed in step 2 (18 848 cases) re-run
     with the final build.
- **Debugging.** Failures were narrowed down with standalone probes
  ([`xferprobe.c`](../tests/cts/xferprobe.c): buffer → image → buffer copies with
  configurable device features; [`drmprobe.c`](../tests/cts/drmprobe.c): DRM-modifier
  image creation), the CTS feature-chain bisect switches, a SIGSEGV handler preloaded
  into the CTS process to get module-relative crash addresses
  ([`segv.c`](../tests/cts/segv.c)), and the job-chain decoder
  (`PANVK_DEBUG=trace PANVK_TRACE_JC=1 PANDECODE_DUMP_FILE=stderr`) to see which GPU
  job hung and what its descriptors pointed at.

## Results

### API and info groups (20 227 cases)

| Build | Pass | Not supported | Fail | Crash | Other |
|---|---|---|---|---|---|
| 1.0.0-winlator | 11 351 | 8 259 | **610** | 1 | 6 |
| 1.1.0-winlator | 11 023 | 9 179 | **18** | 1 | 6 |

"Not supported" grows because `VK_EXT_multisampled_render_to_single_sampled` is no
longer advertised. The 18 remaining:

| Cases | Cause |
|---|---|
| 14 AHB format-property / compression tests | Huawei gralloc allocates large GPU-only AHardwareBuffers as AFBC (and RGB888 in a layout the driver doesn't parse); those can't be imported. Winlator's buffers are CPU-accessible and linear — not affected. |
| `driver_properties.conformance_version` | the driver is not conformant (reports 0.0.0.0) |
| `device_memory_budget_multi_instance` | heap usage from a second instance is not counted |
| `create_instance_layer_name_abuse` | test setup: no Android loader behind the shim |
| `many_indirect_draws_on_secondary` | intermittent (passes when re-run) |

plus `null_handle.destroy_device` (crash; the loader normally filters a NULL device),
four *QualityWarning* (allocation callbacks not used for empty layouts) and two
*ResourceError* (out of memory after creating many devices).

### 1/10 sample of the rest (285 856 cases)

Run with an early build that already had the memory-coherency fix (so the large
smoke/copy failures are not in these numbers):

| Pass | Not supported | Fail | Crash | Timeout |
|---|---|---|---|---|
| 94 244 | 186 825 | 3 045 | 1 725 | 16 |

The 4 784 failing cases re-run one by one on the **1.1.0** build:

| Pass now | Not supported now | Still fail | Still crash | Timeout |
|---|---|---|---|---|
| **2 551** | 664 | 830 | 723 | 16 |

(Many of the first-run crashes were caused by the GPU hang fixed in patch 0025 and by
running two dEQP processes in parallel.)

Regression check: 18 846 / 18 848 previously passing cases still pass; the two others
(`memory_model.message_passing.permuted_index.barrier`, an adjacency draw) are
intermittent on 1.0.0 as well.

### What the tests found and what was fixed

| Finding | Effect | Fix |
|---|---|---|
| With `memoryMapPlaced` enabled, every host-visible allocation is an ION dma-buf, allocated CPU-cached; the G76 does not snoop CPU caches, so HOST_COHERENT memory was not coherent | CPU reads stale data, GPU reads stale data. `VK_EXT_map_memory_placed` is what Wine's WoW64 mode uses for 32-bit applications. Smoke tests 2/8, ~600 API failures | ION allocated write-combined for coherent memory types (0019) |
| Cached dma-buf memory was flushed/invalidated with `KBASE_IOCTL_MEM_SYNC`, which does not cover the dma-buf CPU mapping; the heap's zero fill stayed in the CPU cache | HOST_CACHED memory and `VK_EXT_zero_initialize_device_memory` wrong (76 / 496 failures) | `DMA_BUF_IOCTL_SYNC`, clean on allocation (0020) — 0 failures |
| Vertex-shader storage images / texel buffers: the descriptor copy job adds a *buffer index* offset, the draw passed *bytes* | vertex job reads past its attribute table → GPU hang, device lost (hundreds of `synchronization*` cases, `read_image_vertex`). Same code in upstream Mesa | 0025 |
| Null image descriptors are a 1×1 zero image on Bifrost; size/level/sample queries reported that image, format-less loads returned its texel / alpha 1 | robustness2 `null_descriptor`: 0 of 129 sampled cases | recognise the null descriptors by their 0000 swizzle / component order (0026): 117 / 129 |
| `VK_EXT_image_drm_format_modifier`: with `customBorderColorWithoutFormat` on, AFBC was dropped for BGRA even when the application chose AFBC modifiers from the advertised list | NULL dereference in `vkCreateImage` | rule only for optimal tiling (0024): 945 pass, 0 fail |
| Global priority LOW refused | device creation fails | accept LOW as MEDIUM (0021) |
| `maintenance7` update-after-bind dynamic-buffer totals 0 on Bifrost | inconsistent limits | 0022 |
| `VK_EXT_multisampled_render_to_single_sampled`: all supported cases fail | wrong rendering | not advertised on JM (0023) |

### Open failures (1.1.0)

| Area | Cases in the re-run | Notes |
|---|---|---|
| `query_pool.statistics_query` with adjacency primitives | 476 crash | GPU hang in the geometry-emulation path for `*_with_adjacency` topologies |
| `binding_model.shader_access` — tessellation evaluation with storage images / texel buffers | 199 crash, 33 fail | hang is in a helper compute job of the tessellation emulation, not the user shader |
| `pipeline.*.multisample.misc` (+ `renderpasses` resolve) | ~350 fail | per-sample shading (`gl_SampleID`) into several render targets plus depth/stencil resolve; single-attachment MSAA and resolves pass |
| `robustness.robustness2` | 147 fail | null texel buffers, null cube-map size queries, out-of-bounds vertex fetch |
| `binding_model.descriptorset_random` (update-after-bind) | 48 fail | update-after-bind sets with storage images; also intermittent whole-draw failures in vertex shaders with storage writes |
| `glsl.texture_functions` — offsets with MIRRORED_REPEAT | 48 fail | Bifrost applies the offset after mirroring; same failures in upstream Mesa's G52 list |
| `memory_model.message_passing` | 30 fail | device-scope acquire/release between workgroups, intermittent |
| tessellation / geometry / transform-feedback details | ~90 fail | invariance, winding, layered geometry, xfb fuzz |

None of these are known to affect the games tested so far; the most likely to matter
for D3D11 are the tessellation hang (domain shaders that write UAVs) and per-sample
MSAA shading.
