# Eden (Switch emulator)

Tested with Eden nightly (`dev.legacy.eden_emulator.nightly`) on the Huawei P40, from
1.2.3 on.

## Install

Settings → GPU driver manager → install `panvk-g76-kirin990-<version>.zip` (do not unzip
it) and select it, globally or per game.

Eden can't read `/sdcard/Download`, so driver options go in
`/sdcard/Android/data/dev.legacy.eden_emulator.nightly/files/panvk_env.txt` (same format, see
the README's *Diagnostics*). Leave it absent for normal use.

## Settings

- Huawei's **performance mode** (battery settings): the power governor otherwise
  keeps the GPU clock low; Zelda went from 8–12 to 16–20 FPS with it.
- No PanVK-specific Eden settings are needed.

## Results

| Test (P40) | Result |
|---|---|
| NieR:Automata – The End of YoRHa Edition | runs without crashing (1.2.2: GPU context fault in a 1024-invocation compute shader, fixed by 0044) |
| Zelda: Echoes of Wisdom | 8–12 FPS in cutscenes, 16–20 FPS with Huawei's performance mode; 2D menu smooth. GPU-bound on ~85 render passes per frame; some stutter |

## Notes

- The first launch of a game is slower while Eden builds its pipeline cache.
- Zelda's limit is the emulator's ~85 render passes per frame (three quarters with one
  draw); each pass loads and stores its tiles on this tile-based GPU. See
  [FINDINGS §9](../FINDINGS.md).
- 1.2.2 and older crash in Eden (`vkCmdClearAttachments`, and NieR's 1024-thread compute
  shaders); use 1.2.3 or newer.
