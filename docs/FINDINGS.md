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

Next: avoid the per-draw helper (CPU-known index ranges, caching ranges per index
buffer + range), and look at the Renderer-Driver compositor fault on a device with
`dmesg` access.
