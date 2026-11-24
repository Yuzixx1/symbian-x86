# FINAL_VERIFY_REVIEW — tools/r1_final_verify.py

Verdict: **NEEDS FIXES (minor)** — commands/CLI are valid, but two false-PASS paths remain.

1. MEDIUM (`tools/r1_final_verify.py:73-76,104-108`): default `--no-build-iso` reuses a possibly stale `/tmp/opencode/r1_doom.iso`; selfcheck builds `symbian-grub.iso`, not the DOOM ISO. A final run can PASS on old artifacts. Fix: build/refresh the DOOM ISO once by default (or check mtime); `--rebuild-iso` already drops the flag correctly.
2. MEDIUM-latent (`:148-154`): last explicit verdict unconditionally overrides nonzero exit rc. A script printing PASS/SKIP then crashing (rc!=0 with traceback) reports PASS. Fix: if rc!=0 and word!="FAIL", force FAIL / flag conflict.
3. LOW (`:88,256`): SKIP is globally non-fatal — an unexpected SKIP from a required gate (selfcheck/qemu_smoke) would pass silently. Current required gates only emit PASS/FAIL, so not reachable today.
4. LOW (`:250-261`): "NOT RUN" rows from `--no-keep-going` are absent from BAD, so the OVERALL "fail/not-run" count under-reports; overall still FAIL because the triggering failure is counted.

Confirmed OK: all 11 scripts exist; `--no-build-iso` accepted by all 4 DOOM tests; `r1_genuine_fat_io_test.py` filename correct; selfcheck prints `OVERALL: PASS/FAIL` matching OVERALL_RE; last-by-offset parsing defeats early-PASS misparse; TIMEOUT in BAD; unknown `--only`/`--skip` names rejected (rc=2); `--skip-selfcheck`, `--rebuild-iso`, `--dry-run`, `--list`, `--only` behave as documented; logs default to `/tmp/opencode/final_verify_<name>.log`.

Dry-run result: py_compile OK; `--list` = 11 tests; default dry-run = 11 cmds; `--only doom_main` = 1 cmd; `--rebuild-iso` drops `--no-build-iso`; `--skip bogus` -> rc 2. No QEMU/destructive commands run; kernelhwsrv untouched.
