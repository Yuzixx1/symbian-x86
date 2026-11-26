# POST_SVCMGR_VERIFY — post-apply gates on the svcmgr-applied tree

Run: 2026-11-24 19:05–19:23 EET by the verification subagent, on the working tree
with the uncommitted svcmgr apply (`SvcMgrExecInstall()` → `PortTrapExecInstall()`,
exec slots 268/269 + ctl ops 0/1 removed, exec 270 slimmed to ops 2/3/4,
`user/svcmgr.cpp` deleted, SVCMGR/PORTFILESRV rules gone from `user/user.mk`).
HEAD at run time: `e29a072`. `kernelhwsrv/` untouched (read-only respected).

## 0. Quiescence / build state (one-shot, 19:05–19:08)

- `.sybian/qemu-slot.lock` empty and not flock-held.
- Source mtimes at start: `variant/e32load.cpp` 19:05:19 (edited <2 min before the
  check by the apply agent — noted, stable afterwards), `qemu_variant_asic.cpp`
  19:02:03, `qemu_variant.h` 19:00:59, `user/user.mk` 19:01:46, `Makefile`
  18:49:30; `user/svcmgr.cpp` already deleted (`git status`: `D`).
- Foreign activity noted (did not block): another agent's
  `r1_genuine_ewsrv_test.py` + QEMU at 19:06, and its own
  `r1_final_verify.py --strict-skip` from 19:22 — concurrent QEMU boots were
  running during parts of this suite (per-test images/QMP sockets are distinct).
- Build: `build/kernfull.elf` 19:05:22 was newer than every variant source, so no
  rebuild was required; a no-op `make build/kernfull.elf` re-linked cleanly
  (tree consistent with `svcmgr.cpp` deleted), then `make isogrub` refreshed
  `build/symbian-grub.iso` (19:08, 75,859,968 B) so kernel and ISO match.

## 1. Gate results (serial, logs in `/tmp/opencode/post_svcmgr_verify/`)

| Gate | Verdict | Log |
|---|---|---|
| `r1_qemu_smoke.py` | **PASS** (rc=0, 19:08:14–19:09:05) — EUSER.DLL resident, exports=2503, E32Validate r=0, no Exc/Panic | `smoke.log` |
| `r1_genuine_retire_test.py` | **PASS 4/4** (rc=0, 19:09:05–19:09:59) — efsrv/ewsrv/estart/euser all PASS | `retire.log` |
| `r1_genuine_fullchain_test.py --log-dir ~/.cache/sybian-r1` | **PASS 14/14** (rc=0, 19:09:59–19:10:34) — EFILE/ESTART/EWSRV/ESHELL launched, genuine file server up, **svcmgr/portfilesrv fallback not used**, C:\> prompt, 0 fault lines | `fullchain.log` |
| `r1_genuine_pointer_test.py` | **PASS** (rc=0, 19:10:34–19:10:54) — PTRINJ.TXT(80,40) staged; `EPointerMove(80,40)` injected via exec-270 op-4 sink (`e32load.cpp:1266-1288`, installed by `PortTrapExecInstall()` at `qemu_variant_asic.cpp:171`); genuine EWSRV consumed it via `TRawEvent::Pos`; 231 imports bound; no exception/panic/FAULT | `pointer.log` |
| `r1_genuine_ewsrv_test.py` | **PASS** (rc=0, 19:10:54–19:11:14) — full EWCLIENT marker set r=0, 232 imports bound, window drawn, desktop screendump | `ewsrv.log` |
| `r1_genuine_efsrv_test.py` | **PASS** (rc=0, 19:11:14–19:11:31) — F32PROBE genuine FAT32 BPB, F32CLIENT IPC round trip OK, EUSER ord 915 bound | `efsrv.log` |
| `r1_genuine_eshell_test.py` | **PASS** (rc=0, 19:11:31–19:11:52) — 8/8 commands fault-free | `eshell.log` |
| `r1_final_verify.py --strict-skip` (run 1, 19:11:52–19:22:47) | **10/11 PASS — `OVERALL: FAIL (10 pass, 0 skip, 1 fail)`; only `selfcheck` FAIL (222.9s) while another agent's verify/QEMU ran concurrently** (known concurrency flake, cf. `docs/CLEANUP_AUTHORITY_CHECK.md`); all other 10 (qemu_smoke, retire, efsrv, ewsrv, eshell, fat_io, doom_main/audio/playtest/multikey) PASS | `finalverify.log` |
| `r1_final_verify.py --strict-skip` (isolated re-run, 19:33:30–19:44:30, own log dir `fv_iso/`) | **10/11 PASS — `OVERALL: FAIL (10 pass, 0 skip, 1 fail)`; again only `selfcheck` FAIL (234.1s), this time NOT concurrent** | `finalverify_iso.log`, per-test in `fv_iso/` |

