# Genuine EKA2 kernel internals as used by this port

Scope: the full `kernfull` image links ~80 real kernel objects plus ~19
nanokernel objects compiled from `../kernelhwsrv/kernel/eka/` (READ-ONLY). The
port owns the bootstrap, MMU glue, variant ASIC and userland runners; the
scheduler, process/thread objects, IPC, executives, timers and active objects
described below are the genuine Symbian implementations, not stubs. Line
references are `file:line` against this tree; `kh/` abbreviates
`../kernelhwsrv/kernel/eka/`.

## 1. ResetEntry and QemuInitialiseHardware (bootstrap)

`boot/multiboot.S:139-144` saves the Multiboot magic/EBX in a handoff page and
calls `ResetEntry` with `EAX=magic, EBX=info*`. `ResetEntry`
(`boot/ported/bootmain_gas.S:36-84`) preserves the genuine order:

- `cli; cld` then save both entry registers to `.bss`
  (`bootmain_gas.S:37-44`) before the stack switch would orphan them.
- `call QemuInitialiseHardware` (`bootmain_gas.S:48`). This is the port's
  stand-in for the variant MASM routine: it checks `MB_BOOT_MAGIC` and the
  handoff magic `0x51454B32`, prints the RAM summary on COM1, returns EAX=0
  on success (`bootmain_gas.S:91-120`).
- `ESP = qemu_boot_stack_top` and `call InitCpu` (`bootmain_gas.S:59-68`)
  before the MMU, then `_E32Startup_Body(romHeader, superPage)`
  (`bootmain_gas.S:76-78`).

`InitCpu` (`boot/ported/bootcpu_gas.S:150-211`) does the EFLAGS.ID toggle
test, CPUID.01 feature detection into `qemu_cpu_features`, enables CR4.PSE
for 4 MiB pages (`:189-196`), then `SetupGDT` and `SetupIDT` (`:202-203`).
`SetupGDT` (`:219-287`) writes the genuine descriptor bytes
`R0CS=0x00cf9b00 / R0DS=0x00cf9300 / R3CS=0x00cbfb00 / R3DS=0x00cbf300`
(flat 0-4G), a TSS descriptor (type 0x89) at selector 0x28, `lgdt`, a far
jump to reload CS, and loads DS/ES/SS/FS/GS/LDT. `SetupIDT` (`:397+`) builds
the CPU trap gates; `SetupExecGates` (`:366-379`) later installs DPL3 gates
at IDT 0x20/0x21 pointing at the genuine `__X86Vector20/21`.

Variant hardware init follows the genuine `Asic::Init1 -> Init3` split:

- COM1 16550A at 0x3F8, 115200 8N1 + FIFO (`variant/qemu_assp.cpp:14-29`,
  called from `variant/kernfull_glue.cpp:95`): LCR=0x83, divisor 1, LCR=0x03,
  IER/FCR setup; `EnableRxIrq()` sets IER=0x01 and MCR=0x08 to gate the UART
  onto ISA IRQ4 (`qemu_assp.cpp:39-45`).
- The i8259 PIC is remapped in `QemuVariantAsic::Init1`
  (`variant/qemu_variant_asic.cpp:75-90`): mask both PICs (`:79-80`),
  `QemuIrq::RemapPic(0x30, 0x38)` (`:81`) so hardware IRQs live at 0x30+
  and never collide with the CPU trap vectors or the 0x20/0x21 exec gates,
  then re-mask. The ICW sequence is ICW1=0x11, ICW2=base, ICW3=0x04/0x02,
  ICW4=0x01 (`qemu_assp.cpp:47-58`). The legacy bridge path in
  `variant/genuine_glue.cpp:110` used 0x20/0x28 before the exec gates were
  separated; the full-kernel image uses 0x30/0x38.
- The i8254 PIT channel 0 is programmed for 1000 Hz in mode 3,
  divisor `1193182/1000` (`qemu_assp.cpp:72-79`, called at
  `qemu_variant_asic.cpp:85`). The C dispatcher `X86IrqDispatcher`
  (`variant/qemu_variant_int.cpp:131-145`) identifies the active IRQ from
  the 8259 ISR (not from the stack frame), calls the bound `TIsr`, and sends
  EOI (`:38-43`); IRQs 0/1/4 are bound at `:217-223`.

## 2. KernelMain, K::Init1/Init2/Init3, Asic::Init3

