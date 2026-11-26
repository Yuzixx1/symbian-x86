# PLAN_DECISION_REGISTER — conflicts between stated choices and technical reality

**Date:** 2026-11-25. **Scope:** read-only synthesis of all 12
[`PLAN_SURVEY_*.md`](PLAN_SURVEY_ESOCK.md) surveys in this directory plus
[`GENUINE_PROGRESS.md`](GENUINE_PROGRESS.md) and the repo
[`AGENTS.md`](/home/yuzi/Documents/Sybian REAL/AGENTS.md). No code read beyond
docs, no edits outside this file, no commit/push, no `make clean`, no QEMU.
`kernelhwsrv/` untouched (read-only).

## Top summary

The project's standing rules — "as much REAL Symbian code as possible, own code
only as x86/PC glue, never a replacement for real Symbian logic" (AGENTS.md),
"kernelhwsrv/ is READ-ONLY", "no shim retired until its gate passes", "never
commit unless asked" — are individually sound but jointly unsatisfiable in
five places, because the genuine sources for sockets (ESOCK), GUI
(UIKON/GDI/FBSRV), and media codecs simply do not exist in the frozen tree and
the prebuilt EUSER.DLL's 2503-ordinal export table cannot grow. Each entry
below names one conflict, the realistic options, a recommendation, and the
specific decision the user must make before planning can proceed. Entries 1–4
are the four known conflicts; 5–8 are additional ones found in the surveys.

---

## 1. "Genuine ESOCK / sockets" vs. a tree that contains no socket code

- **Conflict:** The goal of genuine socket support collides with three hard
  facts ([`PLAN_SURVEY_ESOCK.md`](PLAN_SURVEY_ESOCK.md),
  [`PLAN_SURVEY_SOURCE_IMPORT.md`](PLAN_SURVEY_SOURCE_IMPORT.md)): no
  ESOCK/commsdb/protocol-module sources exist anywhere in `kernelhwsrv/`;
  prebuilt EUSER.DLL exports zero socket ordinals and its table is frozen
  because `kernelhwsrv/` is read-only; and PE2 images can only reach sockets as
  IPC functions on a server — which is exactly what the authored
  `user/netstack.c` `!SocketServer` already is. "Genuine ESOCK" as an EUSER
  client API (RSocket/RSocketServ) is therefore impossible without editing
  read-only code.
- **Options:** (a) keep the authored netstack/`!SocketServer` as the permanent,
  documented socket ABI (sanctioned exception, already in
  [`GENUINE_PROGRESS.md`](GENUINE_PROGRESS.md) §4.7 "netstack never"); (b)
  import upstream Symbian^3 ESOCK sources — viable only as one statically
  linked server behind the authored client ABI, since the EUSER ordinal link
  is impossible, so it adds provenance cost without changing the ABI; (c)
  unlock `kernelhwsrv/` to add socket ordinals — violates the read-only
  boundary outright.
- **Recommendation:** (a). It is the only option that keeps every standing rule
  intact; (b) only if the user explicitly wants upstream ESOCK internals as the
  server engine behind the same ABI.
- **User must decide:** formally accept the authored netstack as the permanent
  socket layer (closing "genuine ESOCK" as a goal), or authorize an external
  ESOCK import with `genuine/imported/` provenance marking — and never (c).

## 2. "Genuine UIKON/GDI graphical GUI" vs. absent sources + the never-replace rule

