# CP4_STRESS_POSTFIX_VERIFY — 2026-11-24 19:58 (+0200) — VERIFY-ONLY run

Scope: verify the exec-269 E32Kill fix (`variant/e32load.cpp` PortTrapExecInstall
slot 269 → `PortKillExec` → `E32Kill`) with repeated `r1_cp4_ca_stress_test.py`
runs. Read-only w.r.t. code; no commit/push, no `make clean`, no QEMU started
while the slot was busy. `kernelhwsrv/` untouched (`git status --short
kernelhwsrv/` = empty).

## 1. Quiescence check (one-shot, 19:58:17) — SLOT BUSY → runs NOT started

- `.sybian/qemu-slot.lock`: 0 bytes (empty) — OK.
- `ps` for make/qemu/python-test procs — **ACTIVE**:
  - pid 1004602/1004603 `python3 tools/r1_final_verify.py --strict-skip`
    (elapsed ~212 s at check, log `/tmp/opencode/final_verify_slot269_run2.log`,
    currently in `doom_main` phase; eshell PASS 20.6 s, fat_io PASS 15.5 s so far)
  - pid 1008923 `tools/r1_doom_test.py --no-build-iso` → pid 1008929/1008930
    `qemu-system-i386 … -cdrom /tmp/opencode/r1_doom.iso` (44 s, 84.9 % CPU)
- Per brief: busy → report and stop, no wait-loop. Therefore **0/3 cp4_stress
  runs and 0/1 smoke run were executed in this session** — no results to claim.

## 2. Build freshness (no rebuild needed; rebuild not attempted while busy)

| file | mtime |
| --- | --- |
| `variant/e32load.cpp` (the fix) | 2026-11-24 19:38:53 |
| `build/kernfull.elf` | 2026-11-24 19:57:34 (newer than source → fresh) |
| `build/symbian-grub.iso` | 2026-11-24 19:43 (newer than source → contains fix) |
| `tools/r1_cp4_ca_stress_test.py` (hardened) | 2026-11-24 19:57:23 |

Rebuild condition (`kernfull.elf` older than `e32load.cpp`) is **not** met;
the 19:57:34 relink is the known per-run `r1_g_launch.o` churn
(docs/CP4_STRESS_RECURRENCE.md §2). `make userland` not touched (peer wsdl.c
in flight — irrelevant, cp4_stress uses prebuilt CPSTRESS.E32).

## 3. Static confirmation the fix is live in source

- `variant/e32load.cpp:1383-1384`: `static TInt PortKillExec(TInt aPid, TInt) { return E32Kill((TUint)aPid); }`
- `variant/e32load.cpp:1399-1403` (in `PortTrapExecInstall`): slot 269 installed —
  `tab[3 + 2*269u] = 0;` (flags), `tab[4 + 2*269u] = (TUint32)(TLinAddr)&PortKillExec;`
  plus `KPrintf("PortTrap: kill exec installed (slot 269 -> E32Kill)")`.
- Comments updated at `:1061-1064`, `:1269`, `:1378-1380` (no longer stale).
- `kernelhwsrv/`: git-clean, no edits.

## 4. Test hardening present (peer agent, mtime 19:57:23)

`tools/r1_cp4_ca_stress_test.py` docstring + constants: the fixed 15 s
start-marker window that caused the 19:45 `start1 not seen` flake is replaced by
generous env-overridable polls — `BOOT_TIMEOUT=120`, `PROMPT_TIMEOUT=120`,
`MARKER_TIMEOUT=180`, `DONE_TIMEOUT=180` (defaults) — with fail-fast on hard
failure/fault markers so the wide window cannot mask a real break; the
kill-survivor and drain-count asserts stay strict. PID-unique serial log/disk
(`/tmp/opencode/cp4_stress_<pid>.{log,img}`) so concurrent runs never share
artifacts.

## 5. Prior run evidence (not from this session)

- `/tmp/opencode/cp4_stress_987089.log` (19:43:10, 55 300 B): contains
  `PortTrap: kill exec installed (slot 269 -> E32Kill)`, `CPSTRESS: start1 0`,
  `KILLPROC: killed` — full-size PASS log per docs/CRITICAL_PATH_STATUS.md.
- `/tmp/opencode/cp4_stress_989645.log` (19:45:35, 6 616 B): short fail log,
  `start1 not seen` timeout flake (pre-hardening test) — timing, not the
  pre-fix survivor bug.

## 6. Commands (planned vs executed)

- Planned: `python3 tools/r1_cp4_ca_stress_test.py` ×3 serial (require 3/3
  PASS) → `python3 tools/r1_qemu_smoke.py` (require PASS).
- Executed: none (slot busy, §1). Rebuild command `make build/kernfull.elf &&
  make isogrub` not needed (§2).

## Verdict

**NOT VERIFIED — blocked, not failed.** The exec-269 fix is statically live
(slot 269 → `PortKillExec` → `E32Kill`, e32load.cpp:1383-1403) and the kernel/ISO
builds are fresh w.r.t. it, and the hardened test (19:57:23) addresses the 19:45
timing flake; but 0/3 stress runs were possible because a
`r1_final_verify.py --strict-skip` (incl. DOOM QEMU) was active at the one-shot
quiescence check. Re-run `python3 tools/r1_cp4_ca_stress_test.py` 3× serial +
`r1_qemu_smoke.py` once the slot is free to convert this into SOLID/flaky.

## Blockers

- Active `r1_final_verify.py --strict-skip` (pids 1004602/1004603) + doom QEMU
  (pids 1008923/1008929/1008930) at 19:58:17 — slot busy per ps; brief forbids
  waiting and forbids running QEMU concurrently.

## 7. Re-check (2026-11-24 19:59:20 +0200) — STILL BLOCKED, no runs started

- One-shot quiescence re-check at 19:59:20: `.sybian/qemu-slot.lock` 0 bytes,
  but ps shows **active**: pid 1004603 `python3 tools/r1_final_verify.py
  --strict-skip` (since 19:54) and pid 1011450 `qemu-system-i386` (doom audio
  test, since 19:58, ~98 % CPU, `timeout 110` wrapper pid 1011449).
- Per brief: busy → do not start QEMU, no wait-loop. **0/3 cp4_stress runs,
  0/1 smoke run executed**; verdict remains **NOT VERIFIED — still blocked at
  19:59:20**, not failed. `kernelhwsrv/` git-clean at check time.