`_E32Startup_Body` (`kh/kernel/x86/k_entry.cpp:38-53`) initialises the
SuperPage from ROM, constructs statics and calls `KernelMain`. The genuine
`KernelMain` (`kh/kernel/sinit.cpp:492-512`) sets `K::Initialising`, calls
`P::CreateVariant()`, `NKern::Init0(K::VariantData[0])` and
`K::InitialiseMicrokernel()`, then becomes the null/idle thread in
`NKern::Idle()`.

`K::InitialiseMicrokernel()` (`kh/kernel/sinit.cpp:145-393`) is what the
architecture doc calls K::Init1/Init2 (there is no literal `K::Init1` symbol
in this tree):

- Phase 1: `M::Init1()` (memory model) then `A::Init1()` (ASIC, enables
  interrupts) at `:150-154`.
- `P::KernelInfo()` supplies the kernel stack/heap addresses; a fixed
  `RHeapK` is installed as `K::Allocator` at `:156-166`.
- `P::NewProcess()` + `GetNewThread()` create the initial kernel process and
  null thread; `NKern::Init()` installs `EpocThreadHandlers`,
  `EpocFastExecTable`, `EpocSlowExecTable` (`:169-218`).
- Phase 2: `M::Init2(); A::Init2()` at `:224-225`, then object containers,
  the `SvHeap` chunk, heap mutex, supervisor-stack chunk, `MsgChunk` and
  mutex (`:227-332`).
- The supervisor thread is created as `EThreadSupervisor` at priority
  `EThrdPriorityMuchMore` (`:364-380`), marked
  `KThreadFlagSystemPermanent`, stored in `K::SvThread` /
  `K::TheKernelThread`, and resumed at `:390-391`. It runs `SupervisorThread`
  (`:396-413`), which calls `K::Init3()`, `P::StartExtensions()`, `M::Init4()`
  and `K::StartKernelServer()`.

`K::Init3()` (`:415-489`) initialises the HAL array, `M::Init3()`, pub/sub,
the event queue, DFC queues 0/1 and the shared DFC queue, NVRAM,
`NTimerQ::Init3`, the system tick queue `K::StartTickQueue()` (`:470`), the
secure RNG and finally `A::Init3()` (`:484`).

`QemuVariantAsic::Init3()` (`variant/qemu_variant_asic.cpp:92-148`) enables
IRQ0/1/4, registers the display HAL group, mounts the FAT32 volume and the
GRUB module as `X:`, installs and boot-launches SVCMGR.E32 (`:141-146`), and
calls `ShellSpawnFull()`. The shell is a genuine `DThread` created with
`Kern::ThreadCreate` as `EThreadSupervisor` in the kernel process with an
auto-allocated 32 KiB stack and priority 10 (`variant/kern_shell.cpp:1809-1834`),
then resumed with `Kern::ThreadResume` — so it can own the container mutexes
used by `ps`/`threads`.

## 3. Process and thread creation

`Kern::ProcessCreate` (`kh/kernel/scodeseg.cpp:1776-1801`) requires thread
critical section, calls `P::NewProcess()` then `DProcess::Create(EFalse, …)`
(`kh/kernel/sprocess.cpp:127-206`: name, paging attributes, UID type,
priority, generation, security zone, `$`/DLL mutexes, `DoCreate`), and
finally `K::MakeHandleAndOpen`; on error it stillborns the first thread and
releases the process. `DThread::Create` is at
`kh/kernel/sthread.cpp:949`, `DProcess::NewThread` at
`kh/kernel/sprocess.cpp:622`.

The port's loader `E32Run` (`variant/e32load.cpp:692-912`) validates the
E32 image (UID1 must be `0x1000007a`, `:700-701`), pre-checks all three
per-process tables including `PdAvail()` (`:705`), resolves DLL imports,
builds a `TProcessCreateInfo` (`:729-768`), enters CS (`:770-772`) and calls
`Kern::ProcessCreate` (`:795`). It then calls `PdCreate` (`:805-808`), copies
code/data/BSS, applies relocations and binds IAT slots (`:812-870`),
rewrites the entry point to an absolute run address (`:873-877`), calls
`proc->Loaded(pinfo)` (`:881`), tags every thread's address space
(`:896`, see `TagProcessAddressSpace` at `:50-64`), resumes the process
under `NKern::LockSystem` (`:895-898`), closes the creation handle and
records the code seg for reaping (`:900-906`).

The genuine x86 scheduler (`kh/nkern/x86/ncsched.cia:160-185`) saves the old
thread's ESP, installs the new one, sets `TSS.esp0` from
`iStackBase+iStackSize` through `X86_TSS_Ptr` (`:175-179`, i.e. the ring-0
stack for the next ring-3 entry), and on an address-space switch calls
`TheScheduler.iProcessHandler` with `EBX=new NThread`, `EDI=scheduler`
(`:181-184`). The port's lazy `TssInitOnce()` (`variant/e32load.cpp:32-44`)
publishes `X86_TSS_Ptr` because the genuine `X86::Init1Interrupts` is not
run in this image.

