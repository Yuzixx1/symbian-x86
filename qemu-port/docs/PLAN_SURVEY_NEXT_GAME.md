# PLAN_SURVEY_NEXT_GAME — cost estimate for a second genuine-style game port

**Status:** SURVEY ONLY. No code written, no commits, no `make clean`, no QEMU
runs. `kernelhwsrv/` untouched (read-only). Scope: `qemu-port/ports/doom`,
`qemu-port/tools/r1_doom_*`, exec slots, EUSER imports, EWSRV/VBE framebuffer,
input bridge, AC'97 audio path.

Context: DOOM/Freedoom is the declared integration proof
(`docs/DOOM_INTEGRATION_VERIFICATION.md:13`, `docs/FINAL_PROJECT_SUMMARY.md:18`).
This survey prices a *second* game on the same bridges.

---

## 1. Current pipeline reusable parts

Everything a second game needs already exists and is game-agnostic:

| Bridge | Mechanism | Reuse for game 2 |
|---|---|---|
| Framebuffer/VBE | VBE mode 0x118, 1024x768x32 LFB (0xFD000000, pitch 4096); direct whole-screen blit via **slow exec 270 op 3** (`FBO_FBBLIT`, `variant/qemu_fbblit.h`) with `src_bpp` 8 or 32; fb info via op 2. DOOM paints 640x400 RGBA8888 through it (`ports/doom/e32_platform.c:211`). | Direct copy; fbblit already accepts 8-bit palettized sources, ideal for raycaster/2D games. |
| EWSRV path | Genuine EWSRV raw-event queue + display lists is the *windowed* path (GUI apps); DOOM bypasses EWSRV and owns the LFB. | A full-screen game should copy DOOM's direct-fbblit path, not EWSRV windowing (one full-screen app at a time, `docs/GRAPHICAL_DESKTOP.md`). |
| Input | PS/2 IRQ1 → PortCon `con_tryread()` make/break (break = 0x80\|ascii); ANSI ESC[A-D arrows; DIRECT mode after first break byte + time-based fallback for make-only serial (`ports/doom/e32_platform.c`). Separate EWSRV injection (ISR→DFC→`Kern::AddEvent`, `docs/EWSRV_INPUT_INJECTION.md`) exists for GUI apps. | Copy DOOM's `e32_platform.c` input half verbatim; key map is the only game-specific part. |
| Timing | exec 68 (`EExecTimeNow`, µs, 2000 epoch) → 35 Hz tic clock. | Direct reuse. |
| Audio | **exec 240** multiplexed: PCM ops `PLAY/STATUS/STOP` (0x80000001..3) with 8-slot kernel DMA queue, 16-bit stereo, rates 11025–48000 (`ports/doom/e32_audio.h`, `variant/qemu_audio_exec.cpp`); legacy tone = a1=Hz. AC'97 PCI device, headless-safe. | Submit PCM directly; no new exec needed. Games with sequenced music need a small synth (DOOM's MUS synth in `i_e32sound.c` is the template). |
| Heap/stdio markers | RChunk-backed malloc via exec 107/108 (`ports/doom/e32_shim.c:952`); serial markers via **genuine EUSER.DLL ordinal 915** PE2 import (`tools/elf2e32.py --euser-import 915 --iat-off`), DebugPrint exec 86 fallback. | Copy `e32_shim.c`/`e32_shim.h` nearly verbatim; new binary just needs its own `--euser-import 915` + IAT offset. |
| Assets/disk | 64 MB MBR image, 48 MB FAT32 at LBA 2048; WAD staged as 8.3 `FDOOM1.WAD` via `tools/mkgrubiso.py --wad`; large-file API (`file_open/file_size/file_read_at`) works on AHCI `C:` — **but X: RAMDISK (device 8) opens fail with KErrCorrupt** (`user/doome32.c` header comment; kernel quarantine array covers dev 0..7 only). | Stage assets on the AHCI disk; disk headroom ~16.8 MB free after Freedoom1 — a second multi-MB IWAD needs a bigger image or swap. |
| Build/staging | Append-only `user/user.mk` DOOM block (lines 1605–1656): `DOOM_CORE` obj list, `ld --emit-relocs --defsym=<name>_printf_slot=... -T user/link.ld`, `elf2e32.py --euser-import 915 --iat-off 0x40000`. | Clone the block with a new prefix; pattern is proven. |
| Gates | `tools/r1_doom_test.py` (boot+markers+ord-915 bind+QMP screendump, SKIP-on-missing-assets), `r1_doom_playtest.py`, `r1_doom_audio_test.py`, `r1_doom_visual_test.py`, `r1_doom_release_test.py`, `r1_doom_multikey_test.py`, `r1_doom_stuck_test.py`. All SKIP cleanly when assets are absent. | Clone `r1_doom_test.py` → `r1_<game>_test.py`; same SKIP discipline. |

**Exec-slot budget:** slow table 0..270 is fully live; the only invalid
in-range slots are 240, 247–251 and belong to audio/netbridge/GUI workers;
271–273 are Windowsrv's (retired) and 274/275 are **unsafe** (would overwrite
`EpocFastExecTable` count — `variant/qemu_fbblit.h` header). So a second game
**must reuse existing execs** (270 ctl, 240 audio, 68 time, 86 debug,
107/108 chunks, 241 ps); no new slow-exec slots are available without risky
table surgery. This is the main architectural constraint.

---

## 2. Candidate games ranked by cost

