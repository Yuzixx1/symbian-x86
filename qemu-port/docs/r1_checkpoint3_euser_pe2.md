# R1 Checkpoint 3 — Genuine EUSER/CBase loading + PE2 import resolution

Status: PARTIAL (loading + binding verified; first user instruction faults — see §6)
Date: 2026-11-17
Files of record: `variant/e32load.cpp`, `variant/qemu_variant.h` (TE32Info),
`tools/e32read.py`, `tools/r1_qemu_smoke.py`, log `/tmp/opencode/r1_qemu_smoke.log`.

## 1. Scope

Checkpoint 3 means: the loader no longer fakes anything at the image level.
A real `EUSER.DLL` produced by the genuine Symbian toolchain is validated,
relocated, and made resident; a real PE2-format EXE (`portfilesrv`) is
validated, created as a ring-3 process, and its imports are bound by
1-based ordinal against the resident EUSER export table. What is still
faked: nothing in the bind path, but the user thread faults before its
first meaningful instruction (§6).

## 2. What is now genuine

| Artefact | Evidence | Where in code |
|---|---|---|
| Genuine EUSER.DLL validates, `r=0` | log: `E32LoadDll: E32Validate r=0 uid1=10000079 expCount=2503 codeRelocOff=00063b18` | `e32load.cpp:570` (`E32LoadDll` → `E32Validate`) |
| Genuine EUSER resident, 2503 exports | log: `e32run: EUSER.DLL resident code=217908 data=0 exports=2503` | `e32load.cpp:600-605` (export table snapshot into `sDllExp`) |
| Genuine PE2 EXE (`portfilesrv`) validates and creates | log: `ProcessCreate r=0 run=2200000`, `Loaded r=0` | `e32load.cpp:673-843` (`E32Run`) |
| Imports bound by real ordinals | log: `bind slot 672 ord 1221 -> 002562c8`, `bind slot 676 ord 1953 -> 0025994a` | `e32load.cpp:810-826` |
| Ordinals are real kernel exports | `kernelhwsrv/kernel/eka/bx86gcc/euseru.def` line 1222: `UserSvr::FsRegisterThread @ 1221`; line 1954: `memcpy @ 1953` | read-only upstream tree |
| Automated proof | `tools/r1_qemu_smoke.py` — 10/10 markers PASS | `MARKERS`, lines 41-52 |

EUSER.DLL header facts (dumped from the 412 104-byte file at
`~/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll`):
`uid1=0x10000079 uid3=0x100039e5 flags=0x22005101` (bit `0x20000000` = PE2
import format), `codeOff=200 codeSize=408144 dataSize=0 bssSize=0
textSize=398132 expOff=398332 expCount=2503 codeRelocOff=0x63b18
expDescSize=45 expDescType=2`.

## 3. PE2 import resolution — the algorithm

1. `E32Validate` reads the import section once and records, in `TE32Info`
   (`qemu_variant.h:167-185`, fields `iImpOff/iImpN/iPe2/iSlots[4]`):
   - `iPe2 = (flags & 0x20000000) != 0` (`e32load.cpp:423`).
   - PE2: the section holds only a mangled DLL name
     (`euser{000a0000}[100039e5].dll`); any non-empty name ending
     `".dll\0"` is accepted (`e32load.cpp:461-471`). The IAT lives at the
     end of .text: slot `k` sits at file code offset `textSize + 4*k`
     (`e32load.cpp:437-444`), pre-linked in the file with a 1-based
     ordinal that must be `1..65535` or validation fails.
   - ELF fallback (non-PE2): slot offsets come from the section itself,
     must be 4-aligned, inside `[textSize, codeSize-4]`, non-aliased;
     ordinals are positional (slot `k` → ordinal `k+1`)
     (`e32load.cpp:446-485`).
