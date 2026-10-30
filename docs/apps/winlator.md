# Winlator

Tested with Winlator cmod 4.1 on the Huawei P40.

## Install

**Contents** (or the driver manager) → install `panvk-g76-kirin990-<version>.zip` (do not
unzip it). In the container, or a game shortcut's settings, set the drivers as below.
Driver options: `/sdcard/Download/panvk_env.txt` (see the README's *Diagnostics*).

## Settings (Winlator / DXVK)

These are the settings all results below were measured with (Winlator cmod 4.1).

| Setting | Value | Why |
|---|---|---|
| **Graphics Driver** | `PanVK G76 Kirin 990 1.2.4` | the game renders with PanVK |
| **Renderer Driver** | **System** or `PanVK G76 Kirin 990 1.2.4` | Winlator's screen compositor. Up to 1.2.0 PanVK gave a black screen here (fixed in 1.2.1, patch 0039); System still works and keeps the compositor off PanVK. |
| **Box64 preset** (x86_64 containers) | **Performance** | COMPATIBILITY enables `BOX64_DYNAREC_STRONGMEM=1`: every emulated memory access gets a barrier. The AIO D3D9 test spent 99 % of its CPU time in one emulated byte-swizzle loop (profiled with `simpleperf`, PanVK was 0.17 %). Performance: D3D9 12 → 60 FPS (A51), 5.5 → ~74 FPS (P40). Use COMPATIBILITY only for a game that breaks. |
| **FEXCore preset** (ARM64EC containers) | the fastest one / TSO off | same effect as STRONGMEM above |
| **DXVK** | DXVK-Sarek 1.11/1.12/1.14, DXVK 1.10.3(-async), DXVK 2.3.1, DXVK 2.7.1 gplasync (1.2.4) tested | DXVK 2.x needs robustness2 / null descriptors — present. DXVK 2.4 and later: 1.2.4 or newer (black window before). Older DXVK is still faster here (see *Results*) |
| VKD3D | 2.x / Proton builds | D3D12 FL 11_0 only (no sparse resources → no FL 12_0) |
| **Wrapper BCn emulation** | **None** | the G76 decodes BC1–BC7 natively (tested). With emulation on (`WRAPPER_EMULATE_BCN=3`) the wrapper turns BC textures into RGBA8 images and uploads them from too little data: garbage textures, and up to 1.2.3 a lost device (NieR:Automata with DXVK 2.7) |
| Present mode | default (FIFO/mailbox) | no measurable difference on NieR |

Optional driver options can be set without rebuilding through a text file read at
driver load, `/sdcard/Download/panvk_env.txt`, or for apps without storage access (Eden)
`/sdcard/Android/data/<package>/files/panvk_env.txt` (see *Diagnostics*). Leave it absent or
empty for normal use.

## Results

| Test (P40) | Result |
|---|---|
| AIO Graphics Test, Direct3D 11 cube | ~260–270 FPS (Sarek 1.11, 1.10.3, 2.3.1) |
| AIO Graphics Test, Direct3D 9 cube | 5.5 FPS (Box64 COMPATIBILITY) → **~74 FPS** (faster preset), DXVK 1.10.3 |
| NieR:Automata (DXVK-Sarek 1.12, Proton 9 x86_64) | boots, title screen and settings menu, ~20 FPS on the title screen; no GPU faults in 3 + min; in game ~10–14 FPS (see below) |
| AIO Graphics Test, Direct3D 12 cube (VKD3D-Proton 2.14.1) | shows (1.2.2) |
| Need for Speed: Most Wanted (D3D9, low) | runs (see below) |
| Mortal Kombat Komplete Edition, DmC: Devil May Cry (32-bit) | crash in Wine WoW64, not the driver (see below) |

| Test (A51, G72) | Result |
|---|---|
| AIO Direct3D 11 cube | 213 FPS |
| AIO Direct3D 9 cube | 12 FPS (COMPATIBILITY) → 60 FPS (PERFORMANCE, display-capped) |

## Notes per game

- **DXVK 2.7.1 gplasync** (AIO D3D11 test, 1.2.4): 180–230 FPS, against 380–550 with
  DXVK 1.10.3-async in the same ARM64EC container. Per frame the GPU does ~2× the work
  of 1.10.3 (indirect draws, vertex pulling, more passes and queries), and the Huawei
  governor runs it at ~260 MHz instead of ~580 MHz. Before 1.2.4 the window stayed black
  (lost device).

- **NieR:Automata** (DXVK-Sarek 1.12, Proton 9 x86_64): set `heap_memory_percent=0.42` in
  `panvk_env.txt`, otherwise it loops forever at a loading point (DXVK keeps a game under
  80 % of the reported heap). In game it is GPU-bound on vertex shading (~2.7 M triangles per
  frame, ~2 % visible) at ~10–14 FPS at the ocean flight, 800×600; 1.2.2 made the skinning
  shaders 10–20 % faster. Title screen and menus ~20 FPS.
- **NieR:Automata with DXVK 2.7.1 gplasync** (1.2.4): set **BCn emulation to None**. Intro,
  menu and one save render; another save goes black: the GPU faults in its first heavy
  pass (~850 draws). Not caused by 1.2.4's draw changes (`PANVK_V7_IMG_REUSE=0`,
  `PANVK_V7_INDIRECT_FOLD=0` change nothing); still open. With DXVK 1.10.3 or Sarek that
  save does not load either, on 1.2.3 too.
- **Need for Speed: Most Wanted** (D3D9, low): ~22 FPS on 1.1.0, limited by per-draw GPU
  job cost; 1.2.0 raised indexed draws/s 11 500 → 27 300 and submit→done 88 → 18 ms, so
  the GPU is no longer saturated.
- **Mortal Kombat Komplete Edition** and **DmC: Devil May Cry** (32-bit Unreal Engine 3):
  not working, not because of the driver. Both stop with R6025 "pure virtual function call"
  or a page fault inside the game's own code, under Wine's experimental WoW64 mode (Proton 9
  x86_64 and an ARM64 FEX container). No GPU errors, lost device or DXVK errors before the
  crash. MK KE reached a fight once: ~13 FPS on the GPU side at 1280×720 with stutter, stuck
  after ~45 s. Try a Wine build with classic 32-bit support.
