# R1-DLLTOOL-DIAGNOSTIC v0 — Wine dlltool "CreateProcess" spawn failure

Status: **research result, isolated observed cause, not frozen.** Host-only
diagnostic card per `r1_next_wave.md` #6. No target build, no E32 execution,
no QEMU, no upstream/tool patch, no commits, no system changes. All runs used a
**private** root/prefix created for this card; existing build roots, prefixes
and logs were read but never modified. No cleanup of any other worker's files.

## 0. Summary

The diagnose of `r1_clean_build_v1.md` §4 — `dlltool.exe: CreateProcess`
(dlltool could not spawn `as.exe`) under `Z:\home\yuzi\.cache\sybian-r1\...`,
working under `M:\bin` — is now **isolated at byte level**:

- dlltool (MinGW binutils **2.17.50**) locates `as` by probing
  **`<directory of the invoked dlltool.exe> + "as"`** with `CreateFileA`
  (read-open), and only falls back to extension probing when it decides the
  candidate has no extension.
- Its extension decision is a **plain `strchr(candidate, '.')` over the whole
  candidate path string**: if a dot occurs *anywhere* in it, it treats the name
  as already qualified, probes only the literal extensionless file, and gives
  up when that file does not exist — **before any Wine-level `CreateProcess`
  is ever attempted**. dlltool then prints its generic message
  `...: CreateProcess` and exits 1.
- `Z:\home\yuzi\.cache\...` contains a dot in `.cache`, so the candidate
  `Z:\home\yuzi\.cache\sybian-r1\r1-build-v1\tools\mingw\bin\as` was probed
  as-is, found absent, and dlltool aborted. The tree's real file is `as.exe`.
- The `M:` drive workaround **worked for a different reason than assumed**: a
  drive mapping prefixes everything below the mapped root, i.e. it *truncates
  the dotted part out of the invoked path*. `M:\bin\dlltool.exe` yields the
  dot-free candidate `M:\bin\as`, so dlltool appends suffixes and probes
  `M:\bin\as.com` (absent), then `M:\bin\as.exe` (**found**) → `CreateProcess(app M:\bin\as.exe)` → assembler runs. An explicitly mapped drive
  pointing at a **dotted relative subtree** fails identically
  (`N:\bas.eline\bin\dlltool.exe` → same `CreateProcess` error), which proves
  the drive letter is not the discriminator.

So the CLEAN-BUILD report's ".cache dot-directory" suspicion was correct in
direction but the *rule* is sharper: **any `.` in the as-candidate path breaks
the spawn; drives help only by removing it; `Z:` itself is innocent.** Neither
PATH, `WINEPATH`, `TMPDIR`, spaces, nor path length are involved (covered in
§6), and length has an unrelated separate launcher limit (§6.2).

## 1. Provenance