2. `E32Run` (`e32load.cpp:670`; note: the loader entry is `E32Run`, not
   `E32RunProcess`) loads EUSER once via `E32LoadDll` before creating the
   process, so a missing export fails fast (`e32load.cpp:686-695`).
   `sDllExp[4096]` caps at 4096 exports; EUSER's 2503 fit.
3. Binding (`e32load.cpp:810-826`): for each of the `iImpN` slots, PE2
   reads the 1-based ordinal from the file (`Rd32(file + codeOff +
   slot)`), ELF uses `k+1`; out-of-range ordinals log `ordinal %d out of
   range` and are skipped, not fatal. Then
   `*(u32*)(codeRun + slot) = sDllExp[ord-1]`.

Exact numbers for `portfilesrv` (fixture: 884-byte file, `codeSize=684`,
`textSize=672`, `codeRelocOff=0`, `codeBase=0x400000`):

| Slot (code offset) | File ordinal | Symbol | Bound address |
|---|---|---|---|
| 672 | 1221 | `UserSvr::FsRegisterThread` | `002562c8` |
| 676 | 1953 | `memcpy` | `0025994a` |

`codeRun` was `0x2200000` (`codeDelta = 0x1e00000`), so the two bound
targets are genuine EUSER code inside the relocated DLL image.

Fallback chain if EUSER.DLL is absent/corrupt: `E32LoadDll` may fail with
`KErrNotFound` only when `iImpN == 2`; those two slots then receive the
kernel trampolines `sTrampExit` / `sTrampPutc` (`e32load.cpp:691,830-836`).

## 4. Export-description validation (e32read-equivalent rule)

`E32Validate` now reproduces `tools/e32read.py` (lines 244-277) exactly on
the export-description tail, at `e32load.cpp:496-530`:

- Tail rule: `152 + ((expDescSize + 2 + 1 + 3) & ~3) == codeOff` (field at
  packed offset 152; header size is `codeOff`). EUSER: `152 + ((45+6)&~3)
  = 152 + 48 = 200` ✔.