| Rank | Candidate | Why | Est. new code | Risk |
|---|---|---|---|---|
| 1 (cheapest overall, weakest proof) | **DOOM variant: Freedoom Phase 2 IWAD or a PWAD on existing DOOM.E32** | Zero new code; proves only the asset pipeline, not a new integration. | ~0 (staging only) | Not a genuine "second game"; disk space (16.8 MB free). |
| 2 (cheapest *genuinely different* game) | **Wolfenstein 3D (GPL source release; Wolf4SDL-style freestanding port)** | Raycaster, 320x200 **8-bit palettized** — exactly what fbblit `src_bpp=8` does; smaller engine than DOOM; shareware IWAD is tiny (~4 MB WL1) so it fits the disk; same input/timing/audio bridges. Music/SFX are AdLib/IMF + digitized VOC → needs a small synth/PCM decode (template: `i_e32sound.c` MUS synth). | New platform glue (~600 lines, modeled on `e32_platform.c`) + engine port | No doomgeneric equivalent exists; engine port is real work; audio synth effort. |
| 3 | **Heretic / Hexen (GPL, DOOM-engine lineage)** | Highest bridge reuse — same `I_*` platform interface shape as doomgeneric, so `e32_shim.c`/`e32_platform.c` transfer almost 1:1; Chocolate Heretic/Hexen sources exist. | Engine diff (larger than Wolf3D: new menus, classes, inventory) + new IWAD (shareware available, few MB) | Bigger engine diff; asset size; more time in kernel-heap edge cases. |
| 4 | **Simple third-party 2D game (single-file C, software 320x200, e.g. a 2048/Pac-Man/Tetris-class title with real upstream source)** | Trivially fits fbblit 8bpp + con_tryread; smallest proof surface. | ~200–500 lines glue | Weakest integration proof — hand-written `user/tetris.c`, `user/snake.c` already exist, so a third-party 2D port adds little beyond what DOOM already proved. |

**Recommendation:** rank 2, **Wolfenstein 3D shareware**, as the cheapest
credible second game; rank 1 (second IWAD) only as a zero-cost smoke test of
the asset pipeline beforehand.

---

## 3. Required new work

1. `ports/<game>/` engine tree + thin platform layer reusing `e32_shim.c`,
   `e32_audio.h`, the fbblit exec-270 client, con_tryread input, exec 68 time.
2. Append-only `user/user.mk` block: obj list, `ld --emit-relocs
   --defsym=<game>_printf_slot=0x<free> -T user/link.ld`, `elf2e32.py
   --euser-import 915 --iat-off 0x<free>` (pick IAT off distinct from 0x40000).
3. Asset staging: extend `tools/mkgrubiso.py --wad` (or add `--wad2`) and the
   64 MB disk; if the new IWAD doesn't fit with Freedoom1, grow the image or
   make the WAD set selectable per test.
4. Audio: PCM submit path for SFX; small synth for music if the game has
   sequenced music (copy the `i_e32sound.c` MUS approach).
5. Gate: `tools/r1_<game>_test.py` cloned from `r1_doom_test.py` (marker family,
   ord-915 bind line, QMP screendump, SKIP-on-missing-assets).

**Explicitly NOT needed:** new exec slots, kernel changes outside
`qemu-port/variant`, EUSER import changes (915 suffices), EWSRV changes,
touches to `kernelhwsrv/`.

---

## 4. Gate strategy

Mirror the DOOM gate family, all SKIP-on-missing-assets, exit 0 on SKIP:
`r1_<game>_test.py` (boot GRUB ISO → launch from serial shell → assert
`<GAME>:` marker family + `e32run: bind slot ... ord 915` + no
Exc/PanicKernExec/FAULT + best-effort QMP screendump), then a playtest clone
(long-run, no auto-quit, clean Esc exit) and an audio clone if AC'97 is
attached. Reuse `r1_doom_test.py`'s helpers (check_assets, build_iso,
wait_for_bytes, qmp_screendump, write_keys). Never fail on flaky QMP.

---

## 5. Feasibility verdict

**Feasible, low-risk, cheap.** Every bridge the second game needs is
game-agnostic and already proven end-to-end by DOOM (display via exec 270
fbblit, input via con_tryread make/break, 35 Hz timing via exec 68, AC'97 PCM
via exec 240, genuine EUSER ord-915 markers, RChunk heap). The binding
constraints are (a) no free exec slots — reuse only, (b) assets must live on
the AHCI `C:` volume because device-8 RAMDISK opens are broken (KErrCorrupt),
and (c) ~16.8 MB free disk after Freedoom1 limits IWAD size unless the image
grows. Wolf3D shareware (small IWAD, 8bpp raycaster) fits all constraints.

---

## 6. First 3 concrete steps

1. **Zero-code smoke:** stage Freedoom Phase 2 (or any PWAD) as a second 8.3
   WAD via `tools/mkgrubiso.py` and boot it on the existing DOOM.E32 — proves
   the multi-asset staging path before any engine work.
2. **Scaffold the port:** create `ports/wolf3d/` with the GPL engine + a new
   `e32_platform.c` adapted from `ports/doom/e32_platform.c` (fbblit src_bpp=8
   path, con_tryread input, exec 68 clock), reusing `e32_shim.c` and
   `e32_audio.h` unchanged; add the append-only `user/user.mk` block cloning
   the DOOM rule with its own printf-slot/IAT offsets.
3. **First gate:** write `tools/r1_wolf3d_test.py` as a clone of
   `tools/r1_doom_test.py` (SKIP-on-missing-assets, `WOLF:` marker family,
   ord-915 bind assertion, QMP screendump of the title screen) and get a
   SKIP-clean run before any QEMU boot with assets.
