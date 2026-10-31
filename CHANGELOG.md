# Changelog

## 1.2.6 (testing) — 2026-10-31

**OpenGL in Winlator on Android 10.** Winlator's OpenGL (Zink on PanVK) didn't load on the P40:
its `libGL.so.1` needs `memfd_create()`, which Android 10's C library lacks. The zip now
contains `libGL_memfd.so`, which provides it and loads Winlator's libGL; set
`BOX64_LIBGL` to its path in the container ([Winlator guide](docs/apps/winlator.md#opengl-zink-on-android-10)).
AIO OpenGL cube: 104 FPS, no glitches.

- The driver's name in app driver lists is now `PanVK-G76-Kirin990-<version>` (no spaces, so
  the unpacked directory can be used in an environment variable).
- The driver logs the directory it was loaded from (`panvk: driver loaded from ...`).
- Driver otherwise identical to 1.2.5 (patch 0057 only adds the log line).

## 1.2.5 (testing) — 2026-10-31

**ARMSX2 (PS2) runs at full speed passes.** Kingdom Hearts: 1.2.4 5–8 FPS → **27–30 FPS**
(Huawei's own driver: 35–41). ARMSX2 needs its own zip,
`panvk-g76-kirin990-1.2.5-armsx2.zip` (same driver, library named `libvulkan_freedreno.so`).

- **Rasterization-order attachment access on the G76** (0056). Without it PCSX2 records a
  render-target read-back barrier before almost every draw, and each one ended the render
  pass (~860 pass splits per second). Now PCSX2 reads the render target in the shader.
  CTS: 122 pass, 0 fail.
- Barriers inside a render pass that only order tile-local work no longer end the pass (0056).
- **Overlap on by default** (0056): a pass's vertex/tiler work runs during the previous pass's
  fragment work, with two 128 MiB tiler heap halves (0004 had turned it off with 64 MiB halves).
  Barriers from fragment work to vertex, tessellation/geometry, compute or copy work wait for
  the fragment job. `PANVK_OVERLAP=0` turns it off.
- **Uniform-buffer constants in registers** for PCSX2-based emulators (0055, 0056): dynamic
  uniform buffer words a shader reads at fixed offsets are loaded once per draw instead of per
  pixel/vertex (−12 % GPU time on the large passes). `PANVK_V7_PUSH_UBO=1/0` forces it on/off.
- `panvk_env.txt` works in ARMSX2 (its loader redirects `fopen()`), in
  `/sdcard/Android/data/com.armsx2.nightly/files/` (0056).
- New opt-in experiments: `PANVK_V7_CPU_INDEX_RANGE=1` (index ranges found on the CPU),
  `PANVK_V7_DIRECT_TEX_ALL=1` (textures indexed straight from descriptor memory). Neither raised
  Kingdom Hearts' frame rate.
- Diagnostics: `PANVK_GPU_TS=1` (GPU timestamps of vertex/tiler work in the profile), CPU waits
  by caller, `PANVK_SELFPROF=1` per thread and function, `PANVK_STDERR_FILE` works in ARMSX2.
- Known: Kingdom Hearts waits for the GPU twice a frame (PCSX2's three command buffers); the
  remaining gap to Huawei's driver is GPU time per pass, mostly fragment work.
- Not yet re-checked with overlap on: NieR:Automata's heaviest scenes (if a game loses the
  device on large frames, try `PANVK_OVERLAP=0` and report it).
- CTS on the release build: no new failures against 1.2.4 (regression sample 18 846 pass,
  tile barriers, rasterization order, vertex-shader images, 6 890 indirect draws,
  buffer-to-image copies, descriptor indexing, mutable descriptors, sync2; uniform-buffer
  cases 375/375 with the constant push forced on).

## 1.2.4 (testing) — 2026-10-30

**DXVK 2.4 and later run** (tested: DXVK 2.7.1 gplasync, ARM64EC container).

- **Fixed: black window and lost device with DXVK 2.4+** (0047). DXVK 2.x vertex shaders
  read vertices from a texel buffer; an indexed draw with no vertex buffers lost its vertex
  shader's image table, so the GPU read address 0.
- **DXVK 2.7 draws much cheaper** (0049, 0050, 0052). It records D3D11 draws as indirect
  draws: their helper work is now one job per batch instead of up to three jobs per draw, and
  vertex-pulling shaders reuse their image tables instead of a copy job per draw. AIO
  D3D11 test: 104 → 180–230 FPS. DXVK 1.10.3 / Sarek are still about twice as fast here.
- **Fixed: lost device on texture uploads that read past their source** (0053). Hit by
  NieR:Automata with Winlator's BCn emulation on; the copy is clipped. Set **BCn emulation to
  None** for PanVK: the G76 decodes BC natively.
- Fixed: bindless textures and samplers in Wine/FEX processes, whose descriptor pools can lie
  more than 64 GiB from the driver's own memory (0046).
- Profiler: every draw counted, render pass setup per batch kind (0051). Job decoder:
  bindless tables skipped, bad addresses flagged, push uniforms printed (0048, 0054).
- Known issue: NieR:Automata with DXVK 2.7.1, one save goes black (GPU fault, open). Use
  Sarek 1.12 or 1.10.3 for NieR.
- Tools: `tests/stress/vpull.c` (DXVK 2.x-style draws); `idraw` and `vpull` built by
  `scripts/build-tests.sh`.
- CTS on the release build: no new failures (regression sample 18 846 pass plus the two
  known flaky cases, storage-buffer robustness, sync2, subgroups, buffer-to-image copies,
  6 890 indirect-draw and 3 983 vertex-shader image cases).

## 1.2.3 (testing) — 2026-10-28

First release tested in **Eden** (Switch emulator) too. The same zip installs in
Winlator and Eden; versions no longer carry the `-winlator` suffix.

- **Fixed: Eden crashed in `vkCmdClearAttachments`** (0042). Push-descriptor storage was
  sized in API descriptors; a combined image sampler takes up to 5 hardware descriptors.
- **Fixed: NieR:Automata in Eden crashed** (0044) with a GPU fault: its 1024-invocation
  compute shaders use subgroup operations, which the large-workgroup split refused, so they
  ran unsplit on the G76 (max 384). They are split now, with `gl_SubgroupID` /
  `gl_NumSubgroups` rewritten per logical invocation. `tests/tools/lwgtest`: 1.2.2 got
  1 280 of 5 120 invocations wrong, 1.2.3 none.
- Barriers with nothing to wait for no longer end the GPU batch (0043).
- `panvk_env.txt` is also read from `/sdcard/Android/data/<package>/files/` for apps
  without storage access (Eden).
- Profiler: batches/s and why they were split, slow shader compiles, and the last 128 GPU
  jobs when one fails (0043, 0045).
- Tools: `tests/tools/lwgtest/`, `tests/tools/gsreplay/`, `tests/tools/fmtprobe.c`,
  compute mode in `spvcomp`.
- CTS on the release build: no new failures against 1.2.2 (regression sample 18 846 pass
  plus the two known flaky cases, storage-buffer robustness 5 294 / 0 fail,
  synchronization2 and large-workgroup/subgroup lists unchanged).

## 1.2.2-winlator (testing) — 2026-10-27

- **Faster skinned vertex shaders** (NieR:Automata), two compiler changes:
  - integer compare + select become one `CSEL` instruction (0040): 14–17 % fewer
    instructions in NieR's position shaders, 5–11 % faster per vertex;
  - vertex shaders issue storage-buffer loads up to three ahead (0041): the IDVS tiler
    waits on each vertex's position shader, so its latency counts, not the thread count;
    a skinning shader paid one memory latency per bone-matrix row. 5–11 % faster per
    vertex. In game: 32 % more triangles per GPU cycle at the same spot.
- **NieR:Automata stuck in a loop at a loading point**: not the driver. DXVK holds a game
  to 80 % of the reported heap, and `heap_memory_percent=0.35` capped NieR at 2 080 MB, so
  it could not load the next area. `0.42` (cap ~2.5 GB) gets past it.
- **D3D12 checked end to end** with `tests/tools/d3d12diag.c` through VKD3D-Proton 2.14.1
  in Winlator: 14 draw/readback cases (buffers, root constants, root and table CBVs, depth,
  indexed cube, 852×586 padded rows, BGRA8, RGB10A2, MSAA resolve) and presenting to a
  top-level and a child window all pass. AIO Graphics Test's D3D12 cube shows.
- Tools: `tests/tools/d3d12probe.c` (VKD3D-Proton device requirements and bindless heap
  layouts), `tests/tools/d3d12diag.c`; `hwcnt` prints counter indices.

## 1.2.1-winlator (testing) — 2026-10-27

- **Renderer Driver = PanVK works** (Winlator's own Vulkan compositor): every present
  failed in `vkQueueSignalReleaseImageANDROID` (−13), because PanVK copied semaphore
  state with a DRM sync-object operation the kbase backend can't do. The compositor
  showed a black screen and recreated its swapchain ~40×/s. Now 60 presents/s,
  0.76 ms of GPU time per frame. Renderer Driver = System is no longer required.
- **Large indexed draws batched too** (up to 65 536 indices): 200 draws × 17 400
  indices 158 → 66 ms on the draw benchmark (vendor driver 65 ms). NieR:Automata:
  large vertex/tiler batches 12–17 % faster.
- **Cheaper robust-buffer bounds checks** (one saturating subtract per 16-byte load,
  one compare per component): NieR's skinning vertex shaders 9–15 % faster per vertex.
- Tools: `tests/tools/anbprobe.c` (Android present path), `tests/tools/spvcomp/`
  (compile and benchmark captured game shaders on the PC and the phone).

## 1.2.0-winlator (testing) — 2026-10-27

Performance in games, measured with Mali hardware counters (see
[docs/FINDINGS.md](docs/FINDINGS.md) §6). Includes the 1.1.0 fixes below (1.1.0 was not
published separately).

- **Many small indexed draws** (D3D9 games): one index-range helper job per batch instead
  of one per draw. Need for Speed: Most Wanted: 11 500 → 27 300 indexed draws/s,
  submit→done 88 → 18 ms.
- **Fixed: vertex shaders could read stale descriptor tables** (wrong rendering, lost
  device) on draws without IDVS, since the helper jobs moved to the head of the batch.
- **Robust storage-buffer loads** (DXVK's robustBufferAccess2): straight-line code, and
  consecutive 32-bit loads merged into 16-byte loads. NieR:Automata's heavy batches
  188/110 → 117/72 ms.
- IDVS (index-driven vertex shading) on the G76: NieR vertex/tiler time about halved.
- One device-wide shader stack buffer: NieR's driver memory 1 550 → 1 160 MiB (Android's
  low-memory killer had been closing the game); `heap_memory_percent` settable in
  `panvk_env.txt`.
- Index-range helpers run at the head of the batch behind one barrier (no stall per draw).
- GPU profiler: memory, draw-path and shader lines; `PANVK_STDERR_FILE`, `BIFROST_*` keys
  in `panvk_env.txt` for shader dumps.
- Tools: draw benchmark `tests/stress/idraw.c`, robustness check `tests/ssbovec/`,
  hardware-counter reader, job-chain dumper and vendor-driver loader in `tests/tools/`.

Still slow: NieR:Automata in game (vertex shading in its large draws).

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