- Type 0x00 = NoHoles: nothing more to check.
- Type 0x01 = full bitmap: `((expCount + 7) >> 3) == expDescSize`.
- Type 0x02 = sparse bitmap8 (EUSER's type): `bitmapSize = (expCount+7)>>3`,
  `metaSize = (bitmapSize+7)>>3`; require `metaSize <= expDescSize` and
  `155 + metaSize <= codeOff`; the total starts at `metaSize` and gains
  one per set bit in the `metaSize` bytes at offset 155, and must equal
  `expDescSize` exactly. EUSER: expCount 2503 → bitmapSize 313 →
  metaSize 40 → 40 + 5 set meta bits = 45 = expDescSize ✔.
- Any other type: `KErrNotSupported`.

One deliberate divergence (not a bug, record so nobody "fixes" it): the
standalone `e32read.py` rejects EUSER.DLL with `code relocation page/block
invalid` because EUSER's code-reloc section extends slightly past EOF
(toolchain padding). The kernel's `ValidateRelocs` clamps the section to
the available bytes and accepts it (`e32load.cpp:179-195`). The expDesc
tail rule itself is identical in both.

## 5. How to reproduce

```
python3 tools/r1_qemu_smoke.py            # builds disk, boots, checks 10 markers
python3 tools/r1_qemu_smoke.py --no-rebuild-disk --log /tmp/opencode/r1_qemu_smoke.log
```

- Disk: 32 MB raw image; MBR with one bootable type-0x0C partition, start
  LBA 2048, 16384 sectors; a standalone FAT32 image (`mkfs.fat -F 32`)
  sized to the partition is embedded at byte offset 1 048 576; files
  staged with `mcopy`: `portfilesrv`
  (`~/.cache/sybian-r1/r1-portfilesrv-e32-v1/output/portfilesrv`) and
  `EUSER.DLL` (`~/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll`).
- QEMU: `qemu-system-i386 -M q35 -m 128 -display none -serial stdio
  -kernel build/kernfull.elf -drive file=/tmp/opencode/r1_test.img,
  format=raw,snapshot=on,if=none,id=d0 -device ich9-ahci,id=sata
  -device ide-hd,drive=d0,bus=sata.0 -monitor none -no-reboot`,
  capped by `timeout 50`.
- Serial feed (timed, via bash sleeps): leading newline → `mount` →
  `portfilesrv`.
- Exit 0 only if all 10 markers pass (see `MARKERS`, `r1_qemu_smoke.py:41-52`);
  the serial log lands at `/tmp/opencode/r1_qemu_smoke.log`. Last run:
  10/10 PASS.

## 6. Known defects / open items

- **Exc 06 after `Resume()` — known open defect, fix in progress.** The
  user thread traps immediately:
  `Exc 06 EFLAGS=00051202 FAR=0 ErrCode=0 EAX=00402000 EBX=00000003 ...
  EIP=00000003`, printed between `e32run: Resume()...` and
  `e32run: resumed` (the kernel survives and still prints
  `portfilesrv started (user process)`). Root cause identified: the
  fixture image bakes in absolute addresses from the original PE —
  verified in the binary, `.rdata` value `0x402000` sits at code offsets
  10 and 90, IAT thunks `0x403040`/`0x40303c` at offsets 530/546 — but
  carries `codeRelocOff=0` (no relocations) and only 684 bytes of code,
  so after the code is relocated by `codeDelta` the stale pointers point
  outside the image (`0x2200000..0x22002ac`). Another agent is fixing
  this; do not assume it is fixed until the Exc 06 marker disappears.
- **Heap capacity note.** `Kern::Alloc`-ing ~408 144 bytes for the EUSER
  code destabilised the hybrid heap, so `E32LoadDll` relocates EUSER code
  in place inside the static 512 KB buffer `sDllFile` (comment at
  `e32load.cpp:576-583`); data/BSS would still come from the kernel heap,
  but EUSER has `dataSize=0 bssSize=0` so `dataDelta=0` today. A real
  per-process heap story is checkpoint 4 territory.
- **`e32read.py` vs kernel reloc walk** differ on past-EOF padding (§4);
  intentional, but worth keeping in mind when cross-checking images.
- **Out-of-range ordinals are skipped, not fatal** (`e32load.cpp:817-822`);
  fine for the pilot, will need policy before real F32 exists.

## 7. What checkpoint 4 (active scheduler / CActive) will require

- The Exc 06 above must be gone first: nothing in EUSER's user-side init
  runs today, and CActiveScheduler init lives behind that entry path.
- Verify the bound calls actually work end-to-end, not just that the
  addresses are bound: `UserSvr::FsRegisterThread` (ord 1221) must reach
  the kernel-side F32/PortFile registration path and return.
- Thread-local storage: genuine CActiveScheduler keeps its state in
  thread TLS; the port's DThread TLS plumbing needs to be assessed and
  wired against the real EUSER expectations (currently untested with a
  non-faulting user thread).
- Exit/abandon semantics: `CActiveScheduler::Start` blocks in the thread;
  the existing exit-reason ring (`sExits`, `e32load.cpp:620-669`) must
  carry scheduler-stop and panics out to the shell's `wait`.
- Import surface grows: the pilot binds exactly 2 ordinals; CActive pulls
  in more EUSER exports — `sDllExp[4096]` is big enough, but the 2-orinal
  trampoline fallback must not silently mask real DLL load failures once
  more ordinals matter.
- A fixture exercising CActive (built by the genuine toolchain, PE2, no
  ROM rebuild) plus new smoke markers — the 10-marker set proves loading
  only, and must not be weakened to pass while Exc 06 persists.
- Re-check the heap note in §6: CActiveScheduler allocates at thread
  start; the hybrid-heap ceiling that forced in-place EUSER relocation
  must be re-measured before any kernel-heap allocation is added to the
  user-side path.
