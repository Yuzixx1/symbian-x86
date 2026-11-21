# DOOM on EKA2 — WAD staging and larger FAT disk design

**Status: DESIGN + TEST SCAFFOLD ONLY.** No source files were changed by this
document's author. Every file that must change is named with a path and line in
§12 so another agent can implement it. `kernelhwsrv/` is frozen/read-only;
everything needed here is in `qemu-port/` (`variant/`, `user/`, `tools/`,
`Makefile`, `user/user.mk`).

> **Integration update (2026-11-21):** the FAT-disk and WAD-staging pieces below
> have now landed in `tools/mkgrubiso.py` and `tools/r1_doom_test.py`, using
> **larger geometry than this design assumed**. The real Freedoom1.wad is
> 28,795,076 B (~27.5 MiB), not ~10 MB, so the embedded FAT disk is now
> **64 MB with a 48 MB FAT32 partition at LBA 2048 (98304 sectors)** (~47 MiB
> usable). The WAD is staged at build time from `/tmp/opencode/doom/...` (never
> committed into the repo) under the 8.3 name `FDOOM1.WAD`. `make isogrub`
> succeeds: `build/programs.img` is 64 MB, `build/symbian-grub.iso` is
> 75,853,824 B (~72 MiB), and `mdir -i build/programs.img@@1048576 ::` shows
> all 87 files including `FDOOM1.WAD` (28,795,076 B) with ~16.8 MB free. The
> kernel-side blockers in §6–§10 remain.

---

## 1. Goal, and what "real DOOM" means

The goal is to run **genuine DOOM through `doomgeneric`** as a ring-3 E32
process on the ported EKA2 kernel — not a fake demo that prints the DOOM logo or
draws a static title screen. "Real" means all of the following work through
genuine kernel facilities:

| Subsystem | Requirement |
|---|---|
| Assets | Freedoom IWAD (and optional PWADs) read from a real FAT volume |
| Heap | DOOM zone allocator backed by expanded kernel chunks, not a 64 KiB toy heap |
| Video | 8-bit 320x200 palettized frames converted to RGBA and blitted to the VBE LFB |
| Input | PS/2 key events delivered to `doomgeneric`'s `I_GetEvent()` |
| Timing | 35 Hz game tic clock driven by kernel timers/execs |
| Audio | AC97 PCM-OUT for sound effects; music initially a documented limitation |
| Diagnostics | `DOOM: ...` serial markers plus a QMP screendump of the title screen |

The proof is `tools/r1_doom_test.py` (§11): boot the GRUB ISO, launch `DOOM`,
send keys, assert markers and inspect `/tmp/opencode/doom.png`.

---

## 2. Architecture — end-to-end data path

```
   build/PROGRAMS.IMG  (32 MB MBR disk, FAT32 partition LBA 2048, 49152 sectors)
   +-- FDOOM1.WAD      freedoom1.wad (~10 MB, free licence, 8.3 guest-safe name)
   +-- DOOM.E32        doomgeneric + shim, PE2 E32, EUSER ord 915 import
   +-- *.WAD           optional PWADs (same 8.3 rules)
   +-- EUSER.DLL, CALC.EXE, ... (existing curated set)
        |
        | pycdlib: /PROGRAMS.IMG;1 inside build/symbian-grub.iso
        v
   GRUB 2 (El Torito, serial console) -- module /programs.img
        |
        | Multiboot1 handoff (EAX=0x2BADB002, EBX=MBI, mods_count/addr)
        v
   BootInfo::Capture() -> QemuRamdiskInit() -> block device 8 (read-only)
        |
        | FatMountAlias(8, 0, 'X')  [also dev 4 = AHCI port 0 part 0 at boot]
        v
   X:\FDOOM1.WAD  --(runtime.h file_open / file_read, streaming exec)-->
        |
        v
   +--------------------------------------------------------------+
   | DOOM.E32 (ring 3)                                            |
   |   WAD loader -> lump cache (expanded RChunk heap)             |
   |   doomgeneric: I_Init/I_GetEvent/I_GetTime/I_FinishUpdate      |
   |   libc shim: printf->915, malloc->chunks, memcpy/memset local |
   |   8-bit 320x200 index frame + 256-entry palette               |
   +-----------------------+--------------------------------------+
                           | exec 251 display list: GC_BLIT ("fbblit")
                           | source index buffer + palette in low RAM
                           v
   kernel qemu_gui_exec.cpp (ring 0): palette[256] -> 0x00RRGGBB
                           -> Bochs VBE LFB 1024x768x32, pitch 4096
                           v
   QEMU -vga std visible framebuffer  --QMP screendump-->  /tmp/opencode/doom.png

   PS/2 keyboard -> IRQ1 -> console/PortCon -> con_tryread() -> I_GetEvent()
   exec timers (133/52/53) + sleep_ms() -> I_GetTime() 35 Hz
   exec 240 audio_tone() / new PCM exec -> AC97 PCM-OUT (optional in v1)
```