- **Conflict:** A genuine graphical desktop needs UIKON/EIKON, GDI/FBSRV,
  fonts/bitmap server and a graphical winsrv — none exist in `kernelhwsrv/`,
  `sdk/`, or the preservation archive
  ([`PLAN_SURVEY_UIKON_GDI.md`](PLAN_SURVEY_UIKON_GDI.md),
  [`PLAN_SURVEY_SOURCE_IMPORT.md`](PLAN_SURVEY_SOURCE_IMPORT.md)). The only
  genuine GUI-adjacent code is the text EWSRV plus the edisp LFB driver. Meanwhile
  the never-replace rule forbids writing our own GDI/UIKON, and the user already
  decided (AGENTS.md, 2026-11-24) to retire the authored winsrv/GUI
  (`94ce9c9`) and use console shell + DOOM as the UI "for now". So "genuine GUI
  returns later" can only mean external import, not local sources and not
  authored code. Import additionally hits loader limits: one DLL dependency
  per image, 512 import slots, 512 KiB image buffer, and the unresolved B1
  cross-process `!`-name registration blocker
  ([`PLAN_SURVEY_WSRV_NAME.md`](PLAN_SURVEY_WSRV_NAME.md)).
- **Options:** (a) stay on console + DOOM + genuine text EWSRV (current
  decision; zero risk); (b) authorize importing upstream UIKON/FBSRV/graphical
  winsrv as `genuine/imported/` single-image static PE2 builds, after raising
  the 512 KiB cap and fixing `!`-name registration — highest-value import target
  but real porting work against ARM-era ABI assumptions; (c) write an authored
  GDI behind a Symbian-shaped ABI — forbidden by the never-replace rule.
- **Recommendation:** (a) now; treat (b) as a separately authorized,
  gated phase (new gate G13 per [`PLAN_SURVEY_GATES.md`](PLAN_SURVEY_GATES.md))
  only if the user wants the GUI back. Do not plan any client code against an
  authored GDI.
- **User must decide:** confirm console+DOOM as the standing UI (closing the
  GUI goal), or authorize the external UIKON/GDI import with its prerequisites
  (cap raise, B1 name fix, G13 gate) — and rule out (c) explicitly.

## 3. "Delete all bypassed shims" vs. live kernel hooks pinned by the gates

- **Conflict:** Several "bypassed" components are not dead files but live,
  load-bearing kernel hooks that the user froze: exec 81
  `PortFsRegisterThread` wrapper (removal reintroduces KERN 4 on EFILE
  teardown — AGENTS.md EFILE decision), exec 269 `PortKillExec` (asserted by
  `r1_cp4_ca_stress_test.py`), exec 270 ops 2/3/4 incl. the PTRINJ sink
  (asserted by `r1_genuine_pointer_test.py` and the fbblit path DOOM uses), and
  the `"!"` name-retry fallback in exec 128
  ([`PLAN_SURVEY_SHIMS.md`](PLAN_SURVEY_SHIMS.md),
  [`PLAN_SURVEY_WSRV_NAME.md`](PLAN_SURVEY_WSRV_NAME.md)). Worse,
  `tools/r1_genuine_retire_test.py` statically diffs shim opcodes in
  `fs/r1_fs_core.h` against genuine F32, so deleting that file breaks the
  11/11 gate itself. "Delete everything bypassed" therefore conflicts with
  "keep `r1_final_verify.py --strict-skip` 11/11 green" and with the frozen
  EFILE/KERN-4 constraint.
- **Options:** (a) phased deletion exactly per
  [`PLAN_SURVEY_SHIMS.md`](PLAN_SURVEY_SHIMS.md) §"Recommended deletion order":
  stale artifacts → orphan rules/host → port the retire-test opcode check off
  `fs/r1_fs_core.h` then delete `fs/` → opt-in channel → Lua shim → comments →
  DOOM shim last, never touching exec 81/269/270; (b) delete only the orphans
  and leave `fs/` in place (gate keeps passing, tree stays dirty); (c) rewrite
  the gates to not depend on shim files and delete broadly — changes test
  authority, highest risk.
- **Recommendation:** (a), re-running the strict gate after each step; accept
  that exec 81/269/270 and the DOOM/lua/EWSRV glue shims are legitimate
  bridges, not deletable shims, and that netstack is never deleted (entry 1).
- **User must decide:** approve the phased order (a) including the gate-check
  port before `fs/r1_fs_core.h` deletion, and explicitly confirm that exec
  81/269/270 stay frozen — or choose (b) to defer the `fs/` deletion.

