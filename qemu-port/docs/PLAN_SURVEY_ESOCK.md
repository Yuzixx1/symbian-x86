# PLAN SURVEY — genuine ESOCK from the tree?

Read-only survey of `kernelhwsrv/` (frozen genuine tree) vs. `qemu-port/`
(authored port) to decide whether a genuine ESOCK socket server can be built
from this tree or must remain absent.

## Present sources

- `kernelhwsrv/kernel/eka/drivers/ethernet/d_ethernet.cpp` — ethernet *device*
  driver (LDD) only; builds no protocol.
- `kernelhwsrv/kernel/eka/drivers/ecomm/` — UART/serial comms drivers, not
  sockets.
- PCMCIA/SDIO "socket" code (`drivers/pbus/...`, `pbus.h`) — hardware card
  sockets, unrelated to ESOCK; the only `esock` grep hits in the tree.
- `kernelhwsrv/kernel/eka/bx86gcc/euseru.def` — genuine EUSER.DLL export
  table, 2503 ordinals, **zero** socket/inet/host-resolver/DNS exports
  (grep matches are false positives like `User::IsRomAddress`).
- `qemu-port/variant/qemu_nic.cpp` — polled e1000 LDD (8086:100E, BAR0 MMIO,
  8x2KB rings); `qemu-port/variant/qemu_netbridge.cpp` — kernel frame bridge,
  shared `nb_shared` rings published via repurposed slow-exec slots 247/248.
- `qemu-port/user/netstack.c` + `netstack_iat.S` — authored loopback
  ARP/IPv4/ICMP/UDP/TCP E32 publishing `!SocketServer` (our authored
  exception); real-SLIRP userland apps (`ping.c`, `httpget.c`, `wget.c`,
  `telnet.c`, `dnsnet.c`, `dhcpc.c`) hand-build every frame over the
  netbridge rings. See `docs/NETWORKING_STACK.md`, `docs/NETWORK_TESTING.md`.

## Missing pieces

- ESOCK.DLL / `!SocketServer` genuine server source: no CServer2 socket
  server anywhere (`kernel/eka/newsrv/` does not exist; `ewsrv/` is the
  window server, being retired).
- Client API: no `RSocket`, `RSocketServ`, `TInetAddr`, `RHostResolver`,
  `CResolver` headers or sources; EUSER.DLL exports none of them and the
  table is frozen (2503 ordinals, cannot grow without editing read-only
  kernelhwsrv).
- Comms database (commsdb): absent — `userlibandfileserver/` has only
  `domainmgr` (power domains) and `fileserver`; the only commsdb mention in
  the whole tree is a stale line in `kernel/eka/release_old.txt`.
- Protocol modules: no TCP, UDP, IPv4/IPv6, ARP, ICMP, DHCP or DNS code in
  kernelhwsrv; no in-kernel netstack process, no ARP cache, no demux, no
  per-socket queues.

## Dependency chain

Genuine ESOCK would require, in order: commsdb (connection/IAP settings)
-> ESOCK.DLL protocol engine (TCP/IP/UDP/ARP) -> `!SocketServer` CServer2
IPC service -> EUSER.DLL client classes (RSocket/RSocketServ) -> apps. Every
link except the NIC LDD is absent from the tree, and the EUSER link is
impossible without modifying read-only kernelhwsrv. PE2 import capacity is
capped at the 2503 genuine EUSER ordinals (1-based IAT slots bound by
`variant/e32load.cpp`); there are no socket ordinals to import, so PE2
images can only reach sockets as **new IPC function numbers** on a server —
exactly what the authored `!SocketServer` in `user/netstack.c` does.

## Netstack bridge relationship

The authored netstack is the *only* socket surface and is the sanctioned
bridge: `variant/qemu_nic.cpp` + `qemu_netbridge.cpp` move raw frames
between the polled e1000 and ring-3 E32 via exec slots 247/248; userland
(`netstack.c`, `dnsnet.c`, `dhcpc.c`, `ping.c`, `httpget.c`, …) implements
the protocols and the `!SocketServer` IPC API. It is explicitly documented
as the replacement for the unimplemented genuine design in
`docs/r1_net_design_v1.md` / `r1_net_design_v2.md` (section 7 of
`docs/NETWORKING_STACK.md` lists ESOCK.DLL as "not delivered").

## Feasibility verdict

**Genuine ESOCK cannot be built from this tree and must remain absent.**
No ESOCK/commsdb/protocol sources exist in kernelhwsrv, EUSER.DLL's 2503
ordinals contain no socket API and the tree is read-only, and no genuine
socket server exists to host IPC. The authored `!SocketServer`/netstack
bridge over the netbridge rings is the only viable socket layer.

## First 3 concrete steps

1. Treat `user/netstack.c`'s `!SocketServer` IPC function numbers as the
   stable socket ABI for all future E32 apps; document the function-number
   table in `docs/NETWORKING_STACK.md`.
2. Flesh out the real-SLIRP path (ARP/IPv4/UDP/TCP over `exec2(247/248)`
   rings, per `docs/NETWORK_TESTING.md`) as the production netstack,
   keeping `netstack.c`'s loopback E32 purely as an IPC-surface test.
3. If a BSD-like API is ever wanted, add it as new IPC calls on
   `!SocketServer` (fd = subsession, payload via ipc slots) — never as new
   EUSER ordinals, since the genuine export table is frozen at 2503.
