# LD_LIBRARY_PATH plan (wave-3 item c)
Sources: `docs/GENUINE_BUILD_LINK_AUDIT.md:8`, `docs/WAVE2_NEXT_STEPS.md:13`.
Verified: all four scripts still lack the unset, and no caller wraps them
(`unset LD_LIBRARY_PATH` has zero hits across `*.sh/*.py/Makefile/*.mk`); this
shell points it at `/home/yuzi/.t3/tools/mistral-vibe/3.1.4/linux-x86_64/_internal`.
Insert directly after the `set -e` line the audit cites —
`genuine/efsrv_min/build_server.sh:6`, `genuine/dm/build_dm.sh:8`,
`genuine/f32_probe/build_probe.sh:4`, `genuine/hal/build_hal.sh:5`.

Fix: one bare `unset LD_LIBRARY_PATH` per script — POSIX, valid under both the
`#!/bin/bash` and `#!/bin/sh` shebangs here, inherited by every gcc/ld/python3
child. Not inline `env -u`, which would cover one command, not the script tail.
Outputs unchanged: the variable steers only the host dynamic loader and never
enters CXXFLAGS or link inputs, so ELF/E32 bytes match the wave-2 `env -u` runs.
