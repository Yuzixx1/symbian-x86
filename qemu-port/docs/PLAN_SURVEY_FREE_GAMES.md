# PLAN_SURVEY_FREE_GAMES — free-licensed candidates for the "second game" proof

**Status:** SURVEY ONLY. No code written, no commits/pushes, no `make clean`,
no QEMU runs, no web, no downloads. `kernelhwsrv/` untouched (read-only).
Scope: local sources only — `qemu-port/ports/`, `qemu-port/tools/`,
`docs/DOOM_PORTING.md`, `docs/PLAN_SURVEY_NEXT_GAME.md`,
`docs/PLAN_SURVEY_GAME_ASSETS.md`, `/tmp/opencode/`.

Policy baseline (`docs/DOOM_PORTING.md` §4): **free-licensed assets only**; no
id Software / shareware WADs, nothing that would put a non-free asset into
`PROGRAMS.IMG`. Ranking axes, per the brief: (1) license cleanliness,
(2) asset size vs the ~16.8 MB free disk headroom, (3) reuse of the proven
bridges (8bpp fbblit = slow exec 270 op 3 `FBO_FBBLIT` with `src_bpp` 8/32,
`variant/qemu_fbblit.h:35-52`; PS/2 `con_tryread()`; exec 68 µs clock; AC'97
PCM via exec 240, 16-bit stereo 11025–48000 Hz, `ports/doom/e32_audio.h`;
EUSER ord-915 markers; RChunk heap via exec 107/108).

---

## 1. Local inventory (what exists on this machine today)

| Asset | Location | Size | License | Policy-clean? |
|---|---|---|---|---|
| Freedoom 0.13.0 Phase 1 IWAD `freedoom1.wad` | `/tmp/opencode/doom/freedoom-0.13.0/` | 28,795,076 B (~27.5 MiB) | BSD-3-Clause (`COPYING.txt` in same dir) | Yes — already staged as `FDOOM1.WAD` |
| Freedoom 0.13.0 Phase 2 IWAD `freedoom2.wad` | same dir | 28,787,748 B (~27.5 MiB) | BSD-3-Clause | Yes — **but exceeds the 16.8 MB headroom** alongside Phase 1 |
| Freedoom zip + verify copy | `/tmp/opencode/freedoom.zip`, `/tmp/opencode/_verify_fdoom1.wad` | 24.1 MB / 28.8 MB | BSD-3-Clause (same content) | Yes, redundant copies of Phase 1 |
| Mini-Zork I story file `minizork.z3` | `qemu-port/ports/zmachine/minizork.z3` | 52,216 B | Copyrighted Infocom/Activision promotional demo | **No** — `ports/zmachine/README.md` states it is not redistributable; scratch-FAT staging only |
| Engine trees | `qemu-port/ports/{doom,zmachine,bc,lua,miniz}` | — | GPL/BSD/MIT-style code | Code is free; no extra game data vendored |
| ROMs / LBM / XM / MOD / other WADs / PWADs | full-tree search of repo + `/tmp/opencode` | — | — | **None found.** No Wolf3D `*.WL1`/`*.VOC`/`*.IMF`, no console ROMs, no tracker or LBM files anywhere on disk |

Disk context: 64 MB MBR image, 48 MiB FAT32 at LBA 2048
(`tools/mkgrubiso.py:74` `DISK_MB, FS_MB, PART_LBA = 64, 48, 2048`); with
`FDOOM1.WAD` staged among 87 files, **~16.8 MB free**
(`docs/DOOM_PORTING.md:18`, `docs/DOCS_INDEX.md:274`). Assets must live on the
AHCI `C:` volume — device-8 RAMDISK opens fail with `KErrCorrupt`.

## 2. Candidate list (well-known free/open sets — NOT fetched, none verified local)

| # | Candidate | License | Typical asset size | Bridge fit |
|---|---|---|---|---|
| C1 | **FreeDM** — free deathmatch IWAD for the DOOM engine (freedoom project sibling) | BSD-3-Clause | single-digit MB (verify at acquisition) | Perfect: runs on the existing `DOOM.E32` unchanged (`-iwad`), exercises every bridge incl. 8bpp fbblit, PS/2, AC'97 PCM |
| C2 | **Freedoom Phase 2** (`freedoom2.wad`, local) or any BSD/CC0 **PWAD** on the DOOM engine | BSD-3-Clause / CC0 | Phase 2 = 27.5 MiB (over headroom); most PWADs KB–few MB (fit) | Perfect bridge reuse; zero new engine code |
| C3 | **Freely licensed Z-machine v3 story files** (CC0/CC-BY/GPL interactive fiction, e.g. IF-Archive `games/` entries) | CC0 / CC-BY / GPL | tens of KiB | Fits the proven `ports/zmachine` interpreter (v1–v3, 128 KiB image limit); text/console path — does **not** exercise fbblit or AC'97 |
| C4 | **OpenTyrian** (GPL-2.0 engine; Tyrian 2.1 data released by Epic as freeware) | GPL-2.0 code; data freeware-redistributable, **not an OSI free license** | data ~15–20 MB (borderline vs headroom) | Good: 2D 320x240-ish shooter, 8bpp-style art → fbblit, PS/2, PCM SFX — but license is borderline under a strict free-license-only policy |
| C5 | **OpenArena** (Quake III Arena engine GPL-2.0, assets CC-BY-SA) | GPL-2.0 / CC-BY-SA-3.0 | pk3 set is hundreds of MB | Poor: grossly over disk headroom; engine port is a major project, not a bridge reuse |
| C6 | Freely licensed **retro raycaster/shooter data sets** with libre licenses (community Wolf3D-compatible free asset packs, Blake Stone-style libre sets) | varies, must be verified per pack | few MB | Good engine fit (8bpp raycaster = fbblit `src_bpp=8`), but no known pack is vendored or verified; id shareware data is **excluded by policy** |
| C7 | **LBM (Deluxe Paint) image sets + XM/IT tracker modules** under free licenses | CC0/CC-BY/per-module | KiB–low MB | Format-level fit: LBM is chunky 8bpp → fbblit `src_bpp=8` directly; XM needs a small tracker synth on the AC'97 PCM path (template: DOOM MUS synth in `ports/doom/i_e32sound.c`). Supporting formats, not a standalone game |

