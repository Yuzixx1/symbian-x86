# R1-STARTUP-PATHS — Start-up path mapping (research note, no code)

Card: **R1-STARTUP-PATHS** (owner-directed; not on the `parallel_work.md`
board). Tier: research-only. Base review performed against the current
workspace tree at commit `fa62323` + uncommitted changes; upstream evidence
from read-only `kernelhwsrv/` (oss.FCL.sf.os.kernelhwsrv checkout, paths
given below). **No code or tests were changed**; deliverable is this note.

Scope: map every path a program takes from "typed at the shell" to "running
user code with its command line", including error/failure exits, and identify
what contracts R1 work (R1-ABI / R1-FS / R1-EUSER / R1-LOADER / R1-PROOF)
inherits unchanged and where each card touches this path.

## 1. The normal start-up chain (typed name → running program)

### Step 1 — Shell line parse and implicit execution

`variant/kern_shell.cpp`:

- `Exec()` (line 556) keeps the raw line verbatim in `original` *before* the
  argv tokenizer inserts NULs (line 559–562), because the tokenizer destroys
  the tail but the program's command line must be character-exact.
- The tail after token 1 becomes `execTail` (lines 590–597, "DOS-style
  implicit execution"); typed names run with **no EXEC verb** and get `".E32"`
  appended when extension-less (`RunImage`, lines 511–529, documented in
  `manual_test_order.md` step 14).
- Dispatch to `RunImage(progName, execTail)` at line 1297.

First-token behavior worth knowing for R1 tests: tokens are lowercased for
command dispatch (line 609–610) but the raw `progName` keeps case for
execution; double-quoted tokens group spaces (lines 574–583); the tokenizer
has an 8-word argv limit (line 566) but the exec tail is raw/verbatim, so the
limit only affects shell built-ins, not program command lines.

### Step 2 — Image fetch from mounted volume

`RunImage` (`kern_shell.cpp:511`) then:

- resolves a volume prefix via `VolPrefix` and reads the raw image with
  `FatReadPath(in, eBuf, sizeof(eBuf), got)` into a **single static 64 KiB
  buffer** (`static TUint8 eBuf[65536]`, line 513). This is the hard file-size
  ceiling for executables today (~64 KiB total, directly inside the
  PortFile 64 KiB snapshot contract; see `manual_test_order.md` Boundaries).
- candidate order: name as typed, then name + `.E32` (first hit wins,
  lines 534–542).
- *before creating the process* runs `E32Reap()` (line 545): post-mortem
  code-seg cleanup piggybacks on exec start, so reaping cadence is tied to
  launch attempts, not a background cleaner.
- Not-found prints `"%s: not found"` and the shell returns to its prompt.
  No cache: every launch re-reads the image from disk.

### Step 3 — Validation, pre-registration and attrition-free creation

`E32Run()` in `variant/e32load.cpp:607`. Order of checks (each returns a
typed error to the shell, nothing half-created survives):

1. `E32Validate` (line 611) — header, CRC, sections, sizes into `TE32Info`.
2. UID1 gate (line 615): must be `0x1000007a` — *"EXEC must never start a
   DLL as a process"* (`KErrNotSupported`).
3. Capacity pre-check (line 620): both per-process tables must have room —
   `sProcs[16]` (per-process CWD, `variant/userfiles.cpp:17`) and
   `sExecs[16]` (code-seg reaper records, `e32load.cpp:849`) — else
   `KErrOverflow`. Comment at 617–619: *"Failing before ProcessCreate avoids
   untracked processes and leaked segs."* A 17th concurrent user process or
   17th non-reaped exec record fails to start, cleanly.
4. Import pre-resolution (lines 623–631): `E32LoadDll()` loads
   `EUSER.DLL` (`::EUSER.DLL` import) and captures its export table
   (`sDllExp`, positional ELF order = ordinal). Failure semantics: a
   missing EUSER is tolerated *only if* the app imports exactly 2 symbols
   (`dllR == KErrNotFound && info.iImpN == 2`, line 627 — Exit/PutC, the
   trampoline fallback); any other import-count mismatch is
   `KErrNotSupported`. This means **the loader resolves every import before
   process creation**, so no stillborn process on missing exports.
5. Name/extension split (lines 632–641) into `TProcessCreateInfo` fields —
   `iRootNameOffset`, `iRootNameLength`, `iExtOffset` are set explicitly
   (649–653), matching the genuine layout (`kernelhwsrv/kernel/eka/kernel/
   sprocess.cpp` `Create` consumes all three).
6. Priority normalization (lines 670–681): header value must be one of the
   eight genuine priorities (150…950) to pass through; anything else falls
   back to `EPriorityForeground` *instead of* rejecting — so a garbage
   priority cannot kill exec.
7. Command line: the raw tail is up-cast to **UTF-16 bytes in an HBuf**
   (lines 689–706, "the length executive divides by two"), capped at 256
   chars. This byte-wise widening is what later readback sees via
   `exec2(23, ...)` (see step 6 below).

Then, under kernel critical section (`NKern::ThreadEnterCS`, line 686):
`Kern::ProcessCreate(proc, pinfo, cmd, &h)` (line 709). On failure, the
command-line HBuf is deleted locally (line 710–711: "ownership never
transferred") — a genuine owner-transfer contract.

### Step 4 — What the genuine kernel does at ProcessCreate

Upstream `kernelhwsrv/kernel/eka/kernel/sprocess.cpp`, `DProcess::Create`:

- `iTempCodeSeg` comes from `aInfo.iHandle` when supplied by a caller who
  already loaded a code seg (lines 212–221), else `M::NewCodeSeg` + `Create`
  (222–233). The port supplies nothing here (RAM-loaded model) — see §3.
- If the caller did not pass a first thread (`aKernelProcess` false), the
  main thread is created with (lines 236–251):
  - `t.iFunction = aInfo.iFileEntryPoint` — *"kernel will call veneer, veneer
    will call this"* (line 239): the kernel's thread-entry mechanism jumps
    via `iEntryPtVeneer` after the loader may have substituted a veneer.
  - flags `KThreadFlagProcessPermanent|KThreadFlagOriginal` (253),
    thread name `KLitMain` (246), priority `EThrdPriorityNormal` (245),
    user stack `iStackSize` (244), heap min/max from the header (248–249).
- `iUserThreadsRunning = 1` (254) is set immediately at creation.

Note for R1-LOADER: on this path the kernel creates thread + heap + stack,
while the **port copies sections and binds imports afterwards** (step 5).
Upstream F32 splits this differently (a loaded-then-copied code seg via
loader server, `exstart.cpp:110` traces "FS entrypoint"). The port's
`pinfo.iCodeLoadAddress = 0` comment — *"RAM-loaded: kernel allocates"*
(e32load.cpp:666) — plus post-create `memcpy` (716–738) is the deliberate
deviation from upstream that R1-PROOF must not "fix" invisibly.

### Step 5 — Sections in, imports bound, Loaded/Resume

`E32Run` continues (all after `ProcessCreate` succeeded):

- code copied to `pinfo.iCodeRunAddress`; data run: initialized part + zero
  BSS (715–730, mirroring `CreateDataBssStackArea`'s layout comment).
- `ApplyRelocs` for code and data relocation sections (731–738; negative
  check regressions in `tools/test_e32_relocs.py`, ring-3 verify in
  `test_data_reloc.py`).
- Import binding (739–757): **positional** ordinals (ELF list order =
  ordinal) patched into IAT slots baked into the E32 image by
  `tools/mke32.py` (e.g. `test_args` IAT layout: Exit@64, PutC@68,
  CmdLen@72 — `mke32.py:467`). Rule: if EUSER.DLL loaded with enough
  exports → real DLL export addresses; else ordinals 1–2 fall back to
  kernel trampolines `sTrampExit/sTrampPutc` (e32load.cpp:752–756, declared
  at line 47 as *"both stubs are position-independent"*, direct-model
  kernel-heap RAM is user-executable at line 433). **Contract turn-around
  point for R1-ABI/EUSER**: the ordinal numbering, IAT slot placement and
  cdecl argument passing on INT 0x21 are exactly what an unmodified
  upstream-built binary cannot reproduce (it expects EUSER entry code, not
  IAT writes). e32load.cpp:743's comment — *"Prefer the resident EUSER.DLL
  file; ordinals 1-2 fall back to kernel trampolines"* — is a port-local
  compatibility assumption to verify against genuine code (currently the
  DLL's own entry is never called: `mke32.py:601` *"DLL entry: ret (loader
  does not call it yet)"*).
- `proc->Loaded(pinfo)` (764) — attaches/maps the seg, runs per-process
  attach. Then `ProcExtCapture(proc)` (773): CWD inheritance snapshot.
- `proc->Resume()` under `NKern::LockSystem` (778–781); the launching
  handle is released immediately (784 — *"the process lives on its threads;
  when they die with no handles left, the object is destroyed (no
  zombies)"*), and the `(process, code seg, basename)` triple is remembered
  for the post-mortem reaper (788, `ExecRecRemember`, 858).
- Shell feedback strings: `"%s started (user process)"` on success,
  `"%s (%s)"` + error name on failure (`kern_shell.cpp:549–552`).

### Step 6 — First thread runs, user side up

- Thread start: upstream mechanism calls the entry (via veneer comment on
  `sprocess.cpp:239`); the port's images are built by `tools/mke32.py` /
  `buildapp.py` with entry `_start` in `user/start.S:1–7`:
  `call app_main → push %eax → process_exit → ud2`. **`app_main` is the
  port's application entry convention** (SDK: `sdk/examples/hello/main.c`).
  It receives **no arguments**; the command line is *fetched*, not passed:
  `command_line()` in `user/runtime.c:280` reads length via
  `exec2(82, 0xffff8000, ...)` then copies via `exec2(23, ...)`
  (EExecProcessCommandLine against handle `0xffff8000 = current process`),
  expecting UTF-16 bytes and dividing the descriptor length by 2 (289) —
  consistent with the byte-wise widening in step 3.
- Exit: `process_exit` (`start.S:67–74`) → exec 115
  (`EExecProcessExit?`/Exit on self, category length 0). Return value =
  exit reason, visible in shell `EXITS` (all documented in SDK docs).
- First executive available to user code: INT 0x20 (`ipc_wait_any`) and
  INT 0x21 gated executive calls (`user/start.S`, execs numbered per
  `inc/` + upstream `execs.txt`). Timer caveat, directly relevant to
  R1-EUSER scheduling: `User::After`-style waits use **fresh RTimer
  objects per call** because *"the thread-embedded iTimer … does not
  complete twice in this port"* (`user/runtime.c:25–44` comment). This is a
  documented startup-path-adjacent deviation, not a hardware quirk.

### Step 7 — CWD/namespace inheritance at start-up

`variant/userfiles.cpp`: per-process `TProcExt` in `sProcs[16]`.

- `ProcExtCapture` (line 32) runs *before resume*: inherits the **creator's
  CWD** if it has one, else the active volume's global CWD — comment
  (29–31): *"the shell holds locked across Exec"*, i.e. inheritance is the
  shell's chosen CWD, and user chdir leaks to neither shell nor volume
  globals (validated by `manual_test_order.md` step 6, `test_userland.py`).
- Relative-path resolution at open/exec time goes through
  `ProcExtResolve` (101) with documented rules: `X:\abs`, `X:rel`
  (CWD only when X is the process's current drive, 120–131), bare `\...`
  (process drive), pure relative (CWD + input, 169).
- Removal: `ProcExtRemove` on reap only (e32load.cpp:929) — a dead-but-
  unreaped process still owns its slot, which is what the 16-slot
  `ProcExtAvailable` pre-check in step 3 protects.

## 2. Failure paths along the chain (R1-LOADER surface)

In priority order, all currently *fail fast* (no half-created process):

| Failure | Where detected | Result |
| --- | --- | --- |
| Name not found / not readable | `FatReadPath`, both candidates | `not found`, no process |
| Bad header / CRC / truncated | `E32Validate` | typed error code |
| DLL given to exec | UID1 gate | `not supported` |
| Tables-full (procs/records) | pre-ProcessCreate `KErrOverflow` | clean fail |
| Missing EUSER imports ≠2 | import pre-resolution | clean fail |
| Import count > resolved exports | `sDllExpN < iImpN` check | `not supported` |
| ProcessCreate fails (stack/heap OOM etc.) | genuine `DProcess::Create` | HBuf freed, typed error |
| Crashes after start | genuinely-executed faults | reaper exit line naming the program + `EXITS` |

Known-unresolved hardening gap (already recorded in `manual_test_order.md`
Boundaries and `full_os_checklist.md`): loader *failure-path cleanup* after a
partial `ProcessCreate`/`Loaded` sequence, and the reaper *"only tracks
processes remembered at resume"* (`e32load.cpp`,`ExecRecRemember` skip path
at 876: table-full ≫ leaks that seg, documented). Both are stated cards'
targets (R1-LOADER), confirmed still accurate in current code.

## 3. Where each R1 card touches this path

- **R1-ABI** must freeze: (a) `TProcessCreateInfo` field-by-field, as filled
  at `e32load.cpp:649–682` vs upstream `sprocess.cpp` + `scodeseg.cpp:714–818`
  (`iEntryPtVeneer`/`iFileEntryPoint` pair that the port currently equates —
  `pinfo.iEntryPtVeneer = info.iEntry`); (b) UTF-16 command line in HBuf and
  the exec-23 length divisor-by-vs-`iLength` semantics; (c) priority value
  mapping; (d) IAT slot layout in `mke32.py`. Each has a QEMU fixture today
  that becomes the acceptance test once genuine-built binaries are compared.
- **R1-FS**: `RunImage` reads the image via the FAT kernel path and a static
  64 KiB buffer; a file server changes *where the bytes come from*, not the
  entry chain — but re-deciding the 64 KiB ceiling and "re-read per launch"
  policy is a startup-path decision, so R1-FS owns that contract jointly
  with the loader.
- **R1-EUSER**: two startup-owned contracts: (1) entry convention (`_start →
  app_main`, args fetched not passed, exit via exec 115) vs genuine EUSER's
  thread entry/`E32Startup`; (2) the dual-restart timer limitation in
  `sleep_us` as it interacts with active-object scheduling.
- **R1-LOADER**: owns steps 3–5 semantics (validate → preload imports →
  create → copy → bind → Loaded → Resume) and the failure table in §2, plus
  the `ExecRec`/reaper lifecycle.
- **R1-PROOF**: consumes everything above unmodified; the no-EXEC implicit
  launch plus `.E32` fallback is currently the *only* launch UI, and its
  fixtures (`test_args`, `test_dll`, `test_data_reloc`, `test_userland`) are
  the baseline startup regressions a future app must not regress.

## 4. Unknowns / explicitly unverified assumptions

1. The claim that genuine upstream *requires* `iEntryPtVeneer` to be a real
   veneer (with `Loaded()` substituting it) — the port passes the file entry
   to both fields. Evidence so far is port-side only; `scodeseg.cpp:714–716`
   shows the veneer default *"modified by loader if necessary"*. Needs an
   R1-ABI verdict with an upstream-built binary.
2. Whether the byte-widened (halfwidth) command line we store is bit-exact
   to what upstream EUSER reads via `RProcess::CommandLine` — the port's
   readback tests (`test_args`) prove our own convention only. Confidence:
   medium; genuine `execs.txt` #23 semantics confirm length-divides-by-two,
   which is the core of it.
3. `DProcess::Create` first-thread name `KLitMain` and priority values are
   upstream facts from `sprocess.cpp:245–246`; the port passes them through
   `mke32.py` header fields — verified by fixtures but not against an
   upstream-built consumer.

## 5. Handoff

```text
Card / base commit / owned branch: R1-STARTUP-PATHS / fa62323 + dirty tree /
  research-only
Contract revision and dependencies: none frozen; feeds R1-ABI contract
What researched (source evidence): variant/kern_shell.cpp:511-610,1297,
  variant/e32load.cpp:607-936, variant/userfiles.cpp:17-131,348-402,
  user/start.S:1-74, user/runtime.c:25-44,280-313, tools/mke32.py:382-638,
  sdk/examples/hello/main.c + sdk/tools/buildapp.py; upstream
  kernel/sprocess.cpp:212-260, scodeseg.cpp:468-1180, exstart.cpp:110
Tests run: none needed (read-only mapping; existing fixtures already cover
  the chain: test_e32_relocs/test_args/test_dll/test_data_reloc/
  test_userland/test_crash)
Manual step drafted/updated: none
Compatibility and failure limits: 64 KiB image buffer, 256-char cmdline,
  16-entry sProcs/sExecs tables, imports resolved pre-create (2-import
  trampoline fallback), no EXEC verb, implicit .E32 — all listed with
  citations in §1–3
Blocked assumptions: §4 items 1–3 need the R1-ABI contract freeze before
  R1-EUSER/R1-PROOF adopt an entry convention
Recommended integration order and next owner: hand to R1-ABI owner as
  startup-path input; R1-LOADER consumes §2 table; nobody should change
  kern_shell/e32load/user/start.S until the ABI freeze
```
