# PLAN_FREEDM_MILESTONE — P3-lite: "second playable workload = FreeDM on the existing DOOM.E32"

**Status: DESIGN ONLY.** No code written, no commits/pushes, no `make clean`,
no QEMU runs, no downloads. `kernelhwsrv/` untouched (read-only). Scope: local
sources only — `docs/PLAN_SURVEY_FREE_GAMES.md`, `tools/mkgrubiso.py`,
`tools/r1_doom_test.py`, `user/doome32.c`, `ports/doom/d_main.c`,
`ports/doom/d_iwad.c`, `docs/DOOM_PORTING.md`.

Baseline: `docs/PLAN_SURVEY_FREE_GAMES.md` §3 rank 1 (C1) — FreeDM (BSD-3-Clause,
freedoom-project sibling) on the existing DOOM.E32 is the zero-decision path.
The milestone is deliberately **P3-lite**: staging + gate only. No new game
engine, no new exec slots, no new E32 — the workload switch is an asset swap,
not a binary switch.

---

## 1. Milestone definition

> **Second playable workload = FreeDM (freedm.wad, BSD-3-Clause) running on the
> byte-existing `build/user/DOOM.E32`, proven by a dedicated gate
> `tools/r1_freedm_test.py`, with the asset also staged on the shipped
> `/PROGRAMS.IMG` as `FREEDM.WAD` alongside `FDOOM1.WAD`.**

In scope: asset drop-off + 8.3 staging in `mkgrubiso.py`, gate clone with
SKIP-on-missing-asset discipline, disk-budget proof. Out of scope (deferred to
a true P3): any edit to `user/doome32.c` / `ports/doom/`, any new E32 or
`user/user.mk` block, any exec-slot change, any `FS_MB` grow, any in-guest
`-iwad`/`-file`/`-deathmatch` command-line selection.

Why zero guest code works: `user/doome32.c:27-44` (`kWadPaths[]`) probes
`C:/FDOOM1.WAD` first and passes `-iwad <found> -skill 2 -warp 1 1`
(`doome32.c:88-97`). FreeDM is a standalone IWAD whose first map lump is
`MAP01`, so `D_IdentifyVersion` (`ports/doom/d_main.c:737-744`) resolves
`gamemission = doom2` → `gamemode = commercial`, and the stock `-warp 1 1`
parses as `startmap = atoi("1")` = MAP01 (`d_main.c:1710-1712`). Therefore
**staging the FreeDM bytes under the 8.3 alias `FDOOM1.WAD` makes the
unmodified DOOM.E32 boot FreeDM MAP01 with zero source changes.** The
selection point is disk staging, not the guest.

## 2. Asset path / staging

**(a) Host drop-off candidates** (mirror `WAD_CANDIDATES`,
`tools/mkgrubiso.py:81-89`), in priority order:

```
qemu-port/wads/freedm.wad
qemu-port/wads/FREEDM.WAD
qemu-port/build/wad/freedm.wad
qemu-port/build/wad/FREEDM.WAD
/tmp/opencode/doom/freedoom-0.13.0/freedm.wad
/tmp/opencode/doom/freedm.wad
```

Never committed (like the 28 MB IWAD); license = BSD-3-Clause with
`COPYING.txt` in the same freedoom-0.13.0 dir — same policy class as
`freedoom1.wad` (`docs/DOOM_PORTING.md` §4).

**(b) Guest path + 8.3 naming.** Staged at the **root of the FAT32 partition
at LBA 2048 of `PROGRAMS.IMG`**, which is the AHCI volume: dev 4 part 0,
mounted at **`C:`** when booted via `tools/boot_grub.sh` (extracts
`/PROGRAMS.IMG`, attaches `ich9-ahci`) or via the gate's `--disk-boot`
(`tools/r1_doom_test.py:287-292`). Plain ISO boot exposes the same image only
as the X: RAMDISK (device 8), where large-file opens fail with `KErrCorrupt`
(`user/doome32.c:22-31`) — so **C: is the only valid asset path**, matching
`doome32.c`'s C:-first probe order. 8.3 name: **`FREEDM.WAD`** (6+3, already
8.3-safe, staged uppercase via `mcopy -i fs -o src ::FREEDM.WAD`, exactly like
`WAD_NAME = "FDOOM1.WAD"` at `mkgrubiso.py:80`). Gate-disk alias:
**`FDOOM1.WAD`** (FreeDM bytes under the existing alias — the P3-lite
selection mechanism, see §1).