Two boot paths are supported. The **ISO path** (preferred) needs no `-drive`:
GRUB loads `PROGRAMS.IMG` as a Multiboot module, the kernel snapshots it to
high RAM and mounts it as `X:` (`X: mounted (programs from GRUB module)`; see
`docs/GRUB_BOOT_INTERNALS.md` §4). The **disk path** (`r1_calc_test.py` style)
attaches the same image as an AHCI disk at **dev 4, partition 0**, mounted at
boot. The test scaffold uses the ISO path but stages the same staging rules.

---

## 3. Disk layout: the 8 MB / 16 MB problem

Today the sizes are inconsistent and both too small:

| Where | Constant | Value today | Partition size |
|---|---|---|---|
| `tools/mkgrubiso.py:68` | `DISK_MB, FS_MB, PART_LBA = 32, 16, 2048` | 16 MB FS | 32768 sectors — *now `64, 48, 2048` / 98304 sectors, see update above* |
| `tools/r1_calc_test.py:27` (and ~80 sibling `r1_*` tests) | `MB, LBA, SECTORS = 32, 2048, 16384` | 16384 sectors | **8 MB** |
| `tools/make_fat_disk.py:23`, `tools/make_disk.py:23` | MBR size bytes `\x00\x40\x00\x00` | 16384 sectors | 8 MB |

The acquired `freedoom1.wad` is 28,795,076 B (~27.5 MiB), not the ~10 MB
originally assumed, so it fits neither volume. With DOOM.E32, EUSER.DLL and the
existing E32s the staged set is ~30.5 MiB; the implemented layout is a
**48 MB FAT32 partition at the same LBA 2048** on a 64 MB disk (see update).

**Required new layout (keep the 32 MB disk):**

```
LBA 0       MBR (one 0x0C FAT32 LBA entry, bootable)
LBA 1..2047 gap/reserved (unchanged)
LBA 2048    FAT32 volume start = 49152 sectors = 24 MiB (25,165,824 bytes)
LBA 51200   end of partition (still < 65536 total disk sectors)
```

**Exact constants for other agents to change** (this author did not touch them):

- `tools/mkgrubiso.py:68` — implemented as
  `DISK_MB, FS_MB, PART_LBA = 64, 48, 2048` (not the 32/24 originally planned).
  Lines deriving `fs_secs` and the MBR entry from `FS_MB` update automatically;
  the `mdir` verification passes and the WAD is staged via `WAD_CANDIDATES`.
- `tools/r1_calc_test.py:27` — `MB, LBA, SECTORS = 32, 2048, 49152`.
  The same `16384` literal appears in ~80 test harnesses; grep before editing:
  `grep -rlE '16384|PART_SECTORS' tools --include='*.py'`. Enlarge only the
  harnesses that stage large data (DOOM, and any future WAD/asset tests); the
  small E32-only tests can stay 8 MB.
- `tools/make_fat_disk.py:23` and `tools/make_disk.py:23` — MBR partition size
  `\x00\x40\x00\x00` (16384) becomes `\x00\xc0\x00\x00` (49152).
- `tools/mkfat.py:23-31` needs no change: `base = 2048` and
  `vsects = total - base` are derived.
- `docs/GRUB_BOOT_INTERNALS.md:25-29` describes the old 16 MB image; update the
  prose when the code lands. `build/PROGRAMS.IMG` must be rebuilt and the ISO
  re-mastered with `python3 tools/mkgrubiso.py build/symbian-grub.iso` — a stale
  ISO will still carry the old 16 MB volume.

Why not simply enlarge the disk instead? The kernel's ramdisk snapshot copies
the whole GRUB module into high RAM and the shell exec buffer is fixed; keeping
the disk at 32 MB and only growing the partition preserves all existing
geometry assumptions and keeps the multiboot module under the RAM budget.

---

## 4. WAD licensing and asset policy

