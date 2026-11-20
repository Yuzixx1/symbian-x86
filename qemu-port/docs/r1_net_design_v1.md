# R1 Networking Design v1 — N-01..N-04 (read-only investigation)

Done criteria — §11 "Networking (TODO)" of docs/full_os_checklist.md (deleted in
dabf70b; text recovered from git 1e67072): N-01 NIC bring-up + RX/TX + link
status; N-02 Symbian TCP/IP stack port + ESOCK (RSocketServ/RSocket); N-03
DHCP/DNS/address config; N-04 end-to-end tests (ping, DHCP lease, TCP echo,
socket API). "IPC work is same-machine only and does not imply networking."
D-06 NIC driver blocks all networking.

Current state: no networking code exists.
- grep of user/, variant/, services/, tools/: only hits are host-side Python
  AF_UNIX QEMU-monitor sockets in test_*.py and the "ethernet" PCI class string
  (variant/qemu_pci.cpp:69). No guest NIC driver, stack, socket server,
  DHCP/DNS or networking applications. R4 not started.
- exec_enum.h has zero network executives; EUSER.DLL (~2503 ordinals) is base
  EUSER only; the port's veneers are ordinals 1/2 (variant/e32load.cpp:46-66).
  No ESOCK.DLL / INET DLL anywhere in repo or sdk/.
- tools/r1_qemu_smoke.py:144-154 boots -M q35 with no -netdev/-nic/-device NIC,
  so there is no guest-visible link, RX/TX path or NIC IRQ.

What EUSER/ordinals exist for networking: none. E32 DLL exports are ordinal-
only and genuine network APIs live in ESOCK/INET (absent here). A socket API
must be a NEW IPC service, not an EUSER ordinal.

IPC mechanisms (user <-> kernel/services): user/ipc.c + ipc.h bridge over
genuine execs — server create 126, receive/cancel 34/35, accept 36, complete
65, session connect 128, send 77/37, close 106, TPtr copy 167/169; request-
semaphore wait via int 0x20 (user/start.S:10-14); exec0/2/4 = int 0x21
(start.S:16-65). Kernel EKA2 primitives exist: DProcess::Create
(sprocess.cpp:127), DThread::Create (sthread.cpp:949), ExecHandler::ThreadCreate
(sexec.cpp:1919); E32Run (e32load.cpp:672) creates/loads/resumes user processes;
named global chunks, timers, msg queues and svcmgr execs 268/269/270 available.
Gap: no NIC device object, NIC interrupt handler, buffer pools or socket session
multiplexing.

Concrete next steps:
1. D-06/N-01: add -netdev user,id=n0 -device e1000,netdev=n0 to the smoke test;
   PCI scan (PciFindClass 02,00) + e1000 MMIO TX/RX descriptor rings as a kernel
   ISR/DFC in a NEW variant/ file; wire PCI INTx (config 0x3C) via Interrupt::
   Bind (storage/USB are polled templates); poll one ARP frame first, then IRQ.
2. N-02: small freestanding IPv4/ARP/ICMP/UDP/TCP stack in a user service —
   kernel DFC -> global named chunk -> user stack task, reusing timers and
   ipc_wait_for for async I/O (zero-copy RX/TX via shared chunk).
3. N-03: ESOCK-like IPC server (function numbers, SIpcCopyInfo descriptor
   slots) exposing socket/bind/listen/connect/send/recv plus DHCP/DNS clients.
4. N-04: QEMU regressions (ping, DHCP lease, TCP echo, socket API) in
   tools/test_*.py + a sample networking application.
5. Keep kernelhwsrv/ frozen; gate each step as QEMU-ready per roadmap vocabulary.