## 4. "Another game" under a strict genuine-only rule vs. what a game port actually is

- **Conflict:** A second game is third-party (GPL) engine code plus authored
  platform glue — DOOM itself links the hand-written `ports/doom/e32_shim.c`
  into 4 of the 11 gates. A strict "genuine-only, no authored replacement logic"
  reading permits no second game at all; the proven pattern is authored
  port-bridge glue (fbblit exec 270, con_tryread input, exec 68 clock, exec 240
  audio, RChunk heap), which is sanctioned as glue but is not genuine Symbian
  logic. Hard constraints from
  [`PLAN_SURVEY_NEXT_GAME.md`](PLAN_SURVEY_NEXT_GAME.md): zero free slow-exec
  slots (must reuse 270/240/68/86/107/108), assets must live on AHCI `C:`
  (RAMDISK X: opens fail KErrCorrupt), ~16.8 MB disk headroom after Freedoom1.
- **Options:** (a) Wolfenstein 3D shareware — cheapest genuinely different
  game, 8bpp raycaster matches fbblit `src_bpp=8`, small IWAD fits disk
  (~600 lines glue + engine port); (b) second IWAD/PWAD on existing DOOM.E32 —
  zero code but proves only asset staging, not a new integration; (c) Heretic/
  Hexen — most bridge reuse, bigger engine diff and asset; (d) strict
  genuine-only → no second game.
- **Recommendation:** (a), preceded by the zero-cost (b) smoke test of the
  multi-WAD staging path; clone the `r1_doom_*` gate family with SKIP-on-missing
  assets (G14 per [`PLAN_SURVEY_GATES.md`](PLAN_SURVEY_GATES.md)).
- **User must decide:** relax "genuine-only" for game ports to "authored glue
  allowed, documented as port bridge" (the DOOM precedent), and pick the
  candidate — or accept that genuine-only means no second game.

## 5. "Bootable PC OS on real hardware" vs. frozen kernel + QEMU-shaped variant glue

- **Conflict:** The headline goal is a bootable PC OS, but the variant drivers
  are bus-0-only PCI (no bridge walk, no PCIe ECAM), PIC-only interrupts (no
  APIC/IO-APIC/MSI/ACPI anywhere), no NVMe, HID-only polled USB, and hard-coded
  QEMU assumptions (128 MB RAM, partlba 2048) — while the genuine
  `Interrupt`/`Asic`/`ResetEntry` hooks that would host APIC/ACPI live in
  read-only `kernelhwsrv/`
  ([`PLAN_SURVEY_HW_USB.md`](PLAN_SURVEY_HW_USB.md)). Real-hardware support beyond
  the GRUB-ISO + PS/2 + AHCI + VBE baseline therefore conflicts with the
  read-only boundary unless all new work stays variant-side.
