# PLAN_SURVEY_GAME_ASSETS — Wolf3D asset sourcing, disk budget, packaging

**Status:** SURVEY ONLY. No code written, no commits, no `make clean`, no QEMU
runs, no web/downloads. `kernelhwsrv/` untouched (read-only). Scope: local
disk only — `qemu-port/ports/`, `qemu-port/tools/`, `qemu-port/user/user.mk`,
`docs/PLAN_SURVEY_NEXT_GAME.md`, `docs/DOOM_PORTING.md`, `/tmp/opencode/doom/`.

Question: can Wolfenstein 3D assets be obtained **locally**, and what would
packaging look like?

---

## 1. Local assets

- **DOOM assets exist locally:** Freedoom 0.13.0 (BSD-3-Clause) at
  `/tmp/opencode/doom/freedoom-0.13.0/` — `freedoom1.wad` (28,795,076 B) and
  `freedoom2.wad` (28,787,748 B), plus COPYING/CREDITS. These are the only
  game assets on the machine; they are build-time assets, never committed to
  the git repo (`tools/mkgrubiso.py:76-87`).
- **No Wolf3D assets anywhere on disk.** Full-tree searches for
  `*wolf*`, `wl1*`, `*.voc`, `*.imf`, WAD-like files found nothing outside
  Freedoom. No `ports/wolf3d/` tree exists; `wolf` appears only in
  `docs/PLAN_SURVEY_NEXT_GAME.md`.
- **No Wolf3D engine source either** — the GPL source release would also be
  an external acquisition, and it ships **no game data** (id's graphics,
  sounds, maps are not in the GPL release).

## 2. Sourcing options

| Option | Licence | Fits project policy? | Local today? |
|---|---|---|---|
| Wolf3D **shareware episode 1** (8 `*.WL1` files: VGAGRAPH/VGADICT/VGAHEAD/MAPHEAD/GAMEMAPS/AUDIOHED/AUDIOT/VSWAP, ~4 MB total, estimate from known distributions — not locally verified) | id Software shareware: freely redistributable but **not** free-licensed | **No** — `docs/DOOM_PORTING.md:153-155` explicitly says do NOT ship any id/shareware WAD; free-licensed only | No |
| GPL engine source (Wolf4SDL-style) | GPL-2.0+ code, **assets not included** | Code yes, data no | No |
| Free/libre Wolf3D-compatible asset set (community-made, e.g. freedoom-style replacements) | varies; none vendored here | Only if free-licensed | No |
| User-supplied files dropped into a candidate path | user's responsibility | Matches the existing pattern: the Freedoom WAD is "agent-supplied" into `/tmp/opencode/doom/...` or `qemu-port/wads/`, never downloaded by the build | **This is the only viable path** |

Project download posture: surveys are "no web, no downloads"
(`docs/PLAN_SURVEY_SOURCE_IMPORT.md:3`); the build tooling has no network
fetch at all — assets are always externally supplied into
`WAD_CANDIDATES`-style paths (`tools/mkgrubiso.py:81-87`,
`tools/r1_doom_test.py:52-59`).

## 3. Size/disk budget

- Guest disk: 64 MB MBR image, 48 MiB FAT32 at LBA 2048
  (`tools/mkgrubiso.py:74`: `DISK_MB, FS_MB, PART_LBA = 64, 48, 2048`).
- Current occupancy: 87 files incl. `FDOOM1.WAD` (28,795,076 B) → **~16.8 MB
  free** (`docs/DOOM_PORTING.md:18`, `docs/DOCS_INDEX.md:274`).
- Wolf3D shareware WL1 ≈ 4 MB → **fits** in the 16.8 MB headroom with ~12 MB
  spare; no disk resize needed for Wolf3D alone.
- Caveat: the rank-1 zero-code smoke test from PLAN_SURVEY_NEXT_GAME (stage
  `freedoom2.wad`, 28,787,748 B, as a second WAD) does **not** fit — it would
  need a WAD swap or growing `FS_MB` (one-number change, `mkgrubiso.py:68-74`).
- Host disk is irrelevant: 117 GB free on `/home`.
- Constraint reminder: assets must live on the AHCI `C:` FAT volume; RAMDISK
  (device 8) opens fail with KErrCorrupt.

## 4. Packaging path

Binary (proven pattern, `user/user.mk:1605-1641`):
`ports/wolf3d/*.c` → `ld -m elf_i386 --emit-relocs --gc-sections
--defsym=wolf3d_printf_slot=0x<free> -T user/link.ld` →
`tools/elf2e32.py <elf> <WOLF3D.E32> --euser-import 915 --iat-off 0x<free,
distinct from DOOM's 0x40000>` → `tools/e32read.py` validate. `elf2e32.py`
packs the ELF32 into a relocatable E32 via `tools/mke32.py`
(`build_args_e32`/`crc32_symbian`, E32ImageHeaderV, PE2 EUSER import);
`user/user.mk` gets one append-only cloned DOOM block.

Assets: extend `tools/mkgrubiso.py` (`find_wad()`/`WAD_CANDIDATES`,
`curated_programs(work, wad)`) with a Wolf3D asset list — add a `--wolf`
flag or generalize `--wad`; stage the 8 files under their native 8.3 names
(`VGAGRAPH.WL1` etc. are already 8.3 guest-safe, like `FDOOM1.WAD`). Gate
harness clones `tools/r1_doom_test.py` with its own asset-candidate list and
SKIP-on-missing-assets discipline.

## 5. Blocker/Decision needed

**Wolf3D assets cannot be obtained locally — nothing exists on this machine
and the project has no download path.** The only in-policy acquisition is
**user-supplied files** dropped into a candidate directory (same as Freedoom).
Additionally, Wolf3D's only real asset source (id shareware WL1) collides
with the written asset policy in `docs/DOOM_PORTING.md:153-155` ("do NOT ship
any id Software / shareware WAD; free-licensed only"). **Decision for the
user:** (a) supply WL1 files themselves and explicitly accept the shareware
assets, or (b) amend the policy, or (c) pick a free-licensed asset set /
different game. Until one of these lands, every Wolf3D gate SKIPs by design.

## 6. First steps

1. User drops the WL1 shareware set (or a free-licensed equivalent) into
   e.g. `qemu-port/wads/wolf3d/` or `/tmp/opencode/wolf3d/` and confirms the
   licence decision from §5.
2. Extend `tools/mkgrubiso.py` with the Wolf3D candidate paths + `--wolf`
   staging (8.3 names unchanged); verify with `mdir` that 64 MB/48 MiB still
   holds ~4 MB of new files.
3. Clone the `user/user.mk` DOOM block for `WOLF3D.E32` (new printf slot +
   IAT offset) and `tools/r1_wolf3d_test.py` from `r1_doom_test.py`;
   confirm a SKIP-clean run before any QEMU boot with assets.