## 3. Ranking (license cleanliness × size vs 16.8 MB headroom × bridge reuse)

1. **C1 FreeDM (or any BSD/CC0 PWAD) on the existing DOOM.E32** — cleanest license, fits headroom, maximal bridge reuse, zero engine work; proves the multi-asset pipeline, though it shares DOOM's engine (weaker "second game" claim).
2. **C2 Freedoom Phase 2 / free PWAD (local)** — clean license and local today, but 27.5 MiB does not fit beside Phase 1; needs a WAD swap or an `FS_MB` grow. Best as a zero-code staging smoke test.
3. **C3 free Z-code v3 story files** — clean license, tiny, reuses the proven zmachine port; but text-mode only, so it proves less of the graphics/audio bridges.
4. **C6 libre raycaster data packs** — right shape for 8bpp fbblit + PS/2 + AC'97, but no verified libre pack exists locally; acquisition is user-drop-in only.
5. **C4 OpenTyrian** — good technical fit, but the freeware (non-OSI) data license needs an explicit policy decision; size borderline.
6. **C5 OpenArena** — license-clean but far over disk headroom and a huge engine port; not viable as a bridge-reuse proof.
7. **C7 LBM/XM asset formats** — useful as packaging formats for any of the above, not a game by itself.

## 4. Packaging notes

- Assets are **never downloaded by the build and never committed**: they are
  externally supplied into candidate paths and staged 8.3-safe
  (`tools/mkgrubiso.py:81-93` `WAD_CANDIDATES`/`find_wad()` pattern — extend
  with a second-asset list, e.g. `--wad2` or generalize `--wad`; verify with
  `mdir`). Same drop-off pattern in the gate harness (`tools/r1_doom_test.py:52-59`).
- Binary pattern is proven and append-only: clone the `user/user.mk` DOOM block
  (`user/user.mk:1605-1656`) with a new printf-slot defsym and a distinct
  `--iat-off` (DOOM uses 0x40000), `elf2e32.py --euser-import 915`,
  `e32read.py` validation. New games reuse existing execs only — **no free
  slow-exec slots** (240/247-251 live, 271-275 unsafe; `variant/qemu_fbblit.h`).
- Disk: anything beyond ~16.8 MB alongside `FDOOM1.WAD` requires growing
  `FS_MB` (one-number change, `mkgrubiso.py:74`) or swapping the IWAD per test;
  assets must be staged on the AHCI `C:` FAT volume, not the device-8 RAMDISK.
- Gates clone `tools/r1_doom_test.py` with SKIP-on-missing-assets discipline
  (`r1_<game>_test.py`, own marker family + ord-915 bind assertion + QMP
  screendump, exit 0 on SKIP).

## 5. Blocker / decision needed

1. **License-policy decision:** the written policy is free-license-only
   (`docs/DOOM_PORTING.md` §4). That excludes id shareware Wolf3D WL1 (the rank-2
   pick in `docs/PLAN_SURVEY_NEXT_GAME.md`), commercial Z-machine stories, and
   — strictly — OpenTyrian's freeware data and the local `minizork.z3`. User
   decision: keep strict (C1/C2/C3/C6 only) or explicitly admit
   freeware-redistributable data.
2. **Acquisition path:** this project has no download path by design; the only
   in-policy source for any non-local candidate is a user-supplied file dropped
   into a candidate directory. Nothing can be fetched during the port.
3. **Disk headroom:** ~16.8 MB free after Freedoom Phase 1 — a second multi-MB
   IWAD (Phase 2, Tyrian data) forces an image grow or per-test WAD swap.

## 6. First steps

1. Zero-code smoke: stage `freedoom2.wad` as a swapped/alternate 8.3 WAD (or a
   small free PWAD via `-file`) through `mkgrubiso.py` and boot it on the
   existing `DOOM.E32` — proves multi-asset staging before any engine work.
2. Pick the first genuinely-free second asset within headroom (FreeDM or a
   BSD/CC0 PWAD per C1), drop it into a `WAD_CANDIDATES`-style path, extend
   `mkgrubiso.py` staging, verify with `mdir`.
3. Clone `tools/r1_doom_test.py` → `tools/r1_<game>_test.py` (SKIP-on-missing-
   assets, own marker family, ord-915 bind, QMP screendump) and get a SKIP-clean
   run before any QEMU boot with assets.
