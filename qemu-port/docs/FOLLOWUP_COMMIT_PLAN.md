# FOLLOWUP_COMMIT_PLAN — staging plan for the post-apply follow-up commit (2026-11-24)

Prepared read-only by a subagent. HEAD = `d26fade` (pushed). **No paths staged,
no commit/push performed by this document.** `kernelhwsrv/` untouched
(`git status --porcelain kernelhwsrv/` = empty at write time). No QEMU runs.

## 0. What this commit is

One follow-up commit containing **both** change sets that currently sit as
unstaged working-tree modifications on top of `d26fade`:

1. **Landed r1_fs kernel-glue removal** (verified: `R1Fs::` symbols 56→0, gates
   green; `docs/R1FS_REMOVAL_REVIEW.md`, `docs/R1FS_REMOVAL_STATUS.md`):
   `qemu-port/Makefile` (drops `build/r1_fs_core.o`/`build/r1_fs_fat.o` +
   rules), `qemu-port/variant/qemu_variant.h` (drops `R1Fs` decls),
   `qemu-port/variant/qemu_variant_asic.cpp` (Init3 mount block → direct
   `FatMount(4,0)`; `build/fat32.o` kept for genuine EFSRV).
2. **In-flight svcmgr apply** (agent-owned; per `docs/SVCMGR_RETIREMENT_PLAN.md`):
   `qemu-port/variant/e32load.cpp` (delete `SvcMgrLaunchExec`/`SvcMgrKillExec`/
   `SvcMgrCtlExec` ops 0/1/`SvcMgrBootLaunch`/`SvcMgrExecInstall`; add
   `PortTrapExecInstall()` keeping the exec-81 `FsRegisterThread` wrapper and
   slim exec 270 ops 2/3/4), `qemu-port/variant/qemu_variant.h` (decls →
   `PortTrapExecInstall()`), `qemu-port/variant/qemu_variant_asic.cpp` (call
   site `:171`), `qemu-port/user/user.mk` (drop `SVCMGR.E32`/`PORTFILESRV.E32`
   targets+rules), and deletion of `qemu-port/user/svcmgr.cpp` (when the apply
   agent lands it — at write time the file still exists, 14059 bytes).

**Shared files:** `Makefile`, `qemu_variant.h`, `qemu_variant_asic.cpp`,
`user.mk` carry BOTH change sets. They must be staged **only from the
post-apply state** — never mid-edit — after re-reviewing
`git diff -- <those paths>` confirms the diff contains exactly these two
intended change sets and nothing unrelated.

**Already staged by the in-flight deletions+docs agent (NOT ours — leave
alone):** the 3 portfilesrv deletions (`qemu-port/:portfilesrv`,
`qemu-port/tools/mke32_portfilesrv.py`, `qemu-port/user/portfilesrv.cpp`) and
the `docs/` additions (`CLEANUP_*`, `R1FS_*`, `SVCMGR_*`,
`DANGLING_REFS_SWEEP.md`, etc.). This follow-up runs only after that commit
lands and the index is clean relative to the new HEAD.

## 1. Pre-conditions (ALL must hold before any staging command)

0. The in-flight deletions+docs commit has landed (check `git log --oneline -1`);
   `git status --short` shows only unstaged entries (groups F + follow-up set).
1. svcmgr apply COMPLETE: `user/svcmgr.cpp` deleted (or explicitly deferred —
   then exclude it from this commit), `SvcMgr*` symbols absent from
   `variant/e32load.cpp`/`qemu_variant.h`/`qemu_variant_asic.cpp`
   (`grep -n SvcMgr qemu-port/variant/*.cpp qemu-port/variant/*.h` → comments only),
   `PortTrapExecInstall()` live at `e32load.cpp:1377`, called at
   `qemu_variant_asic.cpp:171`, declared `qemu_variant.h:401`.
2. Clean build post-apply: `make kernfull` and `make isogrub` succeed;
   `nm build/kernfull.elf | grep -c "R1Fs::"` = 0; no `SvcMgr` symbols.