- **Options:** (a) scope real-PC support to the low-risk baseline (GRUB combo
  ISO on CD-R, text console, PS/2, IDE/AHCI, e1000, AC'97, VBE) and add only
  variant-side drivers (recursive PCI walk + ECAM, USB mass storage); (b)
  unlock `kernelhwsrv/` for APIC/ACPI/MSI — violates read-only; (c) declare
  QEMU the only supported target.
- **Recommendation:** (a); keep QEMU CI primary and real HW a secondary smoke
  path (G15 off-CI harness per [`PLAN_SURVEY_GATES.md`](PLAN_SURVEY_GATES.md)).
- **User must decide:** accept the variant-only boundary for real-hardware work
  (closing APIC/ACPI as goals), or explicitly re-scope the read-only rule.

## 6. "Genuine media stack" vs. no codec/player sources in the tree

- **Conflict:** If a media player is wanted, the genuine tree has the SoundSc
  LDD contract but no DevSound server, no MMF codecs (MP3/WAV/MIDI), no image
  codecs, no video — anywhere. Playback is feasible (genuine `soundldd.cpp` +
  an authored AC'97 PDD + the working exec-240 path), but any decoder is
  authored code, and image/video playback is simply not derivable from genuine
  sources ([`PLAN_SURVEY_MEDIA.md`](PLAN_SURVEY_MEDIA.md)).
- **Options:** (a) audio-only player (green): compile genuine `soundldd.cpp`
  against an authored AC'97 PDD, authored WAV decoder feeding exec 240;
  (b) full media player with authored decoders — conflicts with never-replace
  if framed as "Symbian media logic", acceptable if framed as port glue/apps;
  (c) drop media ambitions.
- **Recommendation:** (a) if media is wanted at all; treat image/video as red.
- **User must decide:** whether a media player is in scope at all, and if yes,
  confirm audio-only with authored decoders as glue (not a "genuine media
  stack" claim).

## 7. Hygiene: surveys recommend committing docs vs. "never commit unless asked"

- **Conflict:** [`PLAN_SURVEY_HYGIENE.md`](PLAN_SURVEY_HYGIENE.md) recommends
  committing the closeout + 4 survey docs + 5 modified docs, archiving the 128
  `WINSRV_DELETE_WATCH*.md`, deleting stray `qemu-port/9`, and extending
  `.gitignore` — but AGENTS.md forbids commit/push without an explicit request,
  and `never git add -A` (the 2.4 MB preservation tarball must stay untracked).
- **Options:** (a) user explicitly authorizes a docs-only commit with a named
  file list (no `add -A`); (b) leave the tree dirty; (c) commit including the
  tarball — forbidden.
- **Recommendation:** (a) with an explicit per-file list when the user wants a
  clean tree; otherwise (b).
- **User must decide:** authorize the docs-only commit (and .gitignore edit) or
  not.

## 8. "Genuine = deleted, not bypassed" metric vs. permanent authored exceptions

- **Conflict:** [`PLAN_SURVEY_PROVENANCE.md`](PLAN_SURVEY_PROVENANCE.md) proposes
  counting a subsystem as genuine only when its shim files are deleted, to close
  the bypass-as-retirement hole — but netstack is a permanent authored exception
  (entry 1) and exec 81/269/270 are permanent bridges (entry 3), so a
  "100% genuine / zero glue" target conflicts with those standing decisions.
- **Options:** (a) define the metric with an explicit, documented whitelist
  (netstack, exec bridges, PS/2/AC'97/storage/display bridges) so the ratio
  measures retirement of *replaceable* shims only; (b) chase 100% and delete the
  exceptions — contradicts entries 1 and 3.
- **Recommendation:** (a).
- **User must decide:** adopt the whitelisted genuine-ratio metric as the
  definition of "genuine completeness".

---

## What planning can proceed on without further decisions

Entries 1(a), 2(a), 3(a)-with-frozen-hooks, 5(a), 7(b), 8(a) are all consistent
with the current standing rules and need no new authorization; the register
above only asks the user to *confirm* them or pick a different branch per entry.

---

**2026-11-26 — supersession footer (authoritative gate is now 15 rows):** every gate count in this file that reads **11/11**, **11-gate**, **11 gates**, **13/13** or **13-row** is superseded and is not current evidence. Register entries that treat the 11/11 gate as the thing to keep green refer to the 15-row gate. The authoritative strict gate is `python3 tools/r1_final_verify.py --strict-skip`, whose `DEFAULT_TESTS` list holds **15 rows** (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`, `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`, `doom_playtest`, `doom_multikey`, `hal_probe`), and it is green at **15 pass, 0 skip, 0 fail** with `FINAL27_RC=0` (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`). Where this file enumerates the gate's composition — selfcheck, qemu_smoke, genuine_retire and so on — read that enumeration as the 15 rows above rather than as a literal 11- or 13-item list. Standing plan decisions, control phrases (GO/NO-GO, PASS/SKIP/FAIL) and every other statement in this file are unchanged by this note; only the row count and the gate verdict are restated here.
