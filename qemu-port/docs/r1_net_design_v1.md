# R1 Networking Design v1 — TCP/IP investigation (N-01..N-04)

Read-only investigation. No existing files modified. Sources: PROJECT_GOAL.md
Build Order step 7 (`D-06 -> N-01..N-04`), project_roadmap.md R4 (225-233),
parallel_work.md R4 (235). NOTE: `docs/full_os_checklist.md` named in the brief
does NOT exist in this checkout (only referenced by r1_abi_contract_v0.md:8,101-105).

## Current state: no networking code exists
- Grep of user/, services/, variant/, nk/, fs/, boot/, loader/, console/,
  r1-console/, inc/: only hit is vendor-class string "ethernet" in
  variant/qemu_pci.cpp:69. No socket/tcp/udp/ip/dhcp/dns code anywhere.
- `build/export/exec_enum.h`: zero network executives (only generic IPC/timer/
  chunk/msg-queue/logon execs). No network exec ordinals to call.
- No ESOCK.DLL / INET DLL / socket library in repo or sdk/. EUSER.DLL has zero
  network strings; its ~2503 ordinals are base EUSER only (User/RThread/
  RProcess/RChunk/RTimer/CBase/scheduler). No network EUSER ordinals exist.
- `tools/r1_qemu_smoke.py:145-154`: QEMU `-M q35` launched with no `-netdev`,
  `-nic`, or `-device` NIC. No deliberate guest-visible NIC; no RX/TX, no IRQ.
- No NIC driver, no PCI bus-master setup for a NIC, no packet buffers, no
  stack, no socket server, no DHCP/DNS, no networking apps. R4 not started.

## What EUSER/ordinals exist for networking
- None. E32 DLL exports are ordinal-only; networking in genuine Symbian lives
  in ESOCK (RSocket/CSocket) + INET DNS, neither present. The port's only EUSER
  veneers are ordinals 1 (Exit) and 2 (PutC) in variant/e32load.cpp:46-66, plus
  the shim in user/euser_shim.cpp. A socket API must be a NEW server exposed via
  the existing IPC bridge, not an EUSER ordinal.

## IPC mechanisms available (user <-> kernel/services)
- `user/ipc.c` + `user/ipc.h`: freestanding bridge over genuine kernel execs:
  server create (126), receive/cancel (34/35), accept (36), complete (65),
  session connect (128), sync/async send (77/37), close (106), descriptor
  copy (167/169), wait via request semaphore (`int 0x20`, start.S:10-14).
- `exec0/exec2/exec4` (`int 0x21`, start.S:16-65) = all kernel executives;
  descriptors are genuine TPtr/TPtrC layouts. Async completion is a
  TRequestStatus + one semaphore signal (ipc_wait_for, runtime.c:11-20).
- Kernel: EExecServerReceive/SessionSend/MsgQueue*/Chunk* execs
  (scodeseg.cpp:1676 ProcessCreate, sexec.cpp:1919 ThreadCreate,
  sthread.cpp:949 Create, sprocess.cpp:127 Create) — full EKA2 process/thread/
  IPC/chunk primitives already exist. Global named chunks give zero-copy RX/TX;
  msg queues and semaphores give signalling. svcmgr execs 268/269/270 launch,
  kill and reap services (e32load.cpp:1080-1163).
- Gap: no NIC device object, no interrupt handler registration for a NIC, no
  net server, no socket session/subsession multiplexing, no buffer pools.

## Concrete next steps (order)
1. N-01 NIC driver: add QEMU `-netdev user,id=n0 -device e1000,netdev=n0`
   (or rtl8139) to r1_qemu_smoke.py; write PCI enumeration + e1000 RX/TX ring
   driver as a kernel ISR/DFC in a NEW variant/ file (never touch kernelhwsrv/).
2. N-02 stack: implement a small freestanding IPv4/ARP/ICMP/TCP/UDP stack in a
   new user service (kernel-side DFC -> global chunk -> user stack task),
   using existing chunks + timers + `ipc_wait_for` for async I/O.
3. N-03 sockets: define an ESOCK-like IPC server (function numbers, descriptor
   slots via SIpcCopyInfo) and add `socket/bind/listen/connect/send/recv` and
   DHCP/DNS clients on top; expose in a new user lib header.
4. N-04 apps: add QEMU network regression to tools/test_*.py + test_stable.py
   (DHCP lease, DNS lookup, TCP loopback to host), then a sample net app.
5. Gate each step as QEMU-ready per roadmap vocabulary; keep kernelhwsrv/ frozen.
