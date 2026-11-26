# ASSET_RECOVERY_NOTE — recovery asset durability (read-only, Wave 1)

Verdict: **durable enough for the rest of the wave**, with one caveat: it lives only in `/tmp`.

- `freedoom1.wad` — `/tmp/opencode/doom/freedoom-0.13.0/freedoom1.wad`, mode 0644, 28,795,076 B.
  Header `IWAD` (`49 57 41 44`), 3163 lumps, directory at 28,743,444 — a valid IWAD, not a truncated or foreign file.
- `sha256 7323bcc168c5a45ff10749b339960e98314740a734c30d4b9f3337001f9e703d`, `md5 b93be13d05148dd01614bc205a03648e`
  (pin these to detect re-fetch drift on any future drop-off).
- `/tmp/opencode/final_verify_recovery.log` ends `OVERALL: PASS (11 pass, 0 skip, 0 fail/not-run)` — all 11 gates green.
- **FreeDM is not recoverable locally**: no `freedm*.wad` exists under `/tmp` or `$HOME`; the only WAD in the
  Freedoom dir is `freedoom1.wad`. The P3 FreeDM gate therefore stays **SKIP** until an explicit user drop-off.
  This matches D2 (FreeDM PWAD on the existing `DOOM.E32`) and D5 (per-test WAD swap, do not grow `FS_MB`).
- Caveat: no repo copy and `/tmp` is not reboot-durable. Withdrawal would retake Doom gates and force a
  re-fetch, which is why step 0 of this wave copies the WAD out before any build.

---

> **SUPERSESSION 2026-11-26 — authoritative gate is 15 rows, `FINAL27_RC=0`.**
> The `:9` line "`/tmp/opencode/final_verify_recovery.log` ends `OVERALL: PASS
> (11 pass, 0 skip, 0 fail/not-run)` — all 11 gates green" is **historical**; that
> log is a superseded 11-row artifact. The authoritative run is
> `/tmp/opencode/final27_gate.log` at HEAD `ee8490e`, ending
> `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)` → **15/15 PASS, `FINAL27_RC=0`**,
> with the 15-row roster confirmed by `tools/r1_final_verify.py --list` (row 15 =
> `hal_probe`). `FINAL26_RC=0`, `FINAL23_RC=0`, `FINAL20_RC=0` and every 11-row
> count are superseded history only. The asset facts above (WAD path, mode, size,
> `sha256 7323bcc1…`, the FreeDM-not-recoverable finding) are unaffected and stay
> as written. No QEMU, no gates run, no source edits, no commits/push, no
> `make clean`, `kernelhwsrv/` untouched.
