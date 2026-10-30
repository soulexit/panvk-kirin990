# panvk-kirin990

Mesa **PanVK** Vulkan driver for the Huawei P40 / Kirin 990 (**Mali-G76 MC16**), on the
phone's stock `mali_kbase` (JM) kernel driver: no root, no custom kernel. **One driver
for every app** that loads AdrenoTools drivers: Winlator, Eden, and others. Each release
is a single zip.

> **Testing build.** This is an experimental, non-conformant driver for one device
> family. Keep a working driver installed as a fallback.

It is built on [zenithblue-oss/panvk-kbase-android](https://github.com/zenithblue-oss/panvk-kbase-android)
(Bifrost geometry / tessellation / transform-feedback emulation, robustness2,
depth bounds, …) plus the Kirin 990 fixes in [`patches/`](patches/). This repository was
`panvk-kirin990-winlator` until 1.2.3; old links redirect here. Archived, read-only
predecessors: [`panvk-kirin990-termux`](https://github.com/soulexit/panvk-kirin990-termux)
(Termux build, earlier series) and [`panvk-kirin990-armsx2`](https://github.com/soulexit/panvk-kirin990-armsx2)
(ARMSX2 packs); new releases for those apps come from here too.

## Download

Latest: **1.2.4** (pre-release), `panvk-g76-kirin990-1.2.4.zip` from
[releases](https://github.com/soulexit/panvk-kirin990/releases).

```text
sha256:  9e09250cd1b16301a5f5cf282d6da7dfee35375b1fd2346838adfd2f78c0931a
.so:     c92dcaf1fd8d91265930211f3ac1337529239d65a86b2ce2e69e38d3f3d414be
source:  Mesa 5a07217f + zenithblue 917b9b3b series + patches/0001-0054 (tree 140f694ccd)
```

Older releases and hashes: [docs/RELEASES.md](docs/RELEASES.md). Changes: [CHANGELOG.md](CHANGELOG.md).

## Install

Install the zip as it is (don't unzip it) in the app's driver manager:

| App | Where | Driver options file | Guide |
|---|---|---|---|
| **Winlator** (cmod 4.1) | Contents / driver manager; then Graphics Driver in the container | `/sdcard/Download/panvk_env.txt` | [docs/apps/winlator.md](docs/apps/winlator.md) |
| **Eden** (Switch) | Settings → GPU driver manager | `/sdcard/Android/data/dev.legacy.eden_emulator.nightly/files/panvk_env.txt` | [docs/apps/eden.md](docs/apps/eden.md) |
| Other AdrenoTools apps | their driver manager | `/sdcard/Download/` if the app can read it, else `/sdcard/Android/data/<package>/files/` | untested |

## Games and tests

Everything tested so far on the P40 (Mali-G76) unless noted. FPS as seen on screen or from
the driver's profiler; the newest version each was tested with.

| App | Game / test | API | Version | Result |
|---|---|---|---|---|
| Winlator | AIO Graphics Test, D3D11 cube | D3D11 (DXVK) | 1.0.0 → | ~260–270 FPS (Sarek 1.11, 1.10.3, 2.3.1, x86_64 container); 1.2.4, ARM64EC container: 1.10.3-async 380–550 FPS; A51 (G72) 213 FPS |
| Winlator | AIO Graphics Test, D3D11 cube | D3D11 (DXVK 2.7.1 gplasync) | 1.2.4 | **180–230 FPS** (1.2.3: black window, device lost; first 1.2.4 build: 104 FPS). The GPU governor keeps the clock near 260 MHz for this load, 1.10.3 gets ~580 MHz |
| Winlator | AIO Graphics Test, D3D9 cube | D3D9 (DXVK) | 1.0.0 → | ~74 FPS with Box64 Performance (5.5 FPS with Compatibility); A51 60 FPS |
| Winlator | AIO Graphics Test, D3D12 cube | D3D12 (VKD3D-Proton 2.14.1) | 1.2.2 | cube shows |
| Winlator | `d3d12diag` (14 draw/readback cases, presenting) | D3D12 | 1.2.2 | all pass |
| Winlator | NieR:Automata | D3D11 (DXVK-Sarek 1.12) | 1.2.2 | **playable but slow**: ~20 FPS menus, ~10–14 FPS in game (GPU-bound vertex shading); needs `heap_memory_percent=0.42` |
| Winlator | NieR:Automata | D3D11 (DXVK 1.10.3-async) | 1.2.4 | menu 47 FPS; one save fails to load (also on 1.2.3: Sarek gets stuck, 1.10.3 crashes in game code) |
| Winlator | NieR:Automata | D3D11 (DXVK 2.7.1 gplasync) | 1.2.4 | intro and menu correct, one save loads and renders, **another save goes black** (GPU fault in its first heavy pass, open). Needs BCn emulation **off** |
| Winlator | Need for Speed: Most Wanted (low) | D3D9 | 1.2.0 | runs; ~22 FPS on 1.1.0, per-draw cost cut 2.4× in 1.2.0 |
| Winlator | Mortal Kombat Komplete Edition | D3D9, 32-bit | 1.2.2 | **crashes** (R6025 / page fault in the game under Wine WoW64; no driver errors); ~13 FPS before that |
| Winlator | DmC: Devil May Cry | D3D9, 32-bit | 1.2.2 | **crashes** at start (R6025, same WoW64 issue) |
| Winlator | Renderer Driver = PanVK (compositor) | Vulkan | 1.2.1 | works, 60 Hz |
| Eden | NieR:Automata – The End of YoRHa Edition | Vulkan | 1.2.3 | **runs, no crashes** (crashed on 1.2.2) |
| Eden | Zelda: Echoes of Wisdom | Vulkan | 1.2.3, 1.2.4 | **playable**: 16–20 FPS with Huawei performance mode (8–12 without), some stutter; 2D menu smooth |
| – | Vulkan CTS (dEQP-VK 1.4.7.0) | Vulkan 1.3 | 1.1.0, 1.2.4 | 94 k cases pass in a 1/10 sample of the suite (1.1.0); 1.2.4: no new failures on 37 600 cases (regression sample, storage-buffer robustness, sync2, subgroups, buffer-to-image copies, 6 890 indirect-draw and 3 983 vertex-shader image cases; one image-robustness case failed once and passed 3/3 on its own). [docs/CTS.md](docs/CTS.md) |

Per-game settings and details: [Winlator](docs/apps/winlator.md#notes-per-game),
[Eden](docs/apps/eden.md). Tested something else? Open an issue with the app, game,
driver version and what you saw.

## Driver at a glance

| | Previous Kirin 990 Winlator pack | This driver (1.2.4) |
|---|---|---|
| DXVK 2.x requirements (robustness2, nullDescriptor, …) | missing | all 56 hard requirements present |
| Geometry shaders / tessellation / transform feedback | advertised but not implemented (GPU hang) | implemented (emulated), tests pass |
| VKD3D-Proton (D3D12) | no | feature level 11_0, shader model 6.5; draws, readback and presenting checked (1.2.2); AIO D3D12 cube shows |
| Vulkan CTS (dEQP-VK 1.4.7.0) | – | 94 k cases pass in a 1/10 sample of the whole suite; API/info group 18 failures of 20 k (was 610). See [docs/CTS.md](docs/CTS.md) |

## Device

| Item | Value |
|---|---|
| Phone | Huawei P40 (ANA-NX9) |
| SoC / GPU | Kirin 990 / Mali-G76 MC16 (Bifrost, arch v7) |
| Android | 10 (EMUI 12), API 29, kernel 4.14.116 |
| Kernel GPU driver | stock `mali_kbase`, JM, uAPI 11.16 (56-byte atoms) |
| Driver identity | `panvk`, `PanVK-kbase G76-k990 1.2.4 (Mesa 26.3.0-devel)`, Vulkan 1.3.363, 165 device extensions |

Also checked on a Galaxy A51 (Exynos 9611, Mali-G72 MP3, AOSP 16, kbase uAPI 11.35):
same build runs; all tests pass (see *Results*).

## Results

### On-device driver tests (adb, no root)

Each test dlopens the driver directly (`vk_icdGetInstanceProcAddr`), renders or
computes, reads back and compares. P40 and A51, build 1.0.0-winlator (1.1.0-winlator re-run on the P40: same results):

| Group | Tests | P40 (G76) | A51 (G72) |
|---|---|---|---|
| PanProbe Vulkan suite (zenithblue): geometry, tessellation, xfb, pipeline stats, multi-viewport, fill modes, clip/cull, depth bounds, robustness2, robust image access 2, descriptors, samplers, blending, occlusion queries, BC decode, large draws, VKD3D heap/timeline, DXVK/VKD3D requirement probes, … | 38 | 37 pass + `tess_perf` (benchmark, runs past the 90 s limit) | same |
| AHB import/render/readback (Huawei gralloc handles, 964×530 BGRA with padded stride) | 7 | pass | pass |
| Native-buffer (Android WSI) swapchain image, 1080×2340 | 1 | pass | – |
| Compositor-style compute: `imageStore` into a swapchain image while sampling an AHB | 1 | pass | – |
| 960-invocation compute workgroup with shared atomics + 3 barriers (`lwg`) | 1 | 61 440 / 61 440 correct | – |
| sync_fd semaphore/fence import/export (`syncfd`) | 1 | matches the previous working pack (signaled -1 fences, no hangs) | – |
| Tiler stress, 1600×900, up to 300 k large triangles | 1 | pass (old pack/overlap mode: tiler OOM) | – |

Two expected non-passes when run on purpose: swapchain buffers allocated *without*
CPU-access usage are AFBC on Huawei gralloc and are refused by design (the driver
requests linear buffers itself).

### Feature coverage (from the requirement probes)

- **DXVK**: 56 / 56 hard requirements; soft 26 / 38. Missing: `shaderFloat64`,
  `shaderFloat16`, `drawIndirectCount`, `shaderOutputLayer`, `samplerFilterMinmax`,
  `VK_EXT_memory_priority`, `VK_EXT_multi_draw`, `VK_EXT_sample_locations`.
- **VKD3D-Proton**: device creation OK (0 hard gaps), FL 11_0, SM 6.5, resource
  binding tier 3, no tiled resources / conservative rasterization / ROVs / mesh /
  ray tracing.

## What was fixed (patch series)

| Patch | Problem → fix |
|---|---|
| 0001 | Kernel uAPI 11.16 uses a 32-byte `MEM_IMPORT`; the driver only knew 24/40 bytes → swapchain/AHB imports failed. Retry with the old layout. |
| 0002 | Huawei gralloc has no mapper4 metadata → read width/stride/AFBC flag from Huawei's `private_handle_t`. |
| 0003 | On JM the sync-file export was disabled, so the Vulkan runtime had no SYNC_FD semaphores and Winlator's wrapper hung in `vkAcquireNextImageKHR`. Export now waits for the payload and returns a signaled (-1) fence. |
| 0004 | "Overlap" mode split the tiler heap into two 64 MiB halves → tiler out-of-memory on big frames on the G76. Off by default on Bifrost. |
| 0007 | Swapchain buffers from Huawei gralloc were AFBC (unusable) → request CPU-access usage so gralloc allocates them linear. |
| 0010 | Samsung (A51, AOSP 16, HIDL mapper 2.x only): import buffers as linear when no mapper metadata exists. |
| **0012** | **NieR:Automata hang:** a D3D11 compute shader with 32×30 = 960 invocations per workgroup; the G76 holds at most 384 → the barrier deadlocked and the kernel killed the job (`event=0x4`, DEVICE_LOST). New compiler pass: if all barriers are top-level, the workgroup runs as K copies per hardware invocation (960 → 5 × 192), regions between barriers are emitted once per logical invocation. |
| 0019, 0020 | **HOST_COHERENT memory not coherent** when `memoryMapPlaced` is enabled (`VK_EXT_map_memory_placed`, what Wine WoW64 uses for 32-bit games): every host-visible allocation became an ION buffer allocated CPU-cached, and the G76 does not snoop CPU caches. Now write-combined for coherent memory; cached buffers use `DMA_BUF_IOCTL_SYNC` for flush/invalidate and are cleaned after the kernel's zero fill. Found by CTS: smoke 2/8 → 8/8. |
| 0021 | Global priority LOW was refused (only MEDIUM accepted). |
| 0022 | `VK_KHR_maintenance7` update-after-bind dynamic-buffer totals were 0 on Bifrost. |
| 0023 | `VK_EXT_multisampled_render_to_single_sampled` not advertised on JM: every supported CTS case failed; DXVK/VKD3D-Proton don't use it. |
| 0024 | DRM-modifier image creation crashed (NULL modifier handler) when the application's modifier list was AFBC-only and the device had `customBorderColorWithoutFormat` on. |
| 0025 | **GPU hang with images in vertex shaders**: the descriptor copy job got a byte offset where it adds a buffer index; the vertex job read past its attribute table. Also present in upstream Mesa. |
| 0026 | Null image descriptors: size/level/sample queries returned 1 instead of 0; format-less storage-image loads returned non-zero. |
| 0027 | Debugging knobs used for the CTS work (`PANVK_NO_LWG`, `PANVK_TRACE_JC`, `PANVK_IMPORT_LOG`). |
| 0028 | IDVS (index-driven vertex shading) on the G76, as the vendor driver does: NieR's vertex/tiler batches about halved. `PANVK_V7_IDVS=0` turns it off. |
| 0029, 0030 | One device-wide shader stack buffer instead of one per command buffer (NieR: driver memory 1 550 → 1 160 MiB; Android's low-memory killer had been closing the game); `heap_memory_percent` from `panvk_env.txt`. |
| 0031, 0033 | GPU profiler lines (memory, draws, shaders); `BIFROST_*` keys and `PANVK_STDERR_FILE` in `panvk_env.txt` for shader dumps. |
| 0032 | Index-range helper jobs of indexed draws moved to the head of the batch behind one barrier, instead of a prefetch stall per draw. |
| **0034** | **Robust storage-buffer loads** (DXVK's robustBufferAccess2) were `if`-guarded 32-bit loads, one per clause: NieR's skinning shaders ran at 0.04 instructions/cycle. Now straight-line, and consecutive 32-bit loads of one buffer become one 16-byte load with per-component bounds checks. NieR heavy batches 188/110 → 117/72 ms. |
| **0035** | **One index-range helper job per batch** instead of one per indexed draw (~35 µs of job-manager time each). NFS: Most Wanted 11 500 → 27 300 indexed draws/s. |
| **0036** | **Vertex shaders could read stale descriptor tables** on draws without IDVS since 0032 (63 CTS vertex-shader cases, wrong rendering / lost device): draws now wait for their descriptor copy job. |
| 0037 | Batched index-range helpers also take draws of up to 65 536 indices (one workgroup each) instead of three jobs per draw: 200 × 17 400 indices 158 → 66 ms (vendor 65 ms). |
| 0038 | Robust-buffer bounds checks with one saturating subtract per 16-byte load and one compare per component (were two compares, an add and two ANDs per component): NieR skinning shaders 9–15 % faster per vertex. |
| **0039** | **Winlator Renderer Driver = PanVK: black screen.** `vkQueueSignalReleaseImageANDROID` failed on every present (−13): PanVK copied semaphore payloads with a DRM sync-object transfer the kbase backend doesn't emulate. Off on kbase; the runtime does an empty submit instead. |
| 0040 | Integer compare + select fused into one `CSEL` (was ICMP + MUX): 14–17 % fewer instructions in NieR's position shaders, 5–11 % faster per vertex. `BIFROST_NO_CSEL_FUSE=1` turns it off. |
| 0041 | Vertex shaders issue storage-buffer loads up to three ahead of their use, with their address arithmetic, instead of right before it: NieR's skinning shaders 5–11 % faster per vertex (the IDVS tiler waits on each vertex's position shader). `BIFROST_NO_HOIST=1` turns it off. |
| 0042 | **Eden crash in `vkCmdClearAttachments`**: push-descriptor storage was sized in API descriptors, but a combined image sampler takes up to 5 hardware descriptors → the meta clear overwrote memory. Sized in hardware descriptors now. |
| 0043 | Barriers with nothing to wait for (destination stage only TOP_OF_PIPE, or source only BOTTOM_OF_PIPE) no longer end the batch. `panvk_env.txt` is also read from `/sdcard/Android/data/<package>/files/`. Profiler: why batches were split, slow shader compiles. |
| **0044** | **Eden NieR:Automata crash**: the large-workgroup split (0012) refused shaders with subgroup operations, so 1024-invocation workgroups ran unsplit on the G76 (wrong results, then a GPU fault). `gl_SubgroupID` / `gl_NumSubgroups` are now rewritten per logical invocation and such shaders are split. |
| 0045 | Profiler: on the first failed GPU job, log the last 128 jobs (kind, duration, result). |
| 0046 | Bindless (update-after-bind) textures and samplers indexed set memory from one device-constant base with a 32-bit index; in Wine/FEX processes descriptor pools land more than 64 GiB away. The base is now per draw/dispatch, from the bound sets. `PANVK_VA_HOLE_GB=n` reproduces the placement in tests. |
| **0047** | **DXVK 2.4+ black window / lost device**: an indexed draw without vertex buffers (DXVK 2.x pulls vertices from a texel buffer) made a zero-sized allocation, read as out of memory, and the VS image attribute table stayed NULL. |
| 0048 | Job decoder: bindless texture/sampler tables are not decoded. |
| **0049** | **Indexed indirect draws** (DXVK 2.7 records D3D11 draws this way) got a WRITE_VALUE, a 64-workgroup index search sized by the whole index buffer and a helper job: now one fused job, batched per batch. AIO D3D11 with DXVK 2.7.1: 104 → ~190 FPS. |
| 0050 | Non-indexed indirect draws join the batch's helper job: 0.22–0.27 → 0.05 ms per draw on the benchmark. |
| 0051 | Profiler: every draw counted, render pass size, attachments and load ops in the batch kind. |
| 0052 | A vertex-pulling VS with storage images or texel buffers reuses its image tables while the VS and descriptors are unchanged (no copy job per draw). |
| **0053** | **Buffer-to-image copies that read past their source** are clipped to the rows that exist: the copy shader has no source bounds and the memory after a buffer is often unmapped on kbase (lost device). Hit by Winlator's BCn emulation with DXVK 2.7. |
| 0054 | Job decoder: flags attribute buffers outside known memory, prints push uniforms. |
| 0005, 0006, 0008, 0009, 0011, 0013–0018 | Diagnostics (off unless enabled in `panvk_env.txt`): swapchain pixel check, extension hiding, failed-job batch description, GPU-time profiler, wait/latency statistics. |

Not in this series but required for Winlator: Android-only WSI (`-Dplatforms=android`).
With the X11 WSI compiled in, Winlator's wrapper presents through Mesa's X11
swapchain and every DXVK version fails at `vkAcquireNextImageKHR`.

## How it was tested

1. **adb without root.** `/dev/mali0` is world-accessible on the P40, so the driver
   and test binaries run from `/data/local/tmp` over adb; no Winlator or taps needed.
   `scripts/build-tests.sh`, `scripts/deploy-tests.sh`, `scripts/run-tests.sh`.
2. **Old vs new driver** on the same tests (feature probes, AHB/ANB properties,
   sync behaviour) to find what Winlator's wrapper depends on.
3. **Winlator automation.** Game shortcuts are started over adb
   (`am start -n com.winlator.cmod/.XServerDisplayActivity --es shortcut_path <.desktop> --ei container_id <n>`,
   intents from `dumpsys shortcut`); screenshots with `screencap`; driver logs in logcat.
4. **In-driver diagnostics** switched on from `panvk_env.txt`: which GPU job failed and
   what its batch contained (identified the 960-thread compute shader), SPIR-V dumps
   (`MESA_SPIRV_DUMP_PATH`) to replay the failing shader standalone, a per-batch
   GPU-time profiler.
5. **Host debugging for compiler changes.** The same Mesa tree built for x86 with
   `drm-shim` (`PAN_GPU_ID=7211` = G76), ASan and `NIR_DEBUG=validate`; new compiler
   code is proven there first. On the phone new code runs under `ulimit -v`, because
   an early bug in the workgroup pass exhausted memory and kernel-panicked the P40.
6. **Khronos Vulkan CTS (dEQP-VK 1.4.7.0)** built as a plain Android executable and run
   over adb against the driver `.so` through a small loader shim (no root, no APK):
   the whole API/info group, a 1/10 sample of every other group (286 k cases), every
   failure re-run on its own, and the fixes checked against a 19 k-case sample of
   previously passing tests for regressions. Method, numbers and remaining failures:
   [docs/CTS.md](docs/CTS.md). `scripts/build-cts.sh`, `scripts/run-cts.sh`.
7. **Rooted second phone (A51).** `simpleperf` on the Winlator game process (found the
   D3D9 bottleneck in emulated code, not the driver) and the kernel log.

## Known issues / next steps

- **NieR:Automata with DXVK 2.7.1**: one save goes black (GPU fault in its first heavy pass,
  ~850 draws; cause not found; 1.2.4's draw changes are not it). Use DXVK-Sarek 1.12 or
  1.10.3 for NieR. Set Winlator's **BCn emulation to None** for PanVK containers.
- **DXVK 2.7 is ~2× slower than DXVK 1.10 / Sarek** in the AIO test (180–230 vs 380–550 FPS):
  more GPU work per frame (indirect draws, vertex pulling, more passes and queries), and the
  Huawei governor clocks the GPU at ~260 MHz for it instead of ~580 MHz.

- **Eden: Zelda: Echoes of Wisdom is GPU-bound** (16–20 FPS): the emulator issues ~85 render
  passes per frame, three quarters with one draw, and each pass loads and stores its tiles.
  Passes on the same target can't be merged (they are feedback loops: the next pass samples
  what the previous one drew). Folding clear-only passes into the next pass might gain 5–10 %.
- A 256-invocation compute shader with a barrier inside control flow still runs unsplit
  (the split needs top-level barriers); no game is known to break on it.
- **NieR:Automata in game is GPU-bound** (~10–14 FPS at the ocean flight, 800×600). It
  sends ~2.7 M triangles per frame of which ~2 % are visible; the tiler waits on position
  shading for 94 % of its active time, vertex by vertex, so position-shader latency limits
  it (register count and thread occupancy made no difference when measured). Huawei's
  governor also keeps the G76 at 208–509 MHz instead of 600 MHz (not changeable without root).
- **NieR needs `heap_memory_percent=0.42`** (or so) in `panvk_env.txt`: with 0.35 DXVK caps
  it at 2 080 MB and it loops forever at a loading point. Higher values risk Android's
  low-memory killer.
- No sparse resources → VKD3D-Proton reports FL 11_0. Games that require 12_0/12_1 start
  with `VKD3D_FEATURE_LEVEL=12_0` (or `12_1`) in the container's environment, as long as
  they don't actually use tiled resources.
- Open CTS failures (details in [docs/CTS.md](docs/CTS.md)): GPU hangs in tessellation
  with storage images in the evaluation shader and in some adjacency-primitive draws
  (geometry/tessellation emulation); update-after-bind descriptors with storage images;
  per-sample shading with several render targets plus depth resolve; texture offsets
  with MIRRORED_REPEAT (Bifrost hardware, also failing upstream); Huawei AFBC
  hardware buffers can't be imported (Winlator's are linear, not affected).

Full investigation log: [docs/FINDINGS.md](docs/FINDINGS.md).

## Diagnostics (`panvk_env.txt`)

The file is `/sdcard/Download/panvk_env.txt`; when the app can't read it,
`/sdcard/Android/data/<package>/files/panvk_env.txt`. `KEY=VALUE` lines (only `PANVK_*`, `PAN_*`, `MESA_*`, `BIFROST_*` and `heap_memory_percent`) are put in the environment when
the driver loads; a leading `!` overrides a variable the app already set.

| Option | Effect |
|---|---|
| `PANVK_GPU_PROF=1` | logcat every 5 s: GPU time per kind of batch, CPU wait time, submit rate, submit→done latency, batches/s and why they were split, shader compiles over 5 ms; on the first failed job the last 128 jobs (`HIST`) |
| `PANVK_SPLIT_NOOP_BARRIERS=1` | A/B: no-op barriers end the batch again (before 1.2.3) |
| `PANVK_G76_DIAG=1` | log whether the imported swapchain buffers contain non-black pixels |
| `PANVK_HIDE_EXTS=VK_EXT_a,VK_KHR_b` | hide device extensions |
| `MESA_SPIRV_DUMP_PATH=/sdcard/Download/spv` + `!MESA_SHADER_CACHE_DISABLE=true` | dump every SPIR-V shader (named by hash; failed jobs log the hash) |
| `PANVK_OVERLAP=1` | re-enable the overlap mode (not recommended on G76) |
| `PANVK_GRALLOC_GUESS_LINEAR=0` | refuse buffers without mapper metadata (old behaviour) |
| `PANVK_JM_EXPORT_NOWAIT=1` | A/B only: sync-file export without waiting |
| `heap_memory_percent=0.42` | report a smaller device heap, so DXVK keeps less memory (NieR: 0.42; at 0.35 it can't load the next area) |
| `BIFROST_NO_CSEL_FUSE=1` / `BIFROST_NO_HOIST=1` | A/B: no compare+select fusion / vertex shader loads where nir_opt_move puts them |
| `PANVK_V7_IDVS=0` / `PANVK_V7_BATCH_MINMAX=0` / `PANVK_V7_HOIST=0` | A/B: no IDVS / one helper job per draw / helpers in place |
| `PANVK_V7_NO_SSBO_VEC=1` / `PANVK_V7_BRANCHY_SSBO=1` | A/B: no 16-byte merge of storage-buffer loads / guarded loads |
| `PANVK_SSBO_VEC_LOG=1` | log per shader how many storage-buffer loads were merged |
| `BIFROST_MESA_DEBUG=shaders` + `PANVK_STDERR_FILE=/sdcard/Download/x.txt` | dump compiled shaders from a Winlator game |

Failed GPU jobs are always logged (`panvk: failed atom N ran: …`).

## Build

```bash
export ANDROID_NDK_ROOT=/path/to/android-ndk-r30
scripts/prepare-tree.sh work          # Mesa 5a07217f + zenithblue series + patches/
scripts/build-android.sh work 1.2.4   # → work/dist/panvk-g76-kirin990-1.2.4.zip
scripts/build-tests.sh work           # → out/tests, out/probes
scripts/deploy-tests.sh work/dist/libvulkan_panfrost.so
adb shell sh /data/local/tmp/run-tests.sh /data/local/tmp/drv
scripts/build-cts.sh work             # optional: dEQP-VK for Android, → work/cts-dist
adb push work/cts-dist/. /data/local/tmp/cts/
scripts/run-cts.sh work/dist/libvulkan_panfrost.so caselist.txt results/
```

Performance tools (built by hand with the NDK clang, run over adb): `tests/stress/idraw.c`
(indexed-draw cost; part of the stress tests), `tests/ssbovec/` (robust 16-byte load
check), `tests/tools/hwcnt.c` (Mali-G76 hardware counters, no root),
`tests/tools/jobdump.c` (`LD_PRELOAD` job-chain dump), `tests/tools/halshim.c` (vendor
driver as an ICD, for comparisons), `tests/tools/anbprobe.c` (Android present path:
`vkQueueSignalReleaseImageANDROID`), `tests/tools/spvcomp/` (compile a shader captured
with `MESA_SPIRV_DUMP_PATH` on a host drm-shim build, or benchmark it on the phone
against the vendor driver; compute mode with `SPVCOMP_FILL` / `SPVCOMP_GROUPS`),
`tests/tools/lwgtest/` (1024-invocation workgroup with subgroup operations, shared memory
and barriers), `tests/tools/gsreplay/` (geometry-shader emulation replay),
`tests/tools/fmtprobe.c` (format feature dump), `tests/stress/vpull.c` (DXVK 2.x-style
draws: vertex pulling from a texel buffer, indexed or non-indexed indirect, large shared
index buffers).

Host requirements: meson, ninja, python3 (mako, pyyaml), LLVM + clang + libclc +
SPIRV-LLVM-Translator (for Mesa's host code generators), glslang, xxd.

## License

MIT — see [LICENSE](LICENSE). Mesa code in the patches stays under Mesa's licenses;
test files derived from zenithblue-oss/panvk-kbase-android (MIT) are marked in their
headers. See [NOTICE.md](NOTICE.md).