- **Use Freedoom** (`freedoom1.wad`, ~10 MB, BSD-3-Clause / free redistribution)
  as the IWAD. It is a complete, legal replacement for `DOOM.WAD` / `DOOM1.WAD`.
  It must be credited in `README.TXT`/`NOTES.TXT` on the FAT image and in this
  document.
- **Do NOT ship `DOOM.WAD`, `DOOM1.WAD` or any id Software / shareware WAD.**
  They are not redistributable; embedding one in `PROGRAMS.IMG` would put a
  copyrighted asset in every built ISO.
- **PWADs** (custom levels, dehacked patches) are optional and follow the same
  rule: free-licensed only, staged as separate `*.WAD` files, passed to DOOM via
  `-file` on the E32 command line.
- Another agent is acquiring the Freedoom file; this author owns only the
  design doc and the scaffold test, not the binary.

---

## 5. Staging the WAD on FAT

`tools/mkgrubiso.py:71-121` `curated_programs()` currently returns only
`build/user/*.E32` ≥ 30 KB (`E32_MIN_SIZE`, line 67), `EUSER.DLL` and two
generated text files. It must gain a WAD list:

1. Look for `build/wad/freedoom1.wad` (agent-supplied) and optional
   `build/wad/*.wad` PWADs.
2. Stage the IWAD under an **8.3 name `FDOOM1.WAD`**. `FREEDOOM1.WAD` is nine
   characters in its basename and would rely on mtools LFN aliases; the guest
   FAT driver resolves those (confirmed for `APPLAUNCH.E32`), but the short name
   removes a variable from a first port. PWADs get short names too (`MAP01.WAD`).
3. Use the existing `mcopy -i <fs> -o <src> ::<NAME>` loop at lines 143-144;
   extend the `mdir` verification at 158-164 to require `fdoom1wad`.
4. Keep DOOM.E32 ≥ 30 KB so the existing curator picks it up automatically;
   otherwise add it explicitly alongside `CALC.EXE` at lines 96-98.
5. Update the generated `NOTES.TXT` at 101-115 to name `FDOOM1.WAD`, the
   licence and the launch command (`DOOM` / `DOOM -file MAP01.WAD`).

For standalone host tests (no ISO), the same files are copied with `mcopy`
onto `/tmp/opencode/r1_doom.img@@2048S` by `tools/r1_doom_test.py` (§11).

---

## 6. File I/O from the E32 — the real blocker

`runtime.h:11-17` offers `file_open`, `file_read`, `file_close`, but the current
`variant/userfiles.cpp` DPortFile channel is a **64 KiB whole-file snapshot**:

- `file_open` copies the entire file into a fresh `Kern::Alloc(65536)` buffer;
  `FatRead` rejects files larger than that with `KErrTooBig` (see
  `docs/fat_paths_R1.md` §2, `userfiles.cpp:238-280`, `fat32.cpp:1057-1058`).
- `file_read` serves at most **256 bytes per call** from that RAM buffer
  (`runtime.c:205-210`) and there is no seek verb.

A 10 MB WAD is therefore **unopenable today**. This is the single largest
missing piece. Required kernel-side work (owned by a kernel agent, not this
author):

1. Add a streaming read path to `variant/userfiles.cpp` / `runtime.c`: e.g.
   `file_open_stream(path)`, `file_read_at(handle, buf, len, offset)` backed by
   a new PortFile request that calls `FatReadPath` with an explicit offset and
   the caller's length, with no 64 KiB snapshot allocation. Keep the old verbs
   byte-for-byte compatible for existing tests.
2. Alternative with less ABI churn: a dedicated FAT read exec that fills a
   user chunk (`chunk_create`) in 4 KiB slices. Either way DOOM needs random
   access — lump headers and compressed lumps are read out of order — so a
   seek/offset is mandatory.
3. Keep the `X:` volume read-only (dev 8); DOOM must never write the WAD.
4. Path limit is 127 chars and 8.3 is safest for new writes; `X:\FDOOM1.WAD`
   is 15 chars, well within the limit.

In the E32 the WAD layer calls `file_open("X:\\FDOOM1.WAD")`, reads the 12-byte
IWAD header (`IWAD` magic, lump count, directory offset), then reads the
directory and each lump with `file_read_at` into the expanded heap (§7). Until
the streaming API exists, the scaffold test will SKIP rather than claim a pass.

---

## 7. libc shim and heap strategy

`docs/COMPLEX_PROGRAMS.md` §1 is the baseline: freestanding 32-bit PE2 E32,
no hosted libc, no FPU, `int app_main(void)`.

