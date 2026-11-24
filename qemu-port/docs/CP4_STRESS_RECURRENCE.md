# CP4_STRESS_RECURRENCE — 2026-11-24 (read-only diagnosis)

Verdict: **the stale-object cause did NOT recur.** `build/qemu_variant_asic.o`
is byte-identical to a fresh compile of the current
`variant/qemu_variant_asic.cpp`. The cp4_stress assert in the
euser_shim strict run is a **real functional change at HEAD**: commit
`dd22ae2` retired slow-exec slots 268/269, so ring-3 `exec2(269)`
(`user/killproc.c:26`, the cp4 peer kill) no longer reaches `E32Kill` — it
now lands on the genuine table slot 269 (`KernMsgTest`, see
`kernelhwsrv/kernel/eka/kernel/execs.txt`) and returns success without
killing. The "victim" survives and completes, tripping the assert.

## 1. Failing assertion (strict run, /tmp/opencode/final_verify_euser_shim_close.log)
- selfcheck step `cp4_stress` = `tools/r1_cp4_ca_stress_test.py`
  (`r1_os_selfcheck.py:36`); log `/tmp/opencode/r1_os_selfcheck_cp4_stress.log`,
  qemu serial log `/tmp/opencode/cp4_stress_952080.log` (19:24).
- `AssertionError` at `tools/r1_cp4_ca_stress_test.py:194`:
  **`cycle 1: done ids [1, 2, 3] != survivors [1, 3, 4]`**
  (CLIENTS=4, VICTIM=2, CYCLES=3 → survivors must be [1,3,4]).
- Serial-log signature (also in `cp4_stress_928240.log` 19:09 and
  `cp4_stress_937214.log` 19:13, identical): `KILLPROC: killed` is printed,
  yet later `CPSTRESS: done2 100` appears — client 2 (the victim) was never
  actually killed, and client 4 was still draining when the test aborted.

## 2. Staleness check — RULED OUT
| file | mtime | sha256 |
| --- | --- | --- |
| `variant/qemu_variant_asic.cpp` | 2026-11-24 19:02:03.515570117 | `2f97f9e3…` (== HEAD `0f9ec6a`, git-clean) |
| `build/qemu_variant_asic.o` | 2026-11-24 19:02:03.529570141 | `0d38df0e…` |
| fresh scratch compile of the source (exact `make -n -B` g++ cmd, `-o /tmp/opencode/asic_check.o`) | — | `0d38df0e…` **identical** |
| `build/kernfull.elf` | 2026-11-24 19:24:28.291 (relinked by the selfcheck build) | `9c5776e9…` |

- `make -n build/qemu_variant_asic.o` → "up to date"; the object is a fresh
  build of the current source (hash match), so the earlier stale-`.o` mode
  (`e441e39e…` vs `c6b6c4c3…`, docs/STALE_OBJECT_REBUILD.md) did **not** recur.
  (The 14 ms source→object mtime gap is odd but irrelevant: content is what
  matters and it matches.)
- `make -n build/kernfull.elf` would only recompile `r1_g_launch.o` + relink
  (normal per-run churn; `r1_g_launch.o` was rebuilt 19:33:50 by another
  session) — nothing stale among variant objects; no `variant/*.cpp` is newer
  than its `build/*.o`.
- userland: `user/user.mk` mtime 2026-11-24 19:18:15.962; `build/user/*.o`
  mostly older (Nov 21) but `make -n userland` → "Nothing to be done" —
  `user.mk` is not an object prerequisite; the euser_shim-closure objects
  (`ca_test.o` 19:18:22, `ca_glue.o` 19:18:30) are newer than `user.mk`.
  No stale userland closure either.
- `kernelhwsrv/` untouched (read-only; no edits made this session).

## 3. Root cause — exec 269 retirement (dd22ae2), not a build artifact
- `d26fade` installed `SvcMgrKillExec` at slow-exec slot 269
  (`fns[3] = {SvcMgrLaunchExec, SvcMgrKillExec, SvcMgrCtlExec}` → 268/269/270).
- `dd22ae2` ("Retire svcmgr…") removed those installs; HEAD's
  `PortTrapExecInstall()` (`variant/e32load.cpp:1377`) now writes only
  slot 81 (FsRegisterThread wrapper) and slot 270 (`PortCtlExec`, ops 2/3/4).
  Slot 269 is left at its genuine value: `KernMsgTest`
  (`kernelhwsrv/.../execs.txt`, slow slot 269, `user = special`).
- `user/killproc.c:26` still does `exec2(269u, pid, 0u)`; at HEAD that calls
  genuine `KernMsgTest`, returns ≥ 0 → KILLPROC prints `KILLPROC: killed`
  but the victim process survives → `done2` appears → assert at line 194.
  The shell `kill` still works (in-kernel `E32Kill`, `kern_shell.cpp:1344`);
  only the ring-3 path is broken. The comment at `e32load.cpp:1061`
  ("exec 269 is reachable from ring 3 (KILLPROC)") is now stale.
- Timeline correlation: cp4_stress PASS 18:33, 18:48, 18:58, 19:00
  (55167-byte full logs, 3× `IPCCLI: connect failed` recovery phase);
  FAIL 19:09, 19:13, 19:24 (16782-byte logs, identical assert) — all after
  the 19:02–19:05 working-tree edits that became `e29a072`/`dd22ae2`.

## 4. Does current HEAD pass cp4_stress?
**No — not run here, but the evidence says it fails deterministically:**
3/3 identical failures (19:09, 19:13, 19:24) on freshly built, provably
non-stale objects at HEAD; the mechanism (exec 269 no longer kills) is
structural, not timing. The single test was NOT re-run in this session
because two `r1_final_verify.py` runs were active
(pids 950132 `/tmp/opencode/final_verify_euser_shim_close.log`,
968597 `--log-dir /tmp/opencode/post_svcmgr_verify/fv_iso`); per the brief,
no test/make was started while a build was in flight.

## 5. Recommendation
- Not a rebuild problem — do **not** touch/rebuild objects for this.
- Fix the code path: re-install slot 269 → `E32Kill` in
  `PortTrapExecInstall()` (`variant/e32load.cpp`), or move the cp4 peer kill
  to a mechanism that survives svcmgr retirement (e.g. a dedicated port exec
  slot in the 274/275-free range per the execs.h overlap warning, or keep 269
  claimed explicitly). Update the stale comment at `e32load.cpp:1061`.
- After the fix, rebuild `kernfull.elf` + ISO and re-run
  `python3 tools/r1_cp4_ca_stress_test.py` (×3–4 serial) plus
  `r1_os_selfcheck.py` before the next full `r1_final_verify.py`.
- Note for the record: the assert signature is identical to the stale-`.o`
  incident (docs/CP4_STRESS_INDEPENDENT_CHECK.md), but the root cause here is
  different — always verify object freshness by hash before assuming either.

## Boundaries
Read-only diagnosis: no source/object/ISO modified, no commit/push, no
`make clean`, no test executed (build active), `kernelhwsrv/` untouched.
Scratch compile went only to `/tmp/opencode/asic_check.o`.
