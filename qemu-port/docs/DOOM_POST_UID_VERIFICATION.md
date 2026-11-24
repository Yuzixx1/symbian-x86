# DOOM / Freedoom post-UID-re-stamp verification — 2026-11-24

**Scope:** VERIFICATION ONLY. No source, tests, build scripts, ISO, or
`kernelhwsrv/` were modified/rebuilt; nothing was committed or pushed. `make
clean` was not run. Goal: confirm DOOM/Freedoom still runs after the genuine
userspace changes (eshell UID re-stamp, guard-only EFILE, PS shim, trampoline
removal). Baseline PASS record: `DOOM_INTEGRATION_VERIFICATION.md`.

## 1. Preconditions — no active rebuild, current ISO staged

- Lock check: `.sybian/qemu-slot.lock` is empty, mtime **2026-11-23 22:15:36
  +0200** (stale); no `make`/`isogrub`/`grub-mkrescue`/`r1_final_verify` process
  was running. The only active QEMU job at start was
  `r1_efile_clean_prove.py` (a **test**, not a rebuild) which exited before the
  DOOM runs began. No wait was necessary.
- Current deliverable ISO: `build/symbian-grub.iso`, mtime **2026-11-24
  03:15:27.626352515 +0200**, size **75,882,496 B**.
- The harness scratch ISO `/tmp/opencode/r1_doom.iso` was stale (mtime
  2026-11-24 01:34:05, size 75,874,304 B), so the current ISO was **copied**
  there at 03:28:41; `cmp` confirmed byte-identical. All four tests ran with
  `--no-build-iso` against that copy.
- Read-only ISO content check (`pycdlib` extract of `/PROGRAMS.IMG;1` +
  `mdir -i …@@1048576`): `DOOM.E32` 339,120 B, `WADPROBE.E32` 33,048 B,
  `EUSER.DLL` 412,104 B, `FDOOM1.WAD` 28,795,076 B — all stamped 03:15.
- Host assets: `build/user/DOOM.E32` (00:48:15), Freedoom1 IWAD
  `/tmp/opencode/doom/freedoom-0.13.0/freedoom1.wad` (28,795,076 B),
  cached `euser.dll` (412,104 B).

## 2. `EKA2_CRISP_STOP` — absent from code

`grep -rn` for `EKA2_CRISP_STOP` across `qemu-port/` source/ build / script
files (`*.c`, `*.cpp`, `*.h`, `*.S`, `*.py`, `*.sh`, Makefiles), excluding
`docs/` → **no matches**. Repo-wide hits are documentation prose only
(`AGENTS.md:37`, `DOCS_INDEX.md`, `DOOM_INTEGRATION_VERIFICATION.md`,
`GO_NO_GO.md`, `PRECOMMIT_AUDIT.md`, `FINAL_VERIFY_RUNBOOK.md`,
`FINAL_PROJECT_SUMMARY.md`). Authentic feel preserved; no crisp-stop shortcut.

## 3. Commands run (all from `qemu-port/`)

```sh
cp build/symbian-grub.iso /tmp/opencode/r1_doom.iso
python3 tools/r1_doom_test.py --no-build-iso \
    --log /tmp/opencode/doom_postuid_main.log --png /tmp/opencode/doom_postuid_title.png
python3 tools/r1_doom_audio_test.py --no-build-iso \
    --log /tmp/opencode/doom_postuid_audio.log --png /tmp/opencode/doom_postuid_audio.png
python3 tools/r1_doom_playtest.py --no-build-iso --window 30 --min-frames 50 \
    --log /tmp/opencode/doom_postuid_playtest.log --png /tmp/opencode/doom_postuid_playtest.png
python3 tools/r1_doom_multikey_test.py --no-build-iso --loops 2 \
    --log /tmp/opencode/doom_postuid_multikey.log
```

Tests were run sequentially (shared `/tmp/opencode/r1_doom-qmp.sock`). `tee`
copies of stdout: `/tmp/opencode/doom_postuid_{main,audio,playtest,multikey}.out`.