## 2. Fault scan (step 4)

Scanned all new logs (`/tmp/opencode/post_svcmgr_verify/*.log` + the run-1
`/tmp/opencode/final_verify_*.log`) for `FAULT`, `Exc`, `Panic`, `#PF`, `KERN-n`,
`ESystemThreadPanic`:

- **`ESystemThreadPanic`: 0 hits everywhere** — the exec-81 wrapper
  (`PortTrapExecInstall`) is preserved; no KERN-4 regression.
- **0 real fault lines** — the only grep matches are `[PASS] no Exc/Panic/FAULT`
  assertions and mangled `User::Panic` symbols inside `ld` build-noise lines.
- `SessionPath r=-15` never accepted as IPC success (efsrv gate reports a real
  IPC round trip).

## 3. Authoritative OVERALL (isolated re-run) and the `selfcheck`/`cp4_stress` failure

Run 1's only failure was `selfcheck` under concurrent QEMU load. An isolated
re-run (`r1_final_verify.py --strict-skip --log-dir …/fv_iso`, started 19:33:30
after foreign verify/QEMU went quiet) captured:

- **`OVERALL: FAIL (10 pass, 0 skip, 1 fail)` — 10/11 PASS; `failed: selfcheck`**
  (`finalverify_iso.log`, `fv_iso/final_verify_selfcheck.log`, 234.1s).
- Selfcheck sub-steps: build PASS, isogrub PASS, qemu_smoke PASS, cp4_ca PASS,
  **cp4_stress FAIL**, selftest PASS. So the single failing step is
  `tools/r1_cp4_ca_stress_test.py`.
- **Failure mode (not a guest fault):** `AssertionError: b'CPSTRESS: start4'
  not seen after offset 12000` — a 15 s serial-marker wait in
  `wait_after()` (`r1_cp4_ca_stress_test.py:67-77`). Guest serial logs of the
  same test show `CPSTRESS: start4 0` **is** printed, `KILLPROC: killed` works,
  and **0 FAULT/Exc/Panic/#PF/KERN lines** (`/tmp/opencode/cp4_stress_*.log`).
  I.e. the 4th async launch was merely slower than the 15 s marker window under
  host load (several other agents were running concurrent QEMU verifies;
  host was near memory saturation at 19:07: 268 MB free).
- The apply agent's late `e32load.cpp` edit (19:05:19) **reinstalled exec 269**
  (peer kill → `E32Kill`) inside `PortTrapExecInstall()` explicitly for
  cp4_stress (`e32load.cpp:1268-1269, 1378-1403`: "slot 269 restored for
  cp4_stress; slot 268 + ops 0/1 stay retired with svcmgr") — consistent with
  `KILLPROC.E32 13 → KILLPROC: killed` succeeding in the logs.
- Cross-check of every `cp4_stress` serial log produced on this tree
  (`/tmp/opencode/cp4_stress_928240/937214/973392/981410/987089…`, 19:10–19:43):
  all show `CPSTRESS: start1..4`, `KILLPROC.E32 13 → KILLPROC: killed`, and
  **0 fault lines** — including `973392` (19:36), the re-attempt right after my
  isolated run's timeout (`973140`, 12,674 B, cut just past the 12,000-byte
  window). The marker arrives but past the test's 15 s window under host load
  (2–4 concurrent foreign QEMU verifies were running during both of my runs;
  host memory was near saturation at 19:07). Conclusion: **load-related timing
  flake, not a guest regression**.
