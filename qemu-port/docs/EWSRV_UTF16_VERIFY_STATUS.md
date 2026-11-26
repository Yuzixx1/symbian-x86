# EWSRV_UTF16_VERIFY_STATUS — 2026-11-24 ~21:05 (read-only monitor)

**Status: VERIFIED** (targeted runtime checks post `-fshort-wchar` rebuild).

- `docs/EWSRV_UTF16_RUNTIME_VERIFY.md`: does NOT exist; nearest doc is
  `FSHORTWCHAR_EWSRV_STATUS.md` (20:57, "LANDED-UNVERIFIED" at the time).
- Hand-client Connect r=0 after 20:55: `dbg_ewsrv_guihello.log` (21:00)
  `exec128 SessionCreate name=<!E32WindowServer>` → `WSE32: Connect r=0`
  (lines 891-892); `dt4_desktop.log` (20:58) `DT: Connect r=0` x2. Staged
  images byte-scanned: UTF-16 lit name only, zero UTF-32 occurrences.
- Post-20:55 runs all PASS: `verify_ewsrv_run.log` (20:59, EWCLIENT
  Create/Init/SetTitle/Write r=0), `verify_pointer_run.log` (20:59),
  `verify_input_run.log` (21:00), `r1_qemu_smoke.log` (21:00).
- Caveat: no `final_verify` OVERALL after 20:55 (newest 20:41, PASS 11/11,
  pre-rebuild); `gui_init rc=0` not yet seen (old `dbg_guihello.log` rc=-1).
- kernelhwsrv/ untouched; no QEMU run by this monitor.

---

**Superseded 2026-11-26.** Any 11/11 strict-gate result recorded above is
**historical**. Superseded by the 15-row gate: **15 rows / `FINAL27_RC=0`**
(15 pass, 0 skip, 0 fail; `/tmp/opencode/final27_gate.log`). The body above is
left byte-identical and was not re-verified against the 15-row gate.