## 4. Per-test verdicts

| Test | Verdict | Evidence |
|---|---|---|
| `r1_doom_test.py` | **PASS** (exit 0) | `DOOM: WAD OK / INIT / ENGINE READY / TITLE / FRAME`, WAD byte count 28,795,076, `DISPLAY: VBE mode 0x118`, `X: mounted`, `e32run: EUSER.DLL … exports=2503`, `bind slot 262144 ord 915 -> 003e98b8`, frames `[1, 35, 70, 105]`, no Exc/Panic/FAULT/#PF. |
| `r1_doom_audio_test.py` | **PASS** (exit 0) | AC97 banner `vendor 8384:7600`, `SND: INIT`, `SND: sfx`, `SND: play`, `AU: pcm play`, `DOOM: TITLE`, `DOOM: FRAME`, no faults. `SND: music` informational (no MUS lump). |
| `r1_doom_playtest.py` | **PASS** (exit 0) | Gameplay keys consumed, no `DOOM: DONE` inside 30 s window, frames **140 → 2205** (~73.5 fps, past old 455-frame cap), clean `DOOM: DONE` after Esc and shell prompt returns, no faults. |
| `r1_doom_multikey_test.py` | **PASS** (exit 0) | All **8/8** scenarios PASS (`diagonal_1/2`, `runfire_1/2`, `moveuse_1/2`, `rapid_1/2`), `STALE swallowed: 0` every loop, D/W hold invariants hold, no faults. |

Raw serial-log fault scan (`grep -cE 'Exc |PanicKernExec|FAULT:|MMU #PF'`)
over all four logs: **0 / 0 / 0 / 0**.

## 5. Screenshots (this run, QMP screendumps)

- `/tmp/opencode/doom_postuid_title.png` — 65,636 B (main, in-game/title frame)
- `/tmp/opencode/doom_postuid_audio.png` — 64,085 B (AC97 in-game frame)
- `/tmp/opencode/doom_postuid_playtest.png` — 66,497 B (1024x768, ~134 colours,
  in-game after gameplay)
- Multi-key run produced 21 fresh PNGs under `/tmp/opencode/multikey_*.png`
  (mtime 03:32:16–03:32:38), e.g. `multikey_diagonal_2_both_held.png`
  (56,455 B), `multikey_diagonal_2_after_D_release.png` (67,826 B),
  `multikey_rapid_2_final.png` (42,492 B).

## 6. Caveats

- The earlier baseline doc recorded `r1_doom_multikey_test.py` as `RESULT: FAIL`
  on `diagonal_2` only, attributed to a harness marker-attribution artifact.
  With the current ISO and the latest multi-key harness (tool mtime 01:55),
  **all 8 scenarios passed, including `diagonal_2`**; the artifact did not
  reproduce. No game-side or harness change was made in this pass.
- The EUSER ordinal-915 bind address is now `0x003e98b8` (baseline doc:
  `0x003e7898`), consistent with the UID re-stamp; binding and DOOM execution
  still succeed.
- These DOOM logs run the documented fallback boot path (no explicit
  EFSRV/EWSRV/ESTART runtime markers). The configured state is taken as given
  from the brief — guard-only EFILE (no trampoline), `estart_=0x10272C04`,
  `etshell_=0x101F7AE7`, all-four boot chain 14/14. DOOM is an integration
  proof, not a per-subsystem gate.
- No ISO rebuild or asset regeneration was performed; results apply to the
  byte-identical copy of the 03:15 staged ISO.

## 7. Verdict

**DOOM/Freedoom integration after the genuine-userspace/UID changes: PASS.**
All four tests pass; DOOM starts from `X:/>`, renders authentic frames,
initialises AC97 sound, consumes gameplay and multi-key input, runs past the
old frame cap, exits cleanly, and logs zero Exc/Panic/FAULT/#PF.
`EKA2_CRISP_STOP` is absent from code. `kernelhwsrv/` untouched; no rebuild,
commit, or push.