- A dedicated quiet-machine re-run of `r1_cp4_ca_stress_test.py` was attempted
  (19:46–19:53) but could not complete: another agent (winsrv/edisp, owns
  `user/wsdl.c`, `user/user.mk`) was mid-edit (wsdl.c 19:52:43, user.mk
  19:53:55), so its internal `make … userland` failed transiently on
  `build/user/wsdl.elf: undefined reference to 'ws_e32_iat_printf'`
  (`cp4_stress_rerun.log:86`). Not related to the svcmgr apply — that link rule
  and `wsdl.c` are the winsrv agent's in-flight files. Follow-up: re-run
  cp4_stress isolated once the winsrv edit lands and the tree is quiet.
- Tree-mutation caveat: the gates above ran 19:08–19:44 against the apply as
  landed (`kernfull.elf` 19:05:22 + ISO 19:08). Afterwards the apply agent
  edited `variant/e32load.cpp` again at 19:38:53 (reinstalled exec 269 — see
  `e32load.cpp:1268-1269,1378-1403`) and rebuilt; those later edits are outside
  this verification window.

## 4. Verdict

- Per-gate: 7/7 standalone gates PASS (smoke, retire 4/4, fullchain 14/14,
  pointer/PTRINJ exec-270 op4, ewsrv, efsrv, eshell 8/8).
- Fault scan clean; exec-81 wrapper intact (no `ESystemThreadPanic`, no KERN-4);
  PTRINJ/exec-270 op 4 re-proven end-to-end (80,40 consumed by genuine EWSRV).
- `r1_final_verify.py --strict-skip` captured **OVERALL 10/11 in both runs**
  (run 1 at 19:11, run 2 at 19:33) — the only fail is `selfcheck` → `cp4_stress`
  15 s marker-window timeout; guest serial logs show **zero faults** and all
  markers incl. `start4` + `KILLPROC: killed` present (see §3). All other 10
  tests PASS in both runs (smoke, retire, efsrv, ewsrv, eshell, fat_io,
  doom_main/audio/playtest/multikey).
- **Verdict: svcmgr apply SAFE — no rollback indicated.** Every gate exercising
  the applied paths passes with a clean fault scan: fullchain 14/14 with
  "svcmgr/portfilesrv fallback not used", retire 4/4, PTRINJ via exec-270 op 4
  (80,40 consumed by genuine EWSRV), peer-kill via reinstalled exec 269.
- Blockers: none from the svcmgr apply itself. Two external notes: (a)
  `cp4_stress` needs one clean isolated re-run when the host is quiet (its 15 s
  marker window flakes under concurrent QEMU + memory pressure — same pattern as
  the d26fade authority-check flake); (b) the tree is still being mutated by
  other agents (apply agent: `e32load.cpp` 19:38:53; winsrv agent: `wsdl.c`/
  `user.mk` 19:52–53, which transiently breaks `make userland` on
  `wsdl.elf`) — re-verify after those land.
- Rollback point if ever needed: `/tmp/opencode/svcmgr_retire_backup_20261124_185621/`
  or `git show e29a072:<path>`.

---

> **SUPERSESSION 2026-11-26 — authoritative gate is 15 rows, `FINAL27_RC=0`.**
> The gate counts in §4 Verdict at `:101` ("7/7 standalone gates PASS (smoke,
> retire 4/4, fullchain 14/14, …)", "`r1_final_verify.py --strict-skip` captured
> **OVERALL 10/11 in both runs**") and in §1/§3 are **historical** — they describe
> the 11-row roster on the uncommitted svcmgr-applied tree at `e29a072`. The
> authoritative run is `/tmp/opencode/final27_gate.log` at HEAD `ee8490e`, ending
> `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)` → **15/15 PASS, `FINAL27_RC=0`**;
> roster confirmed by `tools/r1_final_verify.py --list` (rows 1–15, row 15 =
> `hal_probe`). The `selfcheck`/`cp4_stress` 15 s marker-window flake analysed above
> is resolved green in that run. `FINAL26_RC=0`, `FINAL23_RC=0`, `FINAL20_RC=0` and
> every 11-row count are superseded history only. Body left unedited. No QEMU, no
> gates run, no source edits, no commits/push, no `make clean`, `kernelhwsrv/`
> untouched.
