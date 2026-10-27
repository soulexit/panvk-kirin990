# Findings — PanVK on Kirin 990 / Mali-G76 in Winlator

Development log, 2026-10-24/25. Numbers are from one Huawei P40 (ANA-NX9, Android 10,
kbase uAPI 11.16 JM) and one Galaxy A51 (Exynos 9611, Mali-G72 MP3, AOSP 16, rooted).

## 1. Starting point

The previous Kirin 990 Winlator pack (Mesa 26.3-devel on funnymdzz/mesa) worked for
D3D11 but:

- advertised `geometryShader`/`tessellationShader` on Bifrost without implementing
  them; the PanProbe `geometry` test hangs the GPU on it;
- lacked `robustness2`/`nullDescriptor` (hard requirements of DXVK 2.x),
  `transformFeedback`, `depthBounds`;
- NieR:Automata died on its boot screen with `JM atom … failed: event=0x4` and
  `VK_ERROR_DEVICE_LOST`.

The new base is Mesa 5a07217f + zenithblue-oss/panvk-kbase-android (RC5-universal-112),
which emulates geometry shaders, tessellation and transform feedback on Bifrost JM
with compute jobs.

## 2. Bring-up on the Kirin 990 kernel

| Symptom | Cause | Fix |
|---|---|---|
| AHB import `VK_ERROR_INVALID_EXTERNAL_HANDLE` | no mapper4 on Huawei → "refusing guessed layout"; uAPI 11.16 `MEM_IMPORT` is 32 bytes | patches 0001, 0002 |
| Winlator: AIO/NieR exit at `vkAcquireNextImageKHR` | (a) X11 WSI compiled in → wrapper used Mesa's X11 swapchain; (b) `vkImportSemaphoreFdKHR(SYNC_FD, -1)` hung because SYNC_FD semaphores were disabled on JM | Android-only WSI build; patch 0003 |
| Large frames: tiler `OUT_OF_MEMORY` (0x60) / `DATA_INVALID` (0x58) | overlap mode's two 64 MiB tiler-heap halves | patch 0004 |
| Black screen in Winlator | not the game: Winlator's **own** compositor (app process) runs on the selected *Renderer Driver*; its first fragment job faults the GPU context, after which every job is hard-stopped. Game frames were correct in the shared buffers (patch 0005's pixel check). | use Renderer Driver = System; root cause open |

## 3. D3D9 at 5 FPS — not the driver

- Main thread at 96 % CPU, GPU nearly idle; same on every DXVK version.
- Ruled out: fence/query/event polling latency (0.3 ms), CPU reads of uncached memory as
  the main cost (DXVK only uses HOST_COHERENT memory; the GPU is not IO-coherent, tested).
- `simpleperf` on the rooted A51: 99 % of the main thread in box64 JIT code, 0.17 % in
  PanVK. Over 85 % of samples in one ~80-byte loop: AIO's per-byte BGRA→RGBA swizzle of
  its read-back frame, with `dmb` barriers from `BOX64_DYNAREC_STRONGMEM=1`
  (COMPATIBILITY preset).
- Box64 PERFORMANCE preset: A51 12 → 60 FPS, P40 5.5 → ~74 FPS.

## 4. NieR:Automata hang

- Patch 0010/0011 diagnostics: the killed job was a compute-only batch,
  `cs=226d10af wg=30x19x1 local=32x30x1`.
- Mali-G76 `THREAD_MAX_WORKGROUP_SIZE` = 384; the shader asks for 960 (D3D11 allows
  1024, DXVK passes it through). It is a reduction with shared-memory atomics and three
  `OpControlBarrier`s, all at function top level.
- Patch 0012: `panvk_lower_large_workgroup` splits the shader at top-level barriers and
  emits each region K times (K copies per hardware invocation; hardware size ≤ half the
  limit because >32-register shaders halve the threads per core). 960 → 5 × 192.
- Bugs found on the way (host drm-shim + ASan): iterating the function-temporaries list
  while appending clones (endless allocation → kernel OOM panic on the P40, twice);
  built-ins were still variable loads when the pass ran; `nir_undef` is inserted at the
  top of the impl, so it cannot mark a region start.
- Result: standalone test 61 440 / 61 440 correct; NieR reaches the title screen and menu.

## 5. NieR performance (~20 FPS title screen)

- Ruled out: vsync quantization (immediate present: no change), the sync-file export wait
  (never called by the wrapper), CPU round-trips between vertex and fragment jobs (GPU-side
  dependencies), lost completion wake-ups (waiters re-poll every 1 ms), a saturated CPU
  thread (busiest 24 %).
- GPU-bound: our jobs are in flight 93–100 % of the time; submit→done 60–87 ms; DXVK
  keeps 3–4 frames queued.
- Biggest cost: indexed draws. Every indexed draw runs the GPU index min/max helper
  (`PANLIB_DRAW_INDEXED_INDIRECT_MINMAX_HELPER`) before its vertex job, ≈ 0.25 ms of
  vertex/tiler time per draw regardless of mesh size. `PANVK_V7_MINMAX=nofold/0`
  (other helper layouts): no change.
- GPU clock under Huawei's governor: 208–509 MHz, never 600 MHz (`gpufreq/cur_freq`).

## 6. In-game performance (1.2.0)

Measured in game with the GPU profiler (`PANVK_GPU_PROF=1`) and the Mali-G76 hardware
counters, read through the kbase counter reader without root
([`tests/tools/hwcnt.c`](../tests/tools/hwcnt.c)). Job chains were compared with the
vendor driver through an `ioctl` hook ([`tests/tools/jobdump.c`](../tests/tools/jobdump.c),
[`tests/tools/halshim.c`](../tests/tools/halshim.c) loads the vendor driver as an ICD).

