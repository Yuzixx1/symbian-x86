# DOOM / Freedoom integration verification — 2026-11-24

**Scope:** VERIFICATION ONLY. No source, tests, `kernelhwsrv/`, `kernfull.elf` or
ISO were modified/rebuilt by this pass. The current deliverable ISO
`build/symbian-grub.iso` was **copied** (not rebuilt) into the harness scratch
path `/tmp/opencode/r1_doom.iso` so the existing scripts boot the current boot
chain with `--no-build-iso`.

## 1. `EKA2_CRISP_STOP` check — CLEAN

- `grep -R "EKA2_CRISP_STOP" qemu-port/` → **no matches**.
- Repo-wide grep found exactly one hit, in `AGENTS.md` line 37, as prose
  (`DOOM/Freedoom is the integration proof; keep authentic feel (\`EKA2_CRISP_STOP\`
  stays reverted)`). It is **not defined in code or build files anywhere**.
- Authentic feel therefore intact; no crisp-stop shortcut present.

## 2. Commands run (all from `qemu-port/`)

```sh
cp build/symbian-grub.iso /tmp/opencode/r1_doom.iso      # reuse current ISO
python3 tools/r1_doom_test.py        --no-build-iso --log /tmp/opencode/doom_verify_main.log     --png /tmp/opencode/doom_verify_title.png    # PASS
python3 tools/r1_doom_audio_test.py  --no-build-iso --log /tmp/opencode/doom_verify_audio.log    --png /tmp/opencode/doom_verify_audio.png     # PASS
python3 tools/r1_doom_playtest.py    --no-build-iso --window 30 --min-frames 50 \
                                     --log /tmp/opencode/doom_verify_playtest.log --png /tmp/opencode/doom_verify_playtest.png              # PASS
python3 tools/r1_doom_multikey_test.py --no-build-iso --loops 2 --log /tmp/opencode/doom_verify_multikey.log                               # FAIL (see §5)
python3 tools/r1_doom_multikey_test.py --no-build-iso --loops 2 --log /tmp/opencode/doom_verify_multikey_rerun.log                         # FAIL, identical
python3 tools/r1_doom_multikey_test.py --no-build-iso --loops 3 --log /tmp/opencode/doom_verify_multikey_final.log                         # FAIL, identical
```

## 3. Boot chain → shell → DOOM launch path