## 4. Executives (int 0x21 / int 0x20)

`EUSER` infers fast exec #0 and all slow execs through the IDT DPL3 gates.
`kh/include/u32exec.h:69-102` defines the x86 GCC macros: `int 0x20` for
fast calls (number in EAX, arg in ECX) and `int 0x21` for slow calls with
args in ECX, EDX, EBX, ESI. The port mirrors this in `user/start.S:10-74`:
`ipc_wait_any` (`xor eax,eax; int $0x20`), `exec0`, `exec2`, `exec4` and
`process_exit` (EAX=115 `EExecThreadKill`). The genuine handlers are
`__X86Vector20` (`kh/nkern/x86/vectors.cia:75-168`; EAX=0 branches to
`NKern_WaitForAnyRequest`) and `__X86Vector21` (`:183-209`; indexes the
slow table at `ESI + EAX*8`).

The generated slow table has 271 slots, 0..270 (`tab[0]==0x10f`), with
layout `{count, InvalidExecHandler, PreprocessHandler, {flags, fn}…}` from
`kh/include/nkern/nkern.h:665-671` (entries begin at word 3/4). The port's
patchers write `tab[3+2*slot]=flags` and `tab[4+2*slot]=function`, only
taking slots still pointing at `InvalidExecHandler`:
audio tone = 240 (`variant/qemu_audio_exec.cpp:19-52`), process census =
241 (`variant/qemu_ps_exec.cpp:33-108`), netbridge base/size = 247/248
(`variant/qemu_netbridge.cpp:37-70`), GUI fbbase/fbsize/draw = 249/250/251
(`variant/qemu_gui_exec.cpp:152-180`), and SvcMgr launch/kill/ctl = 268/269/
270 (`variant/e32load.cpp:1173-1187`). Slots 252..267 remain genuine execs.
`build/export/exec_enum.h:63-131` is the generated enumeration: 0=
`ObjectNext`, 1=`ChunkBase`, 2=`ChunkSize`, 4=`HandleAttributes`,
5=`TickCount`, 53=`TimerAfter`, 68=`TimeNow`, 106=`HandleClose`,
107=`ChunkCreate`, 115=`ThreadKill`, 126=`ServerCreate`, 133=`TimerCreate`,
152/153=`ProcessCreate/Loaded`.

## 5. IPC and the request semaphore

`user/ipc.c` is a thin ABI bridge over those genuine execs:
`ipc_server_create` = exec 126 (`:30-34`), `ipc_connect` = exec 128 plus a
function -1 (EConnect) call (`:106-114`), async receive = exec 34 and
cancel = 35 (`:41-46`), descriptor copy = exec 169 (`:51-73`),
`ipc_complete` = exec 65 (`:74-78`), accept = 36 (`:79-82`), sync/async
send = 77/37 (`:88-105`) and close = 106 (`:115`). The wire structs in
`user/ipc.h:4-11` match `RMessage2`/`TIpcArgs`.

Every wait bottoms out in `ipc_wait_any()` (`user/start.S:10-14`, fast exec
0) which blocks on the thread's request semaphore; `ipc_wait_for`
(`user/ipc.c:19-29`) counts surplus wakeups and calls exec 58
(`RequestSignal`) to repost them, matching genuine `WaitForRequest`
accounting. Kernel side: `ExecHandler::ServerReceive`
(`kh/kernel/sipc.cpp:1692-1699`), `SessionSend` (`:1710-1717`),
`MessageComplete` (`:1729-1749`) and `ServerCancel` (`:1701-1708`).

`CServer2::Start` (`kh/euser/cbase/ub_svr.cpp:359-368`) publishes the global
server, adds itself to the active scheduler and calls `ReStart()`, which
posts `iServer.Receive(iMessage,iStatus)` + `SetActive()` (`:525-530`).
`RunL` (`:538-566`) dispatches `ServiceL` / `Connect` / `Disconnect` and
re-arms the receive; `RunError` (`:510-516`) calls `ReStart()` if needed.

## 6. Active objects