**(c) `mkgrubiso.py` changes** (all append-only, current behavior unchanged
when the asset is absent):

1. Add `WAD2_NAME = "FREEDM.WAD"`, `WAD2_CANDIDATES = (...)` and `find_wad2()`
   as a clone of `WAD_CANDIDATES`/`find_wad()` (`mkgrubiso.py:80-93`).
2. In `build_programs_image()` (`mkgrubiso.py:163-213`): `wad2 = find_wad2()`;
   if found, **free-space guard** — after staging the curated set, run
   `mdir -i {disk}@@{PART_LBA}S ::`, parse the free-bytes line, and stage
   `(wad2, WAD2_NAME)` only if `os.path.getsize(wad2) <= free`; otherwise warn
   and skip (never fail the ISO build, never grow `FS_MB`).
3. Extend the mdir verification loop (`mkgrubiso.py:201-207`) to also require
   the normalized token `freedmwad`.
4. `curated_programs()` (`mkgrubiso.py:96-145`): extend `iwad_note` to mention
   `FREEDM.WAD` when staged (it already says "optional PWADs go in
   build/wad/ as *.WAD", `mkgrubiso.py:130-133`).
5. Geometry untouched: `DISK_MB, FS_MB, PART_LBA = 64, 48, 2048`
   (`mkgrubiso.py:74`) stays; the ISO grows by the WAD2 size only.

## 3. Gate design — `tools/r1_freedm_test.py`

Clone of `tools/r1_doom_test.py` with the survey's SKIP discipline
(`PLAN_SURVEY_FREE_GAMES.md` §4: "clone `r1_doom_test.py` … exit 0 on SKIP"):

- **Paths**: `DISK=/tmp/opencode/r1_freedm.img`, `LOG=…/r1_freedm.log`,
  `PNG=/tmp/opencode/freedm_final.png`, `ISO=…/r1_freedm.iso`, own QMP socket;
  same `DISK_MB=64 / PART_LBA=2048 / PART_SECTORS=98304` constants, kept in
  sync with `mkgrubiso.py` as today.
- **Assets**: `E32_CANDIDATES` unchanged (`build/user/DOOM.E32`);
  `FREEDM_CANDIDATES` = the §2(a) list. `check_assets()` returns None →
  prints `[SKIP] …` + `RESULT: SKIP`, **exit 0** when DOOM.E32 or freedm.wad
  is missing (mirrors `r1_doom_test.py:158-176`). EUSER.DLL optional with WARN.
- **Disk staging**: `build_disk()` stages `(freedm, "FDOOM1.WAD")` — FreeDM
  under the existing 8.3 alias — plus `DOOM.E32` + `EUSER.DLL`; mdir check for
  `doome32` + `fdoom1wad` (same as `r1_doom_test.py:185-213`).
- **Markers/assertions**: reuse the `DOOM:` family (`r1_doom_test.py:75-79`)
  since the guest can only print ord-915 `DOOM:` lines, but pin the
  **FreeDM identity by exact byte count**: `FREEDM_SIZE = "<exact bytes>"`
  recorded at drop-off, asserted via the `DOOM: WAD C:/FDOOM1.WAD <FREEDM_SIZE>`
  line (clone of the `WAD_SIZE = "28795076"` check, `r1_doom_test.py:82`,
  `:318-321`) — a swapped-in Freedoom cannot pass. Plus boot markers, EUSER
  ord-915 bind, advancing `DOOM: frame N`, no Exc/Panic/FAULT, best-effort QMP
  screendump. Launch is still the bare shell command `DOOM` + `GAME_KEYS`;
  harness prints its own `FREEDM:` PASS/FAIL family.
- **Boot**: same dual mode (GRUB ISO default, `--disk-boot` AHCI), 75 s cap,
  `-vga std`, QMP screendump to `freedm_final.png`.
- First run must be **SKIP-clean with the asset absent** before any QEMU boot
  with assets (`PLAN_SURVEY_FREE_GAMES.md` §6.3).

## 4. Disk budget

- Geometry: 64 MB MBR disk, FAT32 partition at LBA 2048, `FS_MB=48` →
  98304 sectors = 50,331,648 B; FAT32 overhead leaves **~47 MiB usable**
  (`docs/DOOM_PORTING.md:18`).
- Current payload: 87 files ≈ 30.5 MiB (≈2.6 MB E32s + EUSER.DLL +
  `FDOOM1.WAD` 28,795,076 B), leaving **~16.8 MB free**
  (`docs/DOOM_PORTING.md:18`, `docs/DOCS_INDEX.md:274`).
- FreeDM (freedm-0.13.0.wad): expected single-digit MB per
  `PLAN_SURVEY_FREE_GAMES.md` §2-C1 — **verify exact bytes at drop-off** and
  pin them as `FREEDM_SIZE`. Rule: it coexists with `FDOOM1.WAD` iff
  `size(freedm.wad) ≤ ~16.8 MB`; then free after staging ≈ 16.8 MB − size.
  If it ever exceeds headroom, P3-lite does **not** grow `FS_MB` — the gate's
  per-test disk swap (FreeDM as `FDOOM1.WAD`) still works because the 48 MiB
  partition easily holds one WAD + DOOM.E32 (~338 KiB) + EUSER.DLL.
- ISO size: grows by size(freedm.wad) from the current 75,853,824 B; no
  documented ISO budget, so acceptable.

## 5. Difference from a true second game

| Axis | This P3-lite milestone | A true second game (e.g. C4/C6 engine) |
|---|---|---|
| Engine | same doomgeneric core inside the same DOOM.E32 | new port under `ports/`, new `user/user.mk` block |
| Executable | one E32 (`DOOM.E32`), byte-existing | second E32 with its own printf-slot defsym + `--iat-off` |
| Exec slots | unchanged (fbblit 270 op 3, AC'97 240, clock 68, RChunk 107/108) | may need new slow-exec slots (none free: 240/247-251 live, 271-275 unsafe) |
| Selection | staging-time WAD swap (`FDOOM1.WAD` alias / second asset `FREEDM.WAD`) | in-guest `-iwad`/`-file`/`-deathmatch` argv — would require a `doome32.c` argv edit (still one E32, but not byte-existing) and userland command-line access, which `user/start.S` does not pass to `app_main` today |
| Disk | asset-only delta within 16.8 MB headroom | engine + assets, likely `FS_MB` grow |
| Proof value | multi-asset staging + gate discipline on one volume; every graphics/input/audio bridge exercised | proves executable-catalog growth and per-game launch |

Known limitation to flag: without `-deathmatch`, FreeDM MAP01 plays as a
normal (possibly monster-light) map; frames still render and the gate's
`DOOM: FRAME` assertions still prove the workload. Deathmatch mode is a
deferred P3 item.

## 6. First 3 steps

1. **Acquire + pin**: drop `freedm.wad` (with its `COPYING.txt`) into
   `qemu-port/wads/` or `/tmp/opencode/doom/freedoom-0.13.0/`; record the exact
   byte count as `FREEDM_SIZE` in the gate. No download by the build —
   user-supplied file only (`PLAN_SURVEY_FREE_GAMES.md` §5.2).
2. **Stage**: apply the §2(c) `mkgrubiso.py` WAD2 changes; run `make isogrub`
   (no `make clean`); verify `mdir -i build/programs.img@@1048576 ::` lists
   `FREEDM.WAD` and reports the expected remaining free space.
3. **Gate**: clone `tools/r1_doom_test.py` → `tools/r1_freedm_test.py` per §3;
   first run it **without** the asset to confirm `RESULT: SKIP` exit 0, then
   with the asset for a PASS (QMP screendump to `/tmp/opencode/freedm_final.png`).