- **printf / markers** — declare `<name>_printf_slot` at `0x408000` and link
  with `elf2e32.py --euser-import 915` so `RDebug::Printf` is the genuine
  serial path; fall back to `print()` (DebugPrint exec 86) when unbound. The
  shim's `printf()` should implement the small format subset DOOM uses (`%s`,
  `%d`, `%x`, `%c`, `%u`) locally rather than trusting libc. Marker prefix:
  `DOOM:`.
- **malloc / zone memory** — `user/malloc.c` is a first-fit chunk allocator
  with `INITIAL_CHUNK 16 KiB`, `GROW_STEP 16 KiB`, `MAX_CHUNK 65536`. DOOM needs
  megabytes. Two options, both requiring kernel support to grow chunks:
  1. Raise the cap in a DOOM-specific heap (`user/doomheap.c`) and call
     `chunk_create(0, 1 MiB, 8 MiB)` up front; back DOOM's `Z_Malloc` zones
     with a bump allocator over that single chunk. This is preferred: DOOM's
     zone allocator already handles its own frees/purges.
  2. Keep `user/malloc.c` but raise `MAX_CHUNK`; verify the kernel's
     `EExecChunkCreate` / page allocator can satisfy multi-megabyte chunks
     before relying on it.
  Static screen buffers (320x200 and the conversion target) should be explicit
  globals or a dedicated chunk, never the small generic heap.
- **string/memory helpers** — implement `memcpy`, `memset`, `memmove`, `strcmp`,
  `strncmp`, `strlen`, `strcpy`, `strcat` locally (`user/doom_shim.c`); no
  `string.h`. `lprintf`/`I_Error` route to the ordinal-915 marker helper.
- **math** — DOOM's renderer is fixed-point; keep it that way. No `libm`, no
  `float` (the port has no FPU exception support). Replace any hosted `rand()`
  with `random32()` from `runtime.h:35`.
- **build** — add `build/user/doom.elf` → `build/user/DOOM.E32` in
  `user/user.mk` per `docs/COMPLEX_PROGRAMS.md` §3 step 4. The shell exec buffer
  is 64 KiB (`variant/kern_shell.cpp:984` `static TUint8 eBuf[65536]`), so
  DOOM.E32 must stay below that; check `e32read.py` output at build time.

---

## 8. Video: palettized DOOM to the 32 bpp LFB