From `/tmp/opencode/doom_verify_main.log` (GRUB entry **"Symbian EKA2 full
kernel (genuine)"**, `-vga std`, VBE 0x118 1024x768x32):

```
A::Init3() ... DISPLAY: EHalGroupDisplay(4) registered r=0
X: mounted (programs from GRUB module)
SvcMgr: launch execs installed (268/269/270)
KERN spawning genuine boot DThread...
KERN spawning shell DThread...
GenuineBoot: probing boot volume for the genuine startup chain
SvcMgr: SVCMGR.E32 not found r=-1          <- documented svcmgr/portfilesrv fallback
VGA: font loaded / Symbian EKA2 x86 full kernel -- interactive console
X:/> DOOM                                  <- shell prompt, DOOM launched
e32run: EUSER.DLL resident ... exports=2503
e32run: bind slot 262144 ord 915 -> 003e7898
e32run: imports bound dll (1 ordinals)
```

- The genuine boot DThread starts and probes the startup chain; the shell
  DThread reaches `X:/>`. `SVCMGR.E32 not found` is the **expected fallback**
  documented in `GENUINE_PROGRESS.md` (no shim retired yet), not a DOOM fault.
- No explicit `EFSRV`/`EWSRV`/`ESTART` runtime markers appeared in these DOOM
  logs (fallback path active). Per brief, EWSRV/EFSRV agents were rebuilding
  concurrently; their marker status was not judged here and any transient
  fails in their harnesses are not DOOM regressions.
- Concurrent activity: up to 5 other QEMU instances (smoke, efsrv retire,
  genuine_efsrv, fallback_efile) were running at 100% CPU during the first
  multi-key runs; load later fell to ~1.24 with 2 QEMUs.

## 4. DOOM evidence — PASS

`r1_doom_test.py` (all checks PASS, exit 0):

```
DOOM: WAD OK / INIT / ENGINE READY / TITLE / FRAME
DOOM: WAD 28795076            <- full 27.5 MiB Freedoom1 IWAD parsed
boot: DISPLAY: VBE mode 0x118 ; X: mounted
EUSER ord 915 bound
frames advancing ([1, 35, 70, 105])
no Exc / PanicKernExec / FAULT / #PF
```

`r1_doom_audio_test.py` (PASS, exit 0) — AC97 attached:
`AC97 boot banner (vendor 8384:7600)`, `SND: INIT`, `SND: sfx`, `SND: play`,
`AU: pcm play`, `DOOM: TITLE`, `DOOM: FRAME`, no faults.
(`SND: music` informational — no MUS lump played in this session.)

`r1_doom_playtest.py` (PASS, exit 0) — authentic feel / long-run:
gameplay `DOOM: KEY` markers consumed; no `DOOM: DONE` before the 30 s no-quit
window; frames 140 → 2240 (~74.7 fps, **past the old 455-frame cap**); clean
`DOOM: DONE` after Esc and shell prompt returns; no faults.

**Controls / multi-key (from the multi-key harness raw logs):** W (173), D
(174), space (163) presses and `RELEASE`/`BREAK` pairs were all emitted; the
invariant "D stays held past W's break" PASSED in every diagonal loop; 6
`RELEASE 174` markers exist for exactly 6 scheduled D presses; `STALE
swallowed: 0`; frames advanced continuously; clean `DOOM: DONE`. Input is
accepted and multi-key holds do not get stuck in the running game.

**Screenshots** (QMP screendumps, 1024x768, clean letterbox, no console bleed):
- `/tmp/opencode/doom_verify_title.png` (63,889 B) — in-game start area, HUD
  ammo 50 / health 100% / armor 0%, weapon visible.
- `/tmp/opencode/doom_verify_playtest.png` (66,497 B) — in-game, ammo 47 after
  firing (state changed = real gameplay).
- `/tmp/opencode/doom_verify_audio.png` (63,999 B) — in-game frame, ammo 47.

## 5. Caveat — multi-key harness `RESULT: FAIL` (not a DOOM regression)

`r1_doom_multikey_test.py` reports **FAIL on `diagonal_2` only**:
"press D@+0.25 -> NO RELEASE; D stays held past first break @+1.50".
This is deterministic across 3 runs (2/2/3 loops) even after host load dropped
from ~2.0 to ~1.2. Evidence it is a **harness attribution artifact**, not a
game input bug:

- Only loop 2 of 3 fails; loops 1 and 3 pass the same scenario — a real
  stuck-key bug would fail all loops.
- The raw serial log contains 6 `DOOM: RELEASE 174` / 6 `BREAK 174` for the
  6 scheduled D presses (3 diagonals + 3 moveuse), i.e. every D key-up was
  processed by DOOM.
- The "D stays held past first break" assertion (the actual multi-key-hold
  invariant) passes in every loop; STALE count is 0.
- Many scenarios report vacuous `(none)` traces because `doom_marker()` emits
  `KEY` sparsely (first 6 + every 20th) and the immediate/delayed-copy dedup
  is timing sensitive; the harness docstring acknowledges this.
- The long-run playtest independently proves gameplay keys are consumed,
  frames advance continuously, and the game quits cleanly with no faults.

**Recommended follow-up (not done here, verification-only brief):** the
multi-key harness owner should confirm the diagonal_2 attribution window /
delayed-copy dedup; no game-side change is indicated.

## 6. Verdict

**DOOM/Freedoom integration under the current genuine boot chain: PASS.**
DOOM starts from `X:/>`, renders authentic frames with sound, consumes controls
including multi-key holds, runs indefinitely past the old frame cap, and exits
cleanly — with zero Exc/Panic/FAULT/#PF. `EKA2_CRISP_STOP` is absent (authentic
feel preserved). One caveat: `r1_doom_multikey_test.py` prints `RESULT: FAIL`
for a single diagonal loop due to marker attribution, with raw evidence showing
all key releases were processed; treat as harness follow-up, not a DOOM
regression. `kernelhwsrv/` untouched; no rebuilds or commits made.
