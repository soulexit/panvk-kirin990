# panvk-kirin990-winlator

Mesa **PanVK** Vulkan driver for **Winlator** on the Huawei P40 / Kirin 990
(**Mali-G76 MC16**), running on the phone's stock `mali_kbase` (JM) kernel driver
— no root, no custom kernel.

> **Testing build.** This is an experimental, non-conformant driver for one device
> family. Keep a working driver installed as a fallback.

It is built on [zenithblue-oss/panvk-kbase-android](https://github.com/zenithblue-oss/panvk-kbase-android)
(Bifrost geometry / tessellation / transform-feedback emulation, robustness2,
depth bounds, …) plus the Kirin 990 and Winlator fixes in [`patches/`](patches/).
Companion repos: [`panvk-kirin990-termux`](https://github.com/soulexit/panvk-kirin990-termux)
(Termux build, earlier series) and [`panvk-kirin990-armsx2`](https://github.com/soulexit/panvk-kirin990-armsx2)
(ARMSX2 packs).

## At a glance

| | Previous Kirin 990 Winlator pack | This driver (1.2.0-winlator) |
|---|---|---|
| D3D11 (AIO cube) | works | ~260–270 FPS |
| D3D9 (AIO cube) | ~5 FPS | **~74 FPS** with the Performance emulator preset (see *Settings*) |
| DXVK 2.x requirements (robustness2, nullDescriptor, …) | missing | all 56 hard requirements present |
| Geometry shaders / tessellation / transform feedback | advertised but not implemented (GPU hang) | implemented (emulated), tests pass |
| VKD3D-Proton (D3D12) | no | feature level 11_0, shader model 6.5 |
| NieR:Automata | GPU hang on the boot screen | title screen and menu ~20 FPS; **in game still very slow** (vertex shading in its large draws), heavy batches 188/110 → 117/72 ms in 1.2.0 |
| Need for Speed: Most Wanted (D3D9, low) | – | 1.2.0: indexed draws/s 11 500 → 27 300, submit→done 88 → 18 ms (was ~22 FPS, GPU-bound on per-draw jobs) |
| Vulkan CTS (dEQP-VK 1.4.7.0) | – | 94 k cases pass in a 1/10 sample of the whole suite; API/info group 18 failures of 20 k (was 610). See [docs/CTS.md](docs/CTS.md) |

## Device

| Item | Value |
|---|---|
| Phone | Huawei P40 (ANA-NX9) |
| SoC / GPU | Kirin 990 / Mali-G76 MC16 (Bifrost, arch v7) |
| Android | 10 (EMUI 12), API 29, kernel 4.14.116 |
| Kernel GPU driver | stock `mali_kbase`, JM, uAPI 11.16 (56-byte atoms) |
| Driver identity | `panvk`, `PanVK-kbase G76-k990 1.2.0-winlator (Mesa 26.3.0-devel)`, Vulkan 1.3.363, 165 device extensions |

Also checked on a Galaxy A51 (Exynos 9611, Mali-G72 MP3, AOSP 16, kbase uAPI 11.35):
same build runs; all tests pass (see *Results*).

## Install

1. Download `panvk-g76-kirin990-1.2.0-winlator.zip` from the
   [releases](https://github.com/soulexit/panvk-kirin990-winlator/releases).
2. Winlator → **Contents** (or the AdrenoTools/driver manager) → install the zip
   (do not unzip it).
3. In the container (or the game shortcut's settings), set the drivers as in *Settings*.

Release `1.2.0-winlator` (pre-release):

```text
zip:     panvk-g76-kirin990-1.2.0-winlator.zip
sha256:  55868e39f97b1f6eed2cb8aca61088e1733bbb74fa1ac98e54a2c97e7f270a57
.so:     1f5d8e7f039eb4cb461445a4504ee5539f2631f94766812a8144399cdfbf262f
source:  Mesa 5a07217f + zenithblue 917b9b3b series + patches/0001-0036 (tree ded07de903)
```

1.1.0-winlator (CTS fixes, tree 2b001a2bfd) was not published separately; its changes
are in 1.2.0.

Previous release `1.0.0-winlator`:

```text
zip:     panvk-g76-kirin990-1.0.0-winlator.zip
sha256:  2123e49b8958230d89930c03dbc971fa2b2285f1e1ecbcfc76d387be73cdb728
.so:     50e9d366cb064cd2686c60f95d5dc83cf94becaf6261ad883129c879b9528d49
source:  Mesa 5a07217f + zenithblue 917b9b3b series + patches/0001-0018 (tree 60571fd11b)
```

The zip is a standard AdrenoTools package: one stripped `libvulkan_panfrost.so`
(static libc++, no other libraries) and `meta.json` (`minApi` 29).

## Settings (Winlator / DXVK)

These are the settings all results below were measured with (Winlator cmod 4.1).

| Setting | Value | Why |
|---|---|---|
| **Graphics Driver** | `PanVK G76 Kirin 990 1.2.0-winlator` | the game renders with PanVK |
| **Renderer Driver** | **System** (the phone's own Mali driver) | Winlator's screen compositor faults on PanVK on the P40 → black screen. With System the game still renders on PanVK. |
| **Box64 preset** (x86_64 containers) | **Performance** | COMPATIBILITY enables `BOX64_DYNAREC_STRONGMEM=1`: every emulated memory access gets a barrier. The AIO D3D9 test spent 99 % of its CPU time in one emulated byte-swizzle loop (profiled with `simpleperf`, PanVK was 0.17 %). Performance: D3D9 12 → 60 FPS (A51), 5.5 → ~74 FPS (P40). Use COMPATIBILITY only for a game that breaks. |
| **FEXCore preset** (ARM64EC containers) | the fastest one / TSO off | same effect as STRONGMEM above |
| **DXVK** | DXVK-Sarek 1.11/1.12/1.14, DXVK 1.10.3, DXVK 2.3.1 all tested | DXVK 2.x needs robustness2 / null descriptors — present now |
| VKD3D | 2.x / Proton builds | D3D12 FL 11_0 only (no sparse resources → no FL 12_0) |
| Wrapper BCn emulation | not needed | the G76 decodes BC1–BC7 natively (tested) |
| Present mode | default (FIFO/mailbox) | no measurable difference on NieR |

Optional driver options can be set without rebuilding through a text file read at
driver load, `/sdcard/Download/panvk_env.txt` (see *Diagnostics*). Leave it absent or
empty for normal use.

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

### Winlator

| Test (P40) | Result |
|---|---|
| AIO Graphics Test, Direct3D 11 cube | ~260–270 FPS (Sarek 1.11, 1.10.3, 2.3.1) |
| AIO Graphics Test, Direct3D 9 cube | 5.5 FPS (Box64 COMPATIBILITY) → **~74 FPS** (faster preset), DXVK 1.10.3 |
| NieR:Automata (DXVK-Sarek 1.12, Proton 9 x86_64) | boots, title screen and settings menu, ~20 FPS on the title screen; no GPU faults in 3 + min |

| Test (A51, G72) | Result |
|---|---|
| AIO Direct3D 11 cube | 213 FPS |
| AIO Direct3D 9 cube | 12 FPS (COMPATIBILITY) → 60 FPS (PERFORMANCE, display-capped) |

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

- **PanVK as Winlator's *Renderer Driver*** → black screen on the P40 (the compositor's
  first fragment job faults the GPU context). Use Renderer Driver = System.
- **NieR:Automata in game is still very slow.** It is GPU-bound in vertex (position)
  shading of its large draws (~17 500 indices each). 1.2.0 cut those batches by about
  a third; more is needed. Huawei's governor also keeps the G76 at 208–509 MHz instead
  of 600 MHz (not changeable without root).
- No sparse resources → VKD3D-Proton FL 12_0 titles don't start.
- Open CTS failures (details in [docs/CTS.md](docs/CTS.md)): GPU hangs in tessellation
  with storage images in the evaluation shader and in some adjacency-primitive draws
  (geometry/tessellation emulation); update-after-bind descriptors with storage images;
  per-sample shading with several render targets plus depth resolve; texture offsets
  with MIRRORED_REPEAT (Bifrost hardware, also failing upstream); Huawei AFBC
  hardware buffers can't be imported (Winlator's are linear, not affected).

Full investigation log: [docs/FINDINGS.md](docs/FINDINGS.md).

## Diagnostics (`/sdcard/Download/panvk_env.txt`)

`KEY=VALUE` lines (only `PANVK_*`, `PAN_*`, `MESA_*`, `BIFROST_*` and `heap_memory_percent`) are put in the environment when
the driver loads; a leading `!` overrides a variable the app already set.

| Option | Effect |
|---|---|
| `PANVK_GPU_PROF=1` | logcat every 5 s: GPU time per kind of batch, CPU wait time, submit rate, submit→done latency |
| `PANVK_G76_DIAG=1` | log whether the imported swapchain buffers contain non-black pixels |
| `PANVK_HIDE_EXTS=VK_EXT_a,VK_KHR_b` | hide device extensions |
| `MESA_SPIRV_DUMP_PATH=/sdcard/Download/spv` + `!MESA_SHADER_CACHE_DISABLE=true` | dump every SPIR-V shader (named by hash; failed jobs log the hash) |
| `PANVK_OVERLAP=1` | re-enable the overlap mode (not recommended on G76) |
| `PANVK_GRALLOC_GUESS_LINEAR=0` | refuse buffers without mapper metadata (old behaviour) |
| `PANVK_JM_EXPORT_NOWAIT=1` | A/B only: sync-file export without waiting |
| `heap_memory_percent=0.35` | report a smaller device heap, so DXVK keeps less memory (NieR) |
| `PANVK_V7_IDVS=0` / `PANVK_V7_BATCH_MINMAX=0` / `PANVK_V7_HOIST=0` | A/B: no IDVS / one helper job per draw / helpers in place |
| `PANVK_V7_NO_SSBO_VEC=1` / `PANVK_V7_BRANCHY_SSBO=1` | A/B: no 16-byte merge of storage-buffer loads / guarded loads |
| `PANVK_SSBO_VEC_LOG=1` | log per shader how many storage-buffer loads were merged |
| `BIFROST_MESA_DEBUG=shaders` + `PANVK_STDERR_FILE=/sdcard/Download/x.txt` | dump compiled shaders from a Winlator game |

Failed GPU jobs are always logged (`panvk: failed atom N ran: …`).

## Build

```bash
export ANDROID_NDK_ROOT=/path/to/android-ndk-r30
scripts/prepare-tree.sh work          # Mesa 5a07217f + zenithblue series + patches/
scripts/build-android.sh work 1.2.0-winlator   # → work/dist/panvk-g76-kirin990-1.2.0-winlator.zip
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
driver as an ICD, for comparisons).

Host requirements: meson, ninja, python3 (mako, pyyaml), LLVM + clang + libclc +
SPIRV-LLVM-Translator (for Mesa's host code generators), glslang, xxd.

## License

MIT — see [LICENSE](LICENSE). Mesa code in the patches stays under Mesa's licenses;
test files derived from zenithblue-oss/panvk-kbase-android (MIT) are marked in their
headers. See [NOTICE.md](NOTICE.md).