`doomgeneric` produces a **320x200, 8-bit palettized** index frame and a 256-colour
palette loaded from the WAD's `PLAYPAL` lump (768 RGB bytes). Our display is
**1024x768x32 RGBX**: LFB near `0xFD000000`, pitch 4096, pixel word
`0x00RRGGBB` (`docs/GUI_PROGRAMMING.md` §1). Ring 3 cannot touch the LFB
(U/S=0 → MMU #PF), and `GUI_MAX_CMDS` is only 160, so a 64,000-pixel frame
cannot be expressed as `GC_RECT` commands.

**Design — a kernel "fbblit" as a new exec-251 opcode (recommended):**

- There is **no free slow-exec slot**. 240 audio, 241 ps, 247/248 netbridge,
  249/250/251 display, 268-270 SvcMgr, 271-273 window server
  (`variant/qemu_windowsrv.h:11-23`); 242-246 and 252-267 are real EUSER stubs.
  Do not append a new exec number — extend the existing `GC_*` opcode family.
- Add `GC_BLIT` (opcode 3) to `user/guishost.h` and to the switch in
  `variant/qemu_gui_exec.cpp:130-147`:
  `a` = physical address of a 320x200 index buffer in low RAM (identity-mapped,
  U/S=1), `b` = physical address of a 256-entry `0x00RRGGBB` palette,
  `c/d` = destination x/y, `e` = scale (2 → 640x400, 3 → 960x600) and flags.
- Add a small query form (a reserved `arg2` on exec 251 `arg1 == 0`) returning
  the address of a static low-RAM staging region for the frame (64 KiB) and
  palette (1 KiB). Suggested region: `0x00C00000`, above the window-server's
  `0x00800000..0x00BE8FFF` and below kernel data at `0x1000000`. The kernel
  copies the app's frame/palette into that region with a bounded `memcpy` from
  user memory, then converts in ring 0: `dst[y*pitch/4 + x] = palette[src[i]]`.
- The kernel applies clipping, the scale factor and the usual mouse crosshair
  overlay, and keeps exec 251's first-frame pixel readback diagnostic.
- The app's `I_FinishUpdate()` fills the index buffer, submits one `GC_BLIT`
  display list and returns; no user-space RGBA buffer is needed.

**Alternative (rejected for v1):** convert in the app to a 320x200x4 RGBA
buffer (256 KiB) and still issue a kernel blit for the LFB write. It doubles
memory traffic and still needs a new blit opcode, so it has no advantage.

The screendump must show real content: the Freedoom title screen and, once a
level starts, the status bar. A plain blue desktop means the blit did not run.

---

## 9. Input and timing

**Input.** `runtime.h:73-77` provides `con_acquire()`, `con_tryread()` (a
character or negative when empty), `read_line` and `con_puts_raw`; these are the
PortCon lease over the PS/2 keyboard. `doomgeneric`'s `I_GetEvent()` should poll
`con_tryread()` each tic and map ASCII keys to DOOM actions: `w/a/s/d` move,
`<SPACE>` fire, `<ENTER>` use, `Escape` menu, `1..7` weapon select. Current
GUI input drops PS/2 E0 extended scancodes (arrows — see
`docs/GRAPHICAL_DESKTOP.md`), so arrow-key mapping needs a kernel change; WASD
works today. Release `con_acquire()` on exit.

**Timing.** DOOM runs at 35 tics/second. Available primitives: `sleep_us`,
`sleep_ms`, `thread_yield` (`runtime.h:30-33`) and kernel timer execs
`QE_EXEC_TIMER_CREATE 133`, `QE_EXEC_TIMER_AFTER 53`, `QE_EXEC_TIMER_CANCEL 52`
(lines 42-45). `I_GetTime()` should return a monotonic 35 Hz counter backed by
a periodic exec timer (or, for the first cut, a calibrated loop of
`sleep_ms(28)` plus `time_now()` for wall-clock sanity). Avoid `time_now()`
alone: it is whole seconds since 2000. Do not busy-wait for a whole frame —
`thread_yield()` between polls keeps the cooperative scheduler responsive.

---

## 10. Audio

Today: `variant/qemu_audio.cpp` + `qemu_audio_exec.cpp` expose `audio_tone(freq,
ms)` as slow exec **240** — 20 Hz..20 kHz, 1..500 ms, 16.16 phase accumulation
into a 48 kHz stereo AC97 PCM-OUT buffer via a 2-entry BDL, polled completion
(`docs/GUI_AUDIO.md` §6). QEMU must be started with
`-audiodev none,id=a0 -device AC97,audiodev=a0`.

For **real DOOM sound**, additional work is needed:

1. A PCM playback exec, e.g. `QE_EXEC_AUDIO_PCM(buffer, frames, loop)`,
   accepting 8-bit unsigned DOOM sample data from WAD lumps, with a small
   buffer queue and a non-blocking "did the last buffer finish?" query. DOOM's
   `I_StartSound` maps sound lumps to channels; mixer/channel limiting can be
   simple (first 8 channels win).
2. `I_StartMusic` needs a MUS parser and an OPL/AdLib emulator or pre-rendered
   PCM instruments. There is no OPL hardware present; a full music synth is a
   substantial sub-project.
3. **Documented v1 limitation:** music is silent (or a few `audio_tone` beeps
   for menu/title feedback) and there is no volume control, capture or IRQ-driven
   completion. Do not claim "DOOM audio" until PCM sample playback is proven.

---

## 11. Testing plan — `tools/r1_doom_test.py`

This author created only the scaffold (150+ lines). It will **SKIP cleanly** if
`build/user/DOOM.E32` or the WAD is missing, so it is safe to run now.

Flow once the binaries exist:

1. Check tools (`qemu-system-i386`, `mkfs.fat`, `mcopy`, `mdir`, `python3`).
2. Build a 32 MB disk with a **24 MB** FAT32 partition at LBA 2048 (49152
   sectors), stage `DOOM.E32`, `FDOOM1.WAD` (8.3 alias), optional PWADs and
   `EUSER.DLL` with `mcopy`; verify with `mdir`.
3. Build/reuse the GRUB ISO (or boot `-kernel` + `-drive` on the enlarged disk
   if ISO rebuild is unavailable); boot with `-vga std -serial stdio` and QMP
   enabled, feeding `DOOM` at the `EKA2>` prompt.
4. Feed gameplay keys (`w`, space, Enter, Escape) after the title marker.
5. Assert serial markers: `DOOM: INIT`, `DOOM: WAD OK`, `DOOM: TITLE`,
   `DOOM: FRAME`, `DOOM: KEY`, `DOOM: DONE`, plus EUSER ord-915 bind and no
   `Exc `/`PanicKernExec`/`FAULT`/`MMU #PF`.
6. Take a QMP `screendump` to `/tmp/opencode/doom.png`; report it as a visual
   proof artifact (title screen / status bar) and as INFO if QMP is unavailable.
7. Exit 0 only on full pass; exit 0 with `RESULT: SKIP` while assets are absent.

Marker strings are the contract the DOOM E32 must emit; keep them short and
stable. The test never asserts a fake: no marker means FAIL, not a best guess.

---

## 12. Honest status and handoff

### Status table

| Item | State | Evidence / note |
|---|---|---|
| Disk layout (48 MB @ LBA 2048, 64 MB disk) | **Implemented** | `mkgrubiso.py` `64,48,2048`; `mdir` shows ~16.8 MB free |
| Freedoom acquisition | **Done** | `freedoom1.wad` 28,795,076 B at `/tmp/opencode/doom/freedoom-0.13.0/` |
| WAD staging in `mkgrubiso.py` | **Implemented** | `WAD_CANDIDATES` + `find_wad()`; staged as `FDOOM1.WAD`, warned+skipped if absent |
| Streaming WAD reads from E32 | **Missing / blocker** | PortFile 64 KiB snapshot + 256 B reads (§6) |
| `doomgeneric` E32 + libc shim | **Missing** | build rule and heap expansion needed (§7) |
| Palette→RGBA fbblit exec | **Designed, not implemented** | new `GC_BLIT` opcode, §8 |
| PS/2 input mapping | **Partial** | `con_tryread()` works; E0 arrows dropped |
| 35 Hz timing | **Partial** | execs exist, DOOM clock not built |
| AC97 tone exec | **Proven** | exec 240, `r1_qemu_audio_test.py` |
| Real DOOM PCM/music | **Missing** | documented limitation (§10) |
| `r1_doom_test.py` | **Scaffold (SKIP), updated** | new 64/48 constants; WAD drop-off includes `/tmp/opencode/doom/`; still skips without `DOOM.E32` |

### Files that need changes (owner: integration/kernel agents)

| File:line | Change |
|---|---|
| ~~`tools/mkgrubiso.py:68`~~ | **DONE:** `DISK_MB,FS_MB,PART_LBA = 64,48,2048` |
| ~~`tools/mkgrubiso.py` WAD staging~~ | **DONE:** `FDOOM1.WAD` via `WAD_CANDIDATES`, mdir check extended |
| ~~`tools/r1_doom_test.py` constants~~ | **DONE:** `64 MB / 98304 sectors`, WAD drop-off points updated |
| `tools/make_fat_disk.py:23`, `tools/make_disk.py:23` | MBR size `0x4000` → `0xC000` |
| `variant/userfiles.cpp`, `user/runtime.c` | streaming/offset file read (§6) |
| `user/user.mk`, new `user/doom.c`/`doom_shim.c`/`doomheap.c` | build DOOM.E32, enlarged heap, shim |
| `user/guishost.h`, `variant/qemu_gui_exec.cpp:130-147` | `GC_BLIT` opcode + staging region (§8) |
| `variant/qemu_audio*.cpp`, new audio exec | PCM sample playback (§10, optional) |
| `docs/GRUB_BOOT_INTERNALS.md:25-29` | update 16 MB → 48 MB description |

`kernelhwsrv/` stays untouched; `make clean` was not run; no commits or pushes
were made. Re-check §12 after each integration step — this document is a plan,
not a claim that DOOM runs today.

## 13. Integration blocker found by the DOOM E32 port (2026-11-21)

`build/user/DOOM.E32` now builds and validates (338,280 B, PE2 import of
EUSER ord 915), but neither kernel launch path can load it:

* `variant/kern_shell.cpp:984` — `static TUint8 eBuf[65536]`
* `variant/e32load.cpp:1120` — `static TUint8 sSvcImg[256 * 1024]`

`FatReadPath()` returns `KErrTooBig` above the caller's buffer, so the shell
prints `DOOM: not found` and a 338 KiB image never reaches `E32Run()`.  Both
buffers need to grow to at least **384 KiB** (1 MiB recommended) for the
genuine engine image.  This is the only remaining kernel-side change required
to boot DOOM; the streaming WAD reads, fbblit exec and GUI blit are in place.
