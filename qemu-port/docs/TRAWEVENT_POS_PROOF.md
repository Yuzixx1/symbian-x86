# Direct guest proof: genuine `CEvent::RunL()` calls EUSER ord 1868 `TRawEvent::Pos()`

Verdict: **PROVEN**. A direct guest marker `PTRBRIDGE: TRawEvent::Pos x=80 y=40`
is emitted from inside genuine EWSRV's ord-1868 call, with the injected
coordinates, while EWSRV is processing the bridge's `EPointerMove` event.
`kernelhwsrv/` is untouched; genuine EWSRV/EUSER binaries and sources are
untouched; no commit/push; no `make clean`.

## What was added (port-side only, `variant/e32load.cpp`)

- A PE2 IAT rebind that is active **only** for the EWSRV process name AND when
  `\PTRINJ.TXT` is present on the boot FAT volume. All other boots and all other
  processes bind EUSER exports exactly as before. The genuine ord-1868 address
  is cached in `sPortOrigPos`; the IAT slot is pointed at a transparent wrapper
  `PortEwsrvPosHook`.
- The wrapper (hand-written asm in the port loader) re-pushes the caller's
  `this`, calls the genuine `TRawEvent::Pos()` (EUSER.DLL @0x1000ac40, which
  reads `x=[this+8]`, `y=[this+0xc]` and returns them in EAX:EDX), preserves and
  re-emits EAX:EDX unchanged, logs them via port slow exec 270 op 4, and
  returns. Only caller-saved EAX/ECX/EDX are touched; stack and callee-saved
  registers are preserved (cdecl semantics unchanged).
- `SvcMgrCtlExec` op 4 formats the ring-3-supplied value as the serial/VGA
  marker. Using an exec keeps `KPrintf` in ring 0; no kernel-internal logging
  from user mode.
- The wrapper's 4K page is made user-executable via the existing port
  `PdMapUser()` API. Its first placement in kernel `.text` faulted
  (`PD: #PF cr2=0x00110010 user=1 inUserRange=0` → `KERN 8`) because
  `LockKernelPages()` clears U/S on 0x1000000–0x1200000 — GDB caught the
  wrapper being called (`this=0x2202cac` inside EWSRV data) but faulting on the
  first instruction fetch. The explicit `PdMapUser()` fixes that without
  editing `mmu/pd.cpp`.

## Evidence (`/tmp/opencode/genuine_pointer.log`)

```
e32run: PTRINJ gate r=0 got=6 active=1
e32run: PTRINJ EWSRV ord 1868 -> PosHook 00115fd0 (genuine 003e65a8)
PTRBRIDGE: TRawEvent::Pos x=80 y=40
PTRBRIDGE: injected EPointerMove x=80 y=40 add=0 evtThread=103e038 head=1018480 tail=1018480
PTRBRIDGE: verdict head=1018480 tail=1018480 consumed=1
```

The marker appears *before* the bridge's own inject log line: EWSRV's request
was already pending, `Kern::AddEvent` handed the event straight to
`UserSvr::RequestEvent`, and `CEvent::RunL()`'s `EPointerMove` branch invoked
the genuine `Pos()` export. The logged EAX:EDX equal the injected (80,40),
closing the sret-ABI gap identified in `docs/ABI_AUDIT.md` §6 and
`docs/POINTER_BRIDGE_REVIEW.md`.

## Tests

- `python3 tools/r1_genuine_pointer_test.py` — **PASS** (all 6 EWSRV markers,
  9 EWCLIENT window markers, inject `add=0`, `consumed=1`, 231 imports bound,
  no Exc/Panic/FAULT, screendump renders the client window).
- `python3 tools/r1_genuine_ewsrv_test.py` — **PASS** (8/8 EWSRV + 9/9 client
  markers, no faults, desktop renders).
- `python3 tools/r1_qemu_smoke.py` — **PASS** (11/11 markers, no exceptions).
- `make kernfull` + `make isogrub` rebuilt; ISO at
  `qemu-port/build/symbian-grub.iso`.