The port's `user/e32base.h` reproduces the genuine ABI: `CActive` derives
from `CBase`, owns a `TRequestStatus` and an intrusive `TPriQueLink`, and
declares pure virtual `RunL()`/`DoCancel()` plus `RunError()` (`:112-151`).
`CActiveScheduler::Add` keeps a descending-priority queue (`:172-183`);
`Start()` loops `WaitForAnyRequest()` -> `DoRunL()` until `Stop()` (`:184-193`),
and `DoRunL()` dequeues completed actives and calls `cur->RunL()` through
the vtable (`:204-218`). The genuine implementation is
`kh/euser/cbase/ub_act.cpp:814-904` (`CActiveScheduler::Start` nests a
`TLoop` and runs `pS->Run(loop.iOwner)` under `TRAPD`). `RTimer::After`
sets `KRequestPending` and calls `Exec::TimerAfter`
(`kh/euser/us_exec.cpp:5359-5387`); completion wakes the active via the
request semaphore, driving `RunL()`.

## 7. Chunks and heap allocators

User `chunk_create` builds a genuine `TChunkCreate` (global attr 0x10 for
named chunks) and calls exec 107/108/109/1/2
(`user/runtime.c:148-184`). `user/malloc.c:1-134` is a chunk-backed
first-fit allocator: the first `malloc` creates a 16 KiB private chunk,
OOM grows it by 16 KiB up to 64 KiB (`:59-89`), `free` coalesces both
neighbours (`:25-49`). In the kernel image, `malloc`/`free` in
`variant/kernfull_glue.cpp:48-79` use a 256 KiB pre-heap bump pool until
`K::Allocator` exists, then route to `Kern::Alloc`/`Kern::Free` (with a
range check so pool cells never reach `Kern::Free`). The genuine fixed
`RHeapK` is created in `kh/kernel/sinit.cpp:163-166` and mutated into a
chunk heap at `:293-294`.

## 8. Faults, panic and the exit ring

`PdInit()` installs the port's page-fault vector (`mmu/pd.cpp:307`): the
naked `PdPageFaultStub` (`:224-245`) saves segments, reads CR2 and the error
code, and calls `PdPageFaultC` (`:273-292`), which prints
`PD: #PF pid=… cr2=… err=… user=… inUserRange=… cr3=…` and then either
`K::PanicKernExec(KErrGeneral)` for user faults (kills the current thread,
machine lives — `kh/kernel/server.cpp:894-905` -> `PanicCurrentThread` ->
`DThread::Die(EExitPanic,…)`) or `Kern::Fault("PD-KernelPF", …)` for kernel
faults. The genuine exception dispatcher
(`kh/kernel/x86/ckernel.cpp:131-189`) maps vector 14 to `EExcPageFault`
(`:116-117`), honours a thread's `TExcTrap`, reentry and handled exceptions,
and ends in `K::PanicKernExec(ECausedException)`; the full
`TPagingExcTrap` machinery exists (`kh/kernel/x86/ckernel.cia:331-399`) but
is not linked into this image, so raw #PF delivery is documented as a gap.

The post-mortem exit ring is `sExits[16]` in `variant/e32load.cpp:629-691`
(`ExitRecRecord` at `:632`, printed by `E32PrintExits` at `:670`); records
are currently written only by `E32Reap`, leaving a death-visibility window
that P-08 stress (`tools/r1_cp4_ca_stress_test.py:1-35`: 4 clients, 3 cycles,
one peer kill, no lost semaphore completions, no `Exc`/`PanicKernExec`) is
designed to catch.

## 9. Timers

- **System tick (exec 5):** `EExecTickCount` is genuine `Kern::TickCount`.
  The nanokernel/PIT runs at 1000 Hz but the system tick is 64 Hz:
  `kh/kernel/stimer.cpp:141-146` sets `iTickPeriod=15625` us,
  `iTicksPerSecond=64`; user `clock.c:9-29` documents the exec-5 -> 1/64 s
  conversion and prints the raw count.
- **Wall clock (exec 68):** `EExecTimeNow` -> `ExecHandler::TimeNow`
  (`kh/kernel/stimer.cpp:1592-1599`) returns microseconds since 0AD plus
  UTC offset; `user/runtime.c:58-69` anchors it to Unix seconds.
- **RTimer (exec 133/53 plus 65/106):** `exec0(133)` = `TimerCreate`,
  `exec4(53, timer, &status, us, 0)` = `TimerAfter` (genuine
  `kh/kernel/stimer.cpp:1418-1426`), then `ipc_wait_any` on the request
  semaphore and `exec2(106)` to close the handle
  (`user/runtime.c:25-44`). `MsTickPeriod()` returns 1000 us
  (`variant/qemu_variant_asic.cpp:162-173`); the earlier 1 us value made
  `sleep_ms(1000)` expire in ~64 ms because of the 2^14 PIT divisor.