**NieR:Automata in game: ~1 FPS** at first, with Android's low-memory killer ending the game.
- Memory: one device-wide shader stack buffer instead of one per command buffer (0029)
  and `heap_memory_percent` (0030): driver memory 1 550 → 1 160 MiB, and the game
  survives.
- IDVS (index-driven vertex shading, what the vendor driver uses) on the G76 (0028):
  vertex/tiler batches about halved.
- Counters: the tiler waited on position shading for almost all of its active time, the
  shader cores ran 0.04 instructions/cycle, and short (sub-cache-line) reads outnumbered
  full ones 10:1. Cause: with robustBufferAccess2 every storage-buffer load was an
  `if`-guarded 32-bit load, alone in its clause. NieR's skinning vertex shaders do 30–80 of
  them per vertex.
- 0034: straight-line bounded loads, and consecutive 32-bit loads of one buffer merged
  into 16-byte loads with per-component bounds checks. The merge first missed every
  NieR load: DXVK computes byte offsets as `(dword_index + k) << 2`, and constant
  folding hadn't run yet. Found with a per-shader merge log, fixed by splitting
  offsets through add/mul/shift recursively. 1 880 of 2 003 bounded loads merged; big
  batches 188/110 → 117/72 ms, draws/s ~1 000 → ~1 450.
- Still GPU-bound in vertex shading (~730 ms of GPU time per frame in heavy scenes).

**Need for Speed: Most Wanted (D3D9, low settings): ~22 FPS, GPU 100 % busy** but tiler and
shader cores mostly idle.
- ~520 small indexed draws per frame (~180 indices) cost 40 ms of vertex/tiler time:
  ~77 µs per draw. On the draw benchmark ([`tests/stress/idraw.c`](../tests/stress/idraw.c))
  PanVK took 48 µs per indexed draw and the vendor driver 10 µs; non-indexed draws cost the
  same on both.
- Job dumps: PanVK ran one index-range helper compute job per indexed draw, the vendor
  driver three per batch. The job manager costs ~35 µs per job.
- 0035: one helper job per batch, a workgroup per draw. Benchmark 22–25 → 7–9.5 ms
  per 520 draws. In game: 11 500 → 27 300 indexed draws/s, submit→done 88 → 18 ms, GPU
  no longer saturated. The first version only covered IDVS draws; a per-path draw
  counter showed NFS's vertex shaders all run without IDVS.
- Without IDVS, small draws are faster than with it (8 vs 12 ms on the benchmark).

**Bug found on the way (0036).** Testing the batched helper without IDVS, 63
`dEQP-VK.glsl.*_vertex` cases failed — on every build since the helpers moved to the
head of the batch (0032). A draw's vertex job waited on its helper, and the helper on
the draw's descriptor-copy job; hoisted, the helper no longer waits, so the vertex
shader could read descriptor tables before they were copied. Bisected by build, then by
switch (`PANVK_V7_HOIST=0` passes), then confirmed with a barrier and with an explicit
dependency. The vertex and IDVS jobs now depend on the copy job directly.

## 7. 1.2.1: large draws, NieR's shaders, the Renderer Driver

**Large indexed draws.** After 1.2.0 about 2 300 of NieR's ~5 900 indexed draws per
second still had more than 4 096 indices and took three jobs each (WRITE_VALUE, search,
helper). Batched helpers now fold draws of up to 65 536 indices (0037): 200 × 17 400
indices 158 → 66 ms on the benchmark, vendor 65 ms; in game 12–17 % off the large batches.

**NieR's vertex shaders, replayed.** One capture run with `MESA_SPIRV_DUMP_PATH` and
`BIFROST_MESA_DEBUG=shaderdb` gave all 128 shaders. [`tests/tools/spvcomp`](../tests/tools/spvcomp/)
compiles a captured vertex shader on a host drm-shim G76 build (same numbers as in game)
and benchmarks it on the phone with zeroed buffers (position shading + culling), on
PanVK and on the vendor driver:
- 14 of 42 position shaders run at half occupancy (35–40 registers); load/store-bound.
- The disassembly had more integer compare/select than FMA: bounds checks. 0038 makes them
  one saturating subtract per 16-byte load and one compare per component: 485 → 389
  instructions, 9–15 % less time per vertex (largest: 9.8 → 8.4 ns, vendor 6.0–8.5 ns).
- Tried and dropped: pushing UBO ranges into the FAU (compiler supports it, PanVK
  doesn't; ceiling measured with unfilled data only 4–11 % per vertex, needs a GPU copy
  job); spilling to 32 registers (the spiller can't split 4-register vector nodes).

**Renderer Driver = PanVK.** Logged with `PANVK_G76_DIAG=1`: the game's frames reached the
compositor with content, but every `vkQueueSignalReleaseImageANDROID` failed (−13), and
every one of the compositor's GPU jobs came back terminated (event 0x4) while it recreated
its swapchain ~40×/s. Reproduced outside Winlator with
[`tests/tools/anbprobe.c`](../tests/tools/anbprobe.c): the runtime copied semaphore
payloads with `vk_drm_syncobj_copy_payloads`, i.e. `SYNCOBJ_TRANSFER`, which the kbase
sync-object emulation does not have. 0039 turns that path off on kbase. Then: 60 presents/s,
0.76 ms of GPU per frame, no failed jobs — the terminated jobs were a side effect.

Next: register pressure of NieR's position shaders.
