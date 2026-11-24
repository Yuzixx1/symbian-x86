# SELFTEST_MARKER_REGRESSION — 21:13 strict run: selftest step false FAIL (read-only diagnosis)

Root cause: test-harness race, not a boot-output change. `tools/r1_selftest.py` uses fixed
shared paths (`DISK/FS/LOG = /tmp/opencode/r1_selftest.{img,fs.img,log}`, lines 21-23), truncates
LOG at every QEMU start (line 107 `> log`), and reads it exactly once 1.0 s after the wrapper exits
(lines 111-116). No lock exists (`r1_final_verify.py` has no flock; `/tmp/opencode/final_verify.lock`
is a 0-byte marker; `tools/r1_selftest.py`/`r1_os_selfcheck.py` are unmodified vs HEAD). Two
selfchecks overlapped: step logs `r1_os_selfcheck_build.log` 21:14:47 → `r1_os_selfcheck_selftest.log`
21:18:51 (disk rebuilt 21:17:20) while the strict run's own selfcheck was mid-selftest
(`final_verify_selfcheck.log` 21:17:37, 247.9 s) — the 2nd run truncated the shared log/disk, so the
1st checker read a marker-less partial log → all ST FAIL + ord-915 FAIL + "0 exceptions" PASS.

Evidence: guest log `/tmp/opencode/r1_selftest.log` (21:18:50, ctime=mtime) contains ALL 9 markers
verbatim (offsets 7334-7532), `e32run: bind slot 32768 ord 915`, `ST: RESULT PASS`, 0 exceptions;
running r1_selftest.py's own `find()` logic on that file passes 10/10. Exec 127/128 wrappers
(`variant/e32load.cpp` `PortServerCreateOpt`/`PortSessionCreate`) only KPrintf *after* the genuine
handler and emitted only the two "installed" lines (log lines 124/126); ewsrv `-fshort-wchar` is a
userland compile flag; `qemu_variant_int.cpp` diff is KBINJ input logging only. qemu_smoke PASSed in
the same window (`final_verify_qemu_smoke.log` 21:18). Same intermittent FAIL hit the 20:19 attempt
in `final_verify_slot269_retry.log`, cleared on the 20:41 retry — before the 20:41→21:13 edits.

Minimal fix: per-run unique paths (`LOG=f"/tmp/opencode/r1_selftest.{os.getpid()}.log"`, same for
DISK/FS and `r1_os_selfcheck.py:92` step logs) plus an `flock` in r1_selftest/r1_os_selfcheck;
re-read the log until the `terminating on signal` line appears instead of a fixed 1.0 s sleep.

Discriminating test: rerun `python3 tools/r1_os_selfcheck.py` with zero concurrent qemu/make
(`ps aux | grep -c '[q]emu-system-i386'` == 0). All ST markers + ord 915 PASS ⇒ harness race
confirmed; FAIL with markers absent from the guest log itself ⇒ real boot regression.
