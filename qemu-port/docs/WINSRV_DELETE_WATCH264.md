# WINSRV_DELETE_WATCH264 — steady-state monitor

- origin/main HEAD: `94ce9c9` — winsrv/GUI desktop retired (winsrv.c, guishost.h, wsclient.*, qemu_windowsrv.*, qemu_gui_exec.cpp deleted; gates PASS).
- `git status --short kernelhwsrv/` → 0 entries: kernelhwsrv/ untouched (read-only honored).
- Running procs matching r1_final_verify / r1_winsrv_delete / r1_qemu_smoke → 0: no verification or delete job in flight.
- State: steady. Console shell + DOOM/Freedoom remain the UI; genuine EWSRV stays the target when GUI returns.
