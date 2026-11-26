# Clean-tree build audit — stale refs after shim retirement (2026-11-25)

Scope: Makefile, toolchain.mk, user/user.mk, variant/, genuine/*/build_*.{sh,py}. Method: token grep for
winsrv.c, wsclient.*, guishost.h, qemu_windowsrv.*, qemu_gui_exec.cpp, services/r1_service_host.*,
r1_service_state.h, e32base/e32cmn shim objects, plus a read-only run of tools/r1_shim_residue_sweep.py.

## Zero build-active stale references in this set
- Makefile, toolchain.mk, genuine/build_efile.py, genuine/*/build_*.{sh,py}: no mention at all.
- user/user.mk:267 `build/user/%.o: user/%.cpp user/e32base.h` — user/e32base.h EXISTS, a live
  prerequisite of the genuine EUSER shim. Not stale; no fix.
- Comment-only breadcrumbs, leave as-is: user/user.mk:1356,1367,1372 (wsclient.c);
  variant/qemu_mouse.cpp:26 + qemu_variant.h:94 (qemu_gui_exec.cpp); qemu_fbblit.h:14
  (qemu_windowsrv.h). No rule, prerequisite or include names a retired path; no services/ remains.

## G12 coverage
Sweep groups are only r1_fs, winsrv/gui, svchost, portfilesrv. It does carry the winsrv/GUI and svchost
tokens (empty on this audit: 0 residue, 8+1 allowed comments), but no group covers e32base/e32cmn and
bare "winsrv"/"windowsrv" prose is deliberately untokenised, so that class stays grep-only. Its 28
residue findings are all r1_fs (Makefile:342-351, efsrv_min.cpp:30,32) — outside this chunk.
