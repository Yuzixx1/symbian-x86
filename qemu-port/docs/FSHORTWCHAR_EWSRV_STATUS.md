# FSHORTWCHAR_EWSRV_STATUS — 2026-11-24 ~21:00 (read-only monitor)

**Status: LANDED-UNVERIFIED.**

- Landed: `genuine/ewsrv/build_ewsrv.py:56` and `build_client.py:48` both pass
  `-fshort-wchar`; `genuine/build/EWSRV.E32` (20:55:38) + `EWCLIENT.E32`
  (20:55:44) rebuilt. Byte scan: `!E32WindowServer` present ONLY as UTF-16
  (EWSRV @0x9a94/0xa440/0xb8a0, EWCLIENT @0xcbc); zero UTF-32
  (`21 00 00 00 45 00…`) occurrences in either image.
- Unverified: no post-rebuild boot test. Newest hand-client logs
  (`chunkb_guihello.log` 20:56:54, `dbg_ewsrv_guihello.log` 20:56:36,
  `dt4_probe.log` 20:55:54) still show `lit-name connect r=-1` /
  `bang connect r=0`, and their fs images do NOT contain the new
  EWSRV.E32 bytes — they ran the old staged (20:49) image.
- No `final_verify` OVERALL after 20:51 (newest: 20:41,
  `final_verify_slot269_retry.log` = PASS 11/11, pre-rebuild);
  `r1_genuine_ewsrv.log` / `final_verify_genuine_ewsrv.log` are 20:36.
- Next gate: stage the 20:55 EWSRV.E32/EWCLIENT.E32, boot, expect
  `exec128 SessionCreate name=<!E32WindowServer>` → `r=0`, then rerun
  final_verify. kernelhwsrv/ untouched.

---

**Superseded 2026-11-26.** Any 11/11 strict-gate result recorded above is
**historical**. Superseded by the 15-row gate: **15 rows / `FINAL27_RC=0`**
(15 pass, 0 skip, 0 fail; `/tmp/opencode/final27_gate.log`). The body above is
left byte-identical and was not re-verified against the 15-row gate.