3. Gates, run from `qemu-port/`, all PASS on the post-apply tree:
   - `python3 tools/r1_os_selfcheck.py` — PASS
   - `python3 tools/r1_qemu_smoke.py` — PASS (no Exc/PanicKernExec)
   - `python3 tools/r1_genuine_retire_test.py` — 4/4 (EFSRV/EWSRV/ESTART/EUSER)
   - `python3 tools/r1_genuine_fullchain_test.py` — 14/14
   - `python3 tools/r1_genuine_pointer_test.py` — PASS with `PTRBRIDGE:
     TRawEvent::Pos` line and no FAULT (proves exec 270 op 4 survived)
   - `r1_genuine_efsrv_test.py`, `r1_genuine_ewsrv_test.py`,
     `r1_genuine_eshell_test.py`, `r1_genuine_fat_io_test.py` — PASS
   - `python3 tools/r1_final_verify.py --strict-skip` — 11/11 (proves DOOM
     fbblit ops 2/3 + KERN-4-free EFILE teardown survived)
4. Negative checks on the newest logs: **no `KERN 4` / `ESystemThreadPanic`**
   anywhere (exec-81 wrapper live; `FsRegisterThread wrapper installed (slot
   81)` still prints); never accept `SessionPath r=-15` as IPC success; no boot
   path reads `\SVCMGR.E32` (check `tools/mkgrubiso.py` staging).
5. Dangling-refs sweep **re-run CLEAN after the svcmgr deletion** (the existing
   `docs/DANGLING_REFS_SWEEP.md` covers portfilesrv/r1_fs only): `make -n
   userland`, `make -n kernfull`, `make -n isogrub` exit 0 with zero hits on
   `SVCMGR`/`svcmgr.cpp`/`SvcMgr`/`execs 268/269`; accepted soft-fail of the six
   non-gate harnesses (`r1_desktop/guimenu/applaunch/lua/ported_software/
   desktop_handoff/cp4_ca_stress` tests) per plan §6 is expected and documented.
6. `git status --porcelain -- kernelhwsrv/` empty.
7. Explicit user authorization for the commit (and push, separately).

## 2. Exact staging commands (repo root; explicit paths only — NEVER `git add -A`)

```sh
# sanity: index clean vs HEAD (only the landed deletions+docs commit is HEAD)
git status --short

# follow-up code set — post-apply state ONLY (see §0 shared-files rule)
git add -- qemu-port/Makefile \
          qemu-port/variant/qemu_variant.h \
          qemu-port/variant/qemu_variant_asic.cpp \
          qemu-port/variant/e32load.cpp \
          qemu-port/user/user.mk

# svcmgr image deletion — only if the apply agent landed it:
git rm -- qemu-port/user/svcmgr.cpp

# optional: follow-up docs left unstaged by the other agent, if any
# (e.g. updated SVCMGR_APPLY_STATUS.md / CLEANUP_LOG.md / REMAINING_SHIMS_AUDIT.md
#  / GENUINE_PROGRESS.md / this FOLLOWUP_COMMIT_PLAN.md) — explicit paths only:
# git add -- qemu-port/docs/FOLLOWUP_COMMIT_PLAN.md ...

# verify: staged set == exactly the paths above; nothing from §3
git status --short
# dry-run equivalent of any command: append --dry-run
```

## 3. Do-NOT-stage list (never staged, never `git add -A`)

- `AGENTS.md` (root) — project instructions, out of scope.
- `qemu-port/.sybian/coordinator/state.json` — coordinator runtime state.
- `qemu-port/.sybian/qemu-slot.lock` — untracked lock, NOT gitignored.
- `symbian-x86-archive-56fbaff.tar.gz` — preservation tarball, NOT gitignored.
- `qemu-port/build/` — gitignored artifacts (verify via `git status --ignored`).
- `kernelhwsrv/` — READ-ONLY; no staging command may touch it.
- Anything staged by the in-flight deletions+docs agent that has not yet been
  committed (portfilesrv deletions + docs) — leave the index entry alone.