| Item | Value |
|---|---|
| Card | R1-DLLTOOL-DIAGNOSTIC (`r1_next_wave.md` #6), 2026-10-22 |
| Port repo base | `fa62323eea7bc96cefd666e40f5189567731bd44` (branch `main`, **dirty** with other cards' file; this card touched only its owned report) |
| Evidence sources (read-only) | `/home/yuzi/.cache/sybian-r1/r1-build-v1/logs/attempt1_dlltool_blocker/{sbs_euser.out,sbs_euser.err,sbs_euser.xml}`, post-fix wrapper `$R1/tools/x86bin/dlltool`, wrappers in `/tmp/opencode/r1-build-repro/tools/x86bin/`, `env.sh`, `docs/r1_clean_build_v1.md` §4, `docs/r1_research_reconciliation_v0.md` |
| Tools copied (byte-identical in both trees, sha256) | `dlltool.exe` `c5282b2af5cffcc420fc4fa1868f491bbdf20f4ec2be3408e0020634124ec0bc`, `as.exe` `15a80e86586b7e34a7fd155cc87a5a99fa3580be88e480d18ce0d941ec138351` (MinGW 3.4.5 tree, zip hash per CLEAN-BUILD §1) |
| Wine | wine-11.0 (Staging), same host as CLEAN-BUILD; fresh private prefix |
| Scratch root (home-backed) | `/home/yuzi/.cache/sybian-r1/r1-dlltool-diag-v0/` — `wineprefix/` (fresh `wineboot --init`, own prefix), `tools/` (copies), `tmp/` (TMPDIR/TEMP/TMP), `inputs/`, `logs/` (per-cell argv/stdout/stderr/env/rc/meta + `summary.txt`), `notes/`, `cell.sh` (driver, scratch-only, not committed) |
| Extra scratch | `/tmp/opencode/agent-r1-dlltool-diag/` (node/space/length/symlink test trees) |
| Inputs | real failing def: `euser.prep` sha256 `76130964e5da9f6321e9a7b41315e4273eba403cec462274bd4478f10783286e` (from `epoc32/build/euser/c_3217bb63a7562148/euser_lib/x86/urel/`), `rpipe.prep` `02f33d618e2f0702497222be9d9c941d522de1546616fd7dd1df6a324ae55e5d`, plus a 3-export `smoke.def` for bounded cells |
| Footprint | ~1.9 GB under the scratch root, ~224 MB under `/tmp/opencode/agent-r1-dlltool-diag/`; nothing in any other worker's area modified; no commits/pushes |

Command shape used for every cell unless varied (mirrors the failing Raptor
invocation recorded in `sbs_euser.xml`):

```text
wine <exe-Windows-path> -m i386 \
  --input-def Z:\...inputs\(<smoke.def|euser.prep>) \
  --dllname 'euser{000a0000}[100039e5].dll' \
  --output-lib Z:\...\<out>.lib
```

Environment fixed by default: `WINEPREFIX=<private prefix>`,
`WINEDEBUG=-all`, `TMPDIR=TMP=TEMP=<scratch>/tmp`, dlltool's own dir **not**
on `PATH` (mirrors the CLEAN-BUILD wrapper). Success criterion: rc 0 and a
non-0-byte output `.lib`. Every cell's stdout/stderr/env/rc is archived under
`logs/<cell>/`.

## 2. Cells and results

| # | Cell | Invocation path (Z-string length) | Result |
|---|---|---|---|
| 1A | `1A_z_cache` — baseline replication | `Z:\home\yuzi\.cache\...diag-v0\tools\baseline\bin\dlltool.exe` | **FAIL** rc 1, `CreateProcess` (reproduces the build failure) |
| 1B/1C | traces | same as 1A with `WINEDEBUG=+process` and `+relay` | FAIL; full API sequence captured (§5) |
| 2M | `2M_mdrive` — drive mapping (like CLEAN-BUILD) | `M:\bin\dlltool.exe` (mapped to the same dotted real dir) | **PASS** (lib 4106 B) |
| 3N | `3N_nodot_real` — real copy, no dot anywhere | `Z:\tmp\opencode\agent-r1-dlltool-diag\nodot\bin\dlltool.exe` | **PASS** |
| 4S | `4S_space_real` — space in path `spa ce` | `Z:\tmp\...agent-r1-dlltool-diag\spa ce\bin\dlltool.exe` | **PASS** |
| 5A | `5A_asfile` — dotted path, but extensionless `as` copy added to the bin dir | `Z:\tmp\...\asfile\bin\dlltool.exe` | **PASS** |
| 6N | `6N_noexe_noas` — no-dot copy with `as.exe` removed | `Z:\tmp\...\noexe\bin\dlltool.exe` | **FAIL** (candidate `...\bin\as.exe` absent) |
| 7W | `7W_winepath` — dotted path + `WINEPATH` to the mingw bin | `(1A argv)` | **FAIL** (set does not help) |
| 7X | `7X_noexe_winepath` — no-dot, no `as.exe`, + `WINEPATH` | `(6N argv)` | **FAIL** (no PATH fallback exists for such candidates) |
| 8L | `8L_long_nodot` — ~644-host-char no-dot path | very long | **FAIL rc 5 pre-launch** — *not* the dlltool error (§6.2) |
| 8S/8V | exe-path length sweep, short CWD | Z-string 240 / 252 / 257 / 260 / 264 vs 282 / 286 / 293 | PASS ≤ 264; **FAIL ≥ 282** (`ShellExecuteEx failed: File not found.` at Wine's launcher) |
| 8C | CWD sweep | CWD ~443 host chars, short exe | **PASS** (long CWD alone is not the blocker) |
| 8E | `8E_exe_len280` — 280-host-char exe path | FAIL with launcher message (as above) | **FAIL pre-launch** |
| 10A | `--as` override on the **dotted** tree | `--as Z:\...\baseline\bin\as.exe` (path contains dots, file exists) | **PASS** |
| 13T | `13T_notmp` — `TMPDIR/TEMP/TMP` unset | dotted path | **FAIL** (as prior report claim: TMPDIR not a factor) |
| 9O | old-tree control | original `/tmp/opencode/r1-build-repro/tools/mingw/bin/dlltool.exe` (byte-identical) with **my** prefix/input | **PASS** — matches CLEAN-BUILD §4 observation under controlled env |
| 14R/15R/16R | real-input fidelity (`euser.prep`, full Raptor argv incl. `-m i386`, versioned dllname) | dotted `Z:` — **FAIL**; `M:` — **PASS (2.37 s)**; no-dot `Z:` — **PASS** | M: and no-dot runs produced **byte-identical** `euser{000a0000}.lib`, 2 024 964 B, sha256 `1de6ddf9a850f62be858097fe7ef7d94b41c3e93d700703040361ba24cd4e4ba` (both runs) |
| 2R | relay trace of the **passing** M: run | `M:\bin\dlltool.exe`, `+relay` | probes `M:\bin\as.com`, then `M:\bin\as.exe` (§5) |
| 11N/11M | drive letter with dotted/clean **relative** subtree | `N:\bas.eline\bin\dlltool.exe` → **FAIL**; `N:\baseline\bin\dlltool.exe` → **PASS** | drive mapping is not privileged; only the dot-free *visible* path matters |
| 17S/18S | symlinks | `symlink_nodot` (clean name → no-dot tree) → **PASS**; `symlink.d` (dotted **name**, clean target) → **FAIL** | a dot in the *invoked path text* is enough; a symlink name does not hide it |

Excluded cells (disclosed): the very first driver attempt failed with rc 127
(exe called directly, not via `wine` — driver bug, then fixed and re-run as the
real 1A); a malformed intermediate long-path attempt (accidentally empty exe
argument, timed out at 120 s — never produced a named log, excluded); cell
`8C_cwd_long` rc 53 = broken symlink from an invalid setup, excluded;
`grad250/300/400/600` cells were built with intent lengths but the **actual**
Z-string lengths (144/172/220/322 host chars) are recorded in the cell logs —
only grad600 (322) fails, consistent with the ≥282 launcher limit.

## 3. Observed cause (isolated)

From `logs/1C_relay/stderr.txt` (dotted `Z:`, failing run, dlltool thread
`00e8`; same sequence in the original build failures):

```text
00e8:Call msvcrt.strchr("Z:\home\yuzi\.cache\...bin\as", '.')  → non-NULL (first dot = '.cache')
00e8:Call msvcrt.strchr(..., '/')  → NULL        00e8:Call msvcrt.strchr(..., '\\') → found
00e8:Call msvcrt.strcpy/strcat   → candidate "Z:\...\bin\as" kept verbatim (no ".exe" appended)
00e8:Call KERNEL32.CreateFileA("Z:\...\bin\as", GENERIC_READ, ...)  → NtCreateFile → status C0000034 (STATUS_OBJECT_NAME_NOT_FOUND)
     (a second equivalent probe via CreateFileW → again C0000034)
00e8: msvcrt._errno / strerror / vfprintf → dlltool prints "...: CreateProcess" → exit
— and, critically, ZERO CreateProcessTracking / CreateProcess* calls by dlltool:
   the failure happens before the process-creation API is ever reached.
```

From `logs/2R_m_relay/stderr.txt` (dot-free `M:`, passing run), the same code
path but the `strchr('.')` on `M:\bin\as` returns NULL, and the probe list
becomes:

```text
00e8:Call msvcrt.strcat("M:\bin\as", ".com") → CreateFileA("M:\bin\as.com") → not found
00e8:Call msvcrt.strcat("M:\bin\as", ".exe") → CreateFileA("M:\bin\as.exe") → FOUND
00e8:CreateProcessInternalW app L"M:\bin\as.exe" cmdline L"\"M:\bin\as\" \"-o\" ... \"-...\""  → as.exe started (twice, per temp file)
```

Supporting binary evidence (from my copy): `dlltool.exe` embeds libiberty's
Win32 spawner (`_win32_spawn`, `_pex_win32_exec_child`, `_pex_win32_wait`,
suffix-knob symbols `_std_suffixes`/`_no_suffixes`, a `.exe` string at file
offset 0x7ce31, and `TMPDIR`).

Labels: the candidate construction, the `strchr('.')`-before-probe order, the
`CreateFileA` probes, the `C0000034` status, and the absence of any
`CreateProcess` call are **direct byte/trace evidence**. Two inferences are
*labeled inference, not source-verified*:

- that this is libiberty `pex-win32` (2.17) behavior compiled in (`_win32_spawn` etc.): consistent, but no upstream source line was consulted in this card.
- how the candidate's directory prefix is derived (invoked path text vs
  `GetModuleFileName`): the dotted-**symlink-name** failure implies the invoked
  path text dominates; both mechanisms agree with the traces, and Wine's
  `GetModuleFileName` also returned the as-launched text in the relay.

Also relevant for future harness design: the assembler spawn command uses
**CWD-relative temp names** (`"M:\bin\as" -o dumh.o dumh.s`), i.e. temp files
are created in dlltool's current working directory; `TMPDIR` had no effect on
spawn success (13T).

## 4. Corrected picture vs prior claims

Confirmed: `.cache` dot directory was the trigger; `--as` override works;
old-tree path worked; drive mapping works; `WINEPATH` does not help; `TMPDIR`
irrelevant. Corrected: it is **not** "Z: fails / explicit drive works" —
relative dotted components under an explicitly mapped drive fail too (11N),
and dot-free `Z:` paths pass (3N, 9O). Not reproduced (contradiction flagged
for the coordinator): the earlier report's claim that a path *symlinked*
without the dot directory still failed; with a dot-free symlink *name* it
passes here (17S). A failing symlink case is plausible if the symlink path
text itself contained a dot (18S fails under that rule) or the invoked path
traversed a dotted real component; that claim should not be relied on further.

## 5. Negative results (and one unrelated finding)

1. **Spaces** in path: not a factor (4S).
2. **`PATH`/`WINEPATH`**: not a factor (1A/7W/7X; no PATH fallback exists for
   candidates containing `\`).
3. **`TMPDIR`/`TEMP`/`TMP`**: not a factor (13T).
4. **Drive letters**: not a factor themselves (11N vs 11M; 3N vs 2M pairs).
5. **Length (unrelated second finding)**: independently of the spawn rule, a
   dlltool **exe path** longer than the ~260-character Windows range fails at
   the Wine launcher *before* any dlltool code: Z-string lengths 180, 216,
   240, 250, 252, 257, 258, 260, 262 and 264 pass with the tool working;
   282, 286, 288, 292, 293, 322 and 644 fail on launch with either
   `Application could not be started, or no application associated with the
   specified file. / ShellExecuteEx failed: File not found.` or
   `wine: could not open working directory ...` (both pre-launch messages are
   in the respective cell logs). A long **current directory** alone (~443
   chars) passes. The CLEAN-BUILD paths (~60 chars) are far below any
   threshold; this only limits pathological future layouts, and one failed
   threshold boundary was narrowed to 264 < len ≤ 282 without pins further.
6. **Wineboot-sensitivity**: not observed; all cells shared one fresh prefix.

## 6. Workarounds (all evidence-backed, scratch-only in this card)

1. **Dos-drive mapping above the dotted part** (what the CLEAN-BUILD harness
   did): works, and produces **byte-identical** import libraries on real input
   (15R vs 16R, sha256 above). Any new wrapper must keep the mapped root above
   any dotted component (or the subtree below it dot-free).
2. **Extensionless `as` copy beside dlltool** (5A): add a copy of `as.exe`
   named literally `as` in the same bin dir. This lets the *unfixed* dotted
   path pass; zero wrapper changes after that.
3. **Dot-free install/invoked path** (3N/9O): any relative-free dotted-string
   location works (spaces are allowed too).
4. **`--as <path>\as.exe` override** (10A): works even on the dotted tree
   because the override value names an existing file; note the override is
   evaluated with the same probe logic, so it must point at an existing file
   (an extensionless nonexistent `--as` path would fail as in 6N).

No upstream, tool, prefix, or system bytes were changed anywhere; the pinned
v1 tools/prefixes were only used read-only.

## 7. Limits and unverified

- Exact upstream source file/line of the dot rule not consulted (behavior
  verified purely on this host via API traces).
- Native-Windows behavior of the same binaries in a dotted install path is
  *plausible-identical* by the same rule but was not tested (no Windows host).
- Threshold sweep for the launcher length limit is coarse (one open
  boundary between 264 and 282) and was not the purpose of the card.
- Single Wine version (11.0 Staging) as per the project; findings may differ
  on other Wine versions.
- `dlltool` from a *shell wrapper* executes with the wrapper's inherited
  environment; all cells replicated that shape.

## 8. Storage / integration notes

- Scratch (grep it for details; kept, not cleaned): make it a single source of
  truth — `/home/yuzi/.cache/sybian-r1/r1-dlltool-diag-v0/` (wineprefix, cell
  logs incl. `logs/summary.txt`, relay traces) and
  `/tmp/opencode/agent-r1-dlltool-diag/` (test trees). All runs strictly
  sequential (single Wine slot); no QEMU/test-slot occupation.
- This document is the only repo file created by this card.
- Coordinator note: this closes the "who investigates the dlltool spawn
  failure" assignment of `r1_clean_build_v1.md` §12; it does not block
  R1-LOADER-CONTRACT, R1-FS-DESIGN or R1-BUILD-REPEAT (card 5's explicit M:
  mapping stays valid per §6.1; its new root `.cache/sybian-r1/
  r1-build-v1-repeat` is fine as long as invoked tool paths are drive-mapped
  or dot-free above the mapping).

## 9. Handoff

```text
Card / base commit / owned branch: R1-DLLTOOL-DIAGNOSTIC; port base fa62323ea7bc96cefd666e40f5189567731bd44 (dirty, untouched); no new branch, no commit.
Contract revision and dependencies: none entered; feeds nothing frozen. Not a prerequisite for R1-BUILD-REPEAT (its mapping already satisfies the rule) or loader/FS contracts.
What changed (paths) or researched (source evidence): only qemu-port/docs/r1_dlltool_diagnostic_v0.md (this report). Research: byte-level isolation of the dlltool spawn rule (see §3, logs cited per cell).
Tests run (exact commands, result, log paths): bash cell.sh <name> <win-paths as in §2> per cell; results in /home/yuzi/.cache/sybian-r1/r1-dlltool-diag-v0/logs/summary.txt and per-cell logs; rc and evidence in §2/§5. No QEMU/suite/hardware.
Manual step drafted/updated: none (host-only diagnostic; nothing to verify on target PC).
Compatibility and failure limits: cause is MinGW binutils 2.17.50 behavior (dot-in-candidate → no .exe probing); a Wine version change is unlikely to change it, untested. Exe-path >~260 chars fails separately at the Wine launcher (§5.5).
Blocked assumptions or cross-card interface requests: source-line confirmation of the libiberty pex-win32 rule (optional follow-up); earlier "symlinked dot-free path still failed" claim from the previous wave is not reproduced here and should be dropped (or re-derived from that session's logs) per §4.
Recommended integration order and next owner: no code integration. R1-BUILD-REPEAT may proceed unchanged (keep the M: mapping or any dot-free invoked path). Coordinator may list the extensionless `as`-copy workaround as an accepted harness alternative; owner decision not required.
```
