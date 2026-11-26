# Genuine Build Verify

Read-only build evidence for genuine `kernelhwsrv` sources driven through the port's `tools/elf2e32.py` pipeline. No QEMU run, no commits.

| Log | Artifacts | Result |
| --- | --- | --- |
| `/tmp/opencode/verify_build_dm.log` | `dm_domainsrv.E32` (33392 B), `dm_domainpolicy.E32` (33008 B, 3 exports), `dm_domaincli.E32` (33108 B, 51 exports) | OK, `EXIT=0` |
| `/tmp/opencode/verify_build_hal.log` | `hal_smoke.E32` (33292 B) | OK, `EXIT_CODE=0` |
| `/tmp/opencode/verify_build_probe.log` | `F32PROBE.E32` (33072 B), `F32CLIENT.E32` (33020 B) | OK |
| `/tmp/opencode/efsrvmin_build3.log` | `EFSRVMIN.E32` (16396 B), `FATIOCLIENT.E32` (6412 B) | OK |

Every `.E32` was re-read with `tools/e32read.py` and reported `E32 OK`. None of the build scripts carry an execute bit, so all were invoked via `bash`/`sh`. All now unset `LD_LIBRARY_PATH` themselves. `kernelhwsrv/` was not modified.