- The five follow-up paths while the svcmgr apply is still in flight.

## 4. Suggested commit message

```
Retire svcmgr and remove r1_fs kernel glue (follow-up to d26fade)

- r1_fs glue removal (landed+verified): Makefile drops r1_fs_core.o/
  r1_fs_fat.o kernel objects/rules; qemu_variant.h drops R1Fs decls;
  Asic::Init3 calls FatMount(4,0) directly (R1Fs:: symbols 56->0;
  build/fat32.o adapter kept for genuine EFSRV).
- svcmgr retirement: delete user/svcmgr.cpp and the SVCMGR.E32/
  PORTFILESRV.E32 targets/rules in user.mk; e32load.cpp removes
  SvcMgrLaunch/Kill/Ctl(execs 268/269)/Boot/ExecInstall and installs
  PortTrapExecInstall() instead — keeping the sanctioned port traps:
  the exec-81 FsRegisterThread wrapper (no KERN 4 on EFILE teardown)
  and slim exec 270 ops 2/3/4 (fbblit for DOOM + PTRINJ sink).
- Gates post-apply: selfcheck/smoke PASS, retire 4/4, fullchain 14/14,
  pointer test PASS (PTRBRIDGE), efsrv/ewsrv/eshell/fat_io PASS,
  r1_final_verify --strict-skip 11/11; dangling-refs sweep CLEAN.
  Accepted: six non-gate harnesses soft-fail (execs 268/269 consumers).
  Genuine EFSRV remains the sole file server. kernelhwsrv/ untouched.
```

## 5. Rollback

- **Snapshots:** `/tmp/opencode/svcmgr_retire_backup_20261124_185621/`
  (MANIFEST.sha256 + pre-apply copies of `e32load.cpp`, `qemu_variant_asic.cpp`,
  `qemu_variant.h`, `user.mk`, `svcmgr.cpp`);
  `/tmp/opencode/shim_retire_backup_20261124_083011` and `..._083050`
  (file-server cut, token `ROLLBACK-PORTFILESRV-20261124_083011`).
  Restore = copy the affected files back from the snapshot.
- **Git (pre-apply baseline d26fade), per path:**
  `git show d26fade:qemu-port/variant/e32load.cpp`,
  `git show d26fade:qemu-port/variant/qemu_variant.h`,
  `git show d26fade:qemu-port/variant/qemu_variant_asic.cpp`,
  `git show d26fade:qemu-port/Makefile`,
  `git show d26fade:qemu-port/user/user.mk`,
  `git show d26fade:qemu-port/user/svcmgr.cpp`.
- **Post-commit:** `git revert <sha>`; pre-commit, `git reset` unstages (working
  tree untouched).
- Preservation points remain: branch `archive/handwritten-r25` @ `56fbaff` +
  `symbian-x86-archive-56fbaff.tar.gz`.

## 6. Blockers / caveats

- svcmgr apply still in flight at plan time → **do not stage**; re-run the
  §1 diff re-review when the apply agent reports done.
- Exec 270 is shared (fbblit 2/3 DOOM + PTRINJ 4): any staging of a state
  where op 4 or ops 2/3 are missing is invalid — pointer test + 11/11 gate are
  the proof.
- Never re-add the EFILE trampoline (EFILE single-owner decision); the exec-81
  flag-clear wrapper is the sanctioned shim and stays.
- Run real staging only when no other git process holds `.git/index.lock`.
- No commit/push without explicit user authorization; no `make clean`;
  `kernelhwsrv/` read-only; no QEMU runs by the planner.

2026-11-26 supersession note: the 11-row / 10-11-pass gate described above is historical. Current authority is 15 rows / FINAL27_RC=0 (15 pass, 0 skip, 0 fail, /tmp/opencode/final27_gate.log).
