# R1 Networking Stack — full architecture (source-verified)

**Scope.** This document describes the networking that actually exists in this
tree today, read directly from source. The original intent is recorded in
`docs/r1_net_design_v1.md`, `docs/r1_net_design_v2.md` and
`docs/r1_dns_design_v1.md`; those docs proposed a kernel netstack + `!SocketServer`
CServer2 with BSD-style sockets. That design was **not** implemented. What ships
is a much smaller, pragmatic three-layer design in which ring-3 E32 programs
build and parse every protocol byte themselves and the kernel only provides a
polled NIC plus a shared-memory frame pipe.

`kernelhwsrv/` is frozen/read-only throughout; every networking change lives in
`variant/` (kernel side) and `user/` (E32 side). No executive-table entries of
the genuine kernel are added — two already-invalid slow-exec slots are repurposed.

## 1. Layer diagram

```
 ring-3 PE2 E32 (user/*.c)           raw Ethernet/IPv4/UDP/TCP/ICMP/ARP frames
   netbridge_test / dnsres / dnsnet / dhcpc / ping / httpget / wbrowser / wget / telnet
        |  exec2(247)=base, exec2(248)=size  ->  gSh = struct nb_shared*
        v
 shared struct nb_shared (low, identity-mapped RAM, U/S=1 in user PDs)
   tx ring: user producer -> kernel consumer      rx ring: kernel producer -> user consumer
        |  netbridge_poll() / NetBridgePollLoop(ms)  [kernel `netbridge` shell command]
        v
 kernel e1000 driver (variant/qemu_nic.cpp)  PCI 8086:100E, BAR0 MMIO, polled TX/RX
        |  8 legacy TX descriptors / 8 RX descriptors, 2048-byte buffers
        v
 QEMU user-mode SLIRP (user network): guest 10.0.2.15, NAT gateway 10.0.2.2
```

There is no socket layer, no in-kernel IP stack and no interrupt/driver-thread
path. Userland *is* the stack.

## 2. Layer 1 — the polled e1000 driver (`variant/qemu_nic.cpp`)

* **Discovery:** `e1000_init()` scans bus 0 device 0..31 (and multifunction
  children) for vendor `0x8086` / device `0x100E` (82540EM). BAR0 is masked with
  `0xFFFFFFF0` and saved in `gBase`. Config command is OR'd with `0x06`
  (MEM + bus master). The base is used directly as an MMIO pointer (`Rd32`/`Wr32`).
* **Reset and MAC:** `e1000_reset()` writes `CTRL.RST`, spins until self-clear,
  then forces `SLU | ASDE` (link up + address-space decode) and caches `STATUS.LU`.
  `e1000_read_mac()` bit-bangs the EEPROM through EERD: for words 0..2 write
  `(word<<8)|START`, poll for the DONE bit (bit 4 — QEMU `flash_eerd_read`
  convention), then read the 16-bit word from bits 31:16; each word is stored
  low byte first. QEMU's default EEPROM yields guest MAC **52:54:00:12:34:56**.
* **Receive address:** RAL/RAH are programmed from `gMac` with the AV bit set.
* **Rings:** static, 16-byte-aligned `TxDesc`/`RxDesc` arrays of `kNumDesc = 8`
  with `kBufSize = 2048`. Because RAM is identity mapped, descriptor and buffer
  addresses written into TDBAL/RDBAL are plain virtual addresses.
  * TX: `e1000_init_tx()` zeroes descriptors, sets TDBAL/TDBAH/TDLEN (128 bytes),
    TDH=TDT=0, and `TCTL = EN | PSP | CT | COLD`.
  * RX: `e1000_init_rx()` points each RxDesc at `gRxBuf[i]`, sets RDBAL/RDBAH/
    RDLEN, RDH=0, **RDT = kNumDesc-1** (QEMU treats RDT as an exclusive tail), and
    `RCTL = EN | BAM | SECRC`.
* **TX:** `e1000_tx()` spins until `TDH == TDT` (previous descriptor reclaimed),
  copies the frame into `gTxBuf[TDT]`, sets `iAddr`, `iLower = len | EOP | IFCS |
  RS`, a compiler barrier, then kicks hardware by writing `TDT = idx+1`. The
  caller supplies the complete frame — the driver builds no protocol.
* **RX:** `e1000_recv()` is non-blocking. If `gRxRing[gRxNext].iStatus & DD` is
  clear it returns 0; otherwise it copies `iLength` bytes out, recycles the
  descriptor, advances the local cursor, and writes `RDT = consumed` (**not**
  the next cursor — writing the next cursor stalls RX after one packet).
* **Link/IRQ:** `e1000_link_up()` reads `STATUS.LU`. `e1000_irq()` is a *poll*,
  not an ISR: it reads ICR, ACKs by writing it back, and refreshes cached link on
  LSC. **No PCI IRQ/ISR or DFC is bound.**
* **Boot hook:** `__attribute__((constructor)) E1000BootInit()` calls
  `e1000_init()` from HwInit0's `.init_array` walk. Bring-up prints:
  `e1000: BAR0=... MAC=... link=1`.
* Public API declared in `variant/qemu_variant.h`: `e1000_init/send/recv/
  link_up`, plus bring-up helpers `e1000_reset/read_mac/init_tx/init_rx/tx/irq`.

## 3. Layer 2 — the kernel netbridge (`variant/qemu_netbridge.cpp`)

`netbridge_init()` calls `e1000_init()`, zeroes one static, 4096-aligned
`struct nb_shared gNetBridge`, copies the EEPROM MAC into `gNetBridge.mac`, sets
`version = NB_VERSION`, both rings' `head = tail = 0`, a fence, and finally
publishes `magic = NB_MAGIC` last. `gBridgeBase` is the physical (= virtual)
address of the structure.

**Why two execs instead of a named chunk:** a genuine `RChunk` always allocates
its own RAM (`MM::AllocRegion`) and cannot be backed by a caller-supplied buffer;
aliasing it to the e1000 rings would require invasive edits to read-only kernel
code. Low RAM is already identity mapped and user-accessible, so returning the
physical base is far less invasive.

**Executive installation:** `NetBridgeExecInstall()` patches the genuine
`EpocSlowExecTable`. It requires `tab[0] >= 249` (271 entries in this build) and
only repurposes slots still pointing at `InvalidExecHandler` (`tab[1]`):
slot **247** = `QE_EXEC_NETBRIDGE_BASE` → `NetBridgeBaseExec` (returns the base),
slot **248** = `QE_EXEC_NETBRIDGE_SIZE` → `NetBridgeSizeExec` (returns
`sizeof(gNetBridge)`). Each handler is written at `tab[4 + 2*slot]` with flags at
`tab[3 + 2*slot]`. Boot banner:
`netbridge: base=... size=... mac=... (execs 247/248)`.

**Pump:** `netbridge_poll()` drains up to 16 TX slots — for each ready slot it
calls `e1000_tx(s->data, s->len)`, clears `s->len`, advances `tx->tail` — then
fills at most one RX slot from `e1000_recv()` and advances `rx->head`. It returns
the number of frames moved. `NetBridgePollLoop(aMaxMs)` (the `netbridge` shell
command) re-inits if needed, prints `netbridge: polling up to N ms (done=0)`, and
loops `netbridge_poll(); NKern::Sleep(10)` while `!gNetBridge.done` and time
remains, ending with `netbridge: poll end moved=... done=... tx=.../... rx=.../...`.

The user program sets `gSh->done = 1` at completion to stop the loop. `E32Run`
returns immediately, so the harness launches the E32 first and *then* runs the
`netbridge` command while the process waits on the rings. Both translation units
have `.init_array` constructors, made harmless by `e1000_init()` being idempotent.

## 4. Layer 3 — userland and the ring protocol (`user/netbridge.h`)

`struct nb_shared` is the wire contract between kernel and user:

| Field | Meaning |
|-------|---------|
| `magic` / `version` | `NB_MAGIC` = `"NBRG"` (`0x4E425247`), `NB_VERSION` = 1; validated by every E32 |
| `done` | user sets 1 → kernel poll loop exits |
| `mac[6]` | guest MAC read from EEPROM; used as the Ethernet source address |
| `tx`, `rx` | two `struct nb_ring`s |

`struct nb_ring` holds `head` (producer cursor), `tail` (consumer cursor),
`drops` (producer slot-busy overruns) and `slot[16]`. `struct nb_slot` holds a
`volatile len` (ready flag: **0 = empty**, else frame byte count), reserved word
and `data[2048]` (`NB_FRAME_SIZE`). All cursors are `mod NB_RING_SLOTS` (16).

It is a single-producer/single-consumer queue per direction:

* **TX (user → kernel):** producer writes `slot[head].data`, sets `len`, an
  `__asm__ __volatile__("" ::: "memory")` fence, then advances `head`. Kernel
  consumer reads `slot[tail].len`, sends it, clears `len`, fences, advances `tail`.
* **RX (kernel → user):** kernel producer writes `slot[head].data`, sets `len`,
  fences, advances `head`. User consumer reads `slot[tail].len`, copies/parses it,
  clears `len`, fences, advances `tail`.

The `len` word is the readiness flag; there is no separate doorbell. The
producer drops a frame if the target slot's `len` is still non-zero. Every app
implements small helpers (`tx_post`/`post_frame`, `rx_take`/`rx_next`) with the
same fence discipline. Discovery is by `exec2(247,0,0)` / `exec2(248,0,0)` — the
returned base is cast directly to `struct nb_shared*`.

## 5. SLIRP topology (QEMU user-mode networking)

Every test boots with `-netdev user,id=n0 -device e1000,netdev=n0` on a `-M q35`
machine. Relevant addresses, hard-coded in the apps:

* guest IPv4: **10.0.2.15**; gateway / L2 next hop: **10.0.2.2**;
* SLIRP DNS proxy: **10.0.2.3** (port 53); SLIRP DHCP server on UDP 67;
* gateway MAC (learned by ARP, or fallback `52:55:0a:00:02:02`); guest MAC
  **52:54:00:12:34:56** from EEPROM;
* HTTP targets reached through SLIRP NAT at `104.20.23.154:80` with
  `Host: example.com`; local TCP ports 49152 (HTTP/DNS) and 49153 (telnet).

## 6. Real protocols implemented in userland

All framing/checksum code is freestanding (no libc). A shared RFC-1071
one's-complement `csum()` folds carries to 16 bits; IPv4 and TCP/UDP checksums
use the pseudo-header where required.

* **ARP:** 42-byte Ethernet/ARP who-has; reply parsed by EtherType `0x0806`,
  op=2, sender protocol IP. Used by `netbridge_test`, `dnsnet`, `ping`; the TCP
  apps also emit a who-has to teach SLIRP the guest MAC and answer SLIRP's own
  who-has 10.0.2.15 (`arp_process`) so the SYN-ACK is delivered.
* **IPv4 + checksums:** `0x45`, TTL 64, DF set, protocol 1/6/17; header checksum
  verified on ping replies and computed on all TX frames.
* **UDP + DNS:** `dnsnet.c` builds a 12-byte header (random 16-bit ID, RD
  `0x0100`, QD=1), QNAME labels + root, QTYPE=A/QCLASS=IN, UDP length/pseudo-header
  checksum, and parses the reply by matching ID, QR=1, RCODE=0, skipping
  compressed names (0xC00n pointers) and CNAMEs, taking the first A RDATA.
* **DHCP DORA:** `dhcpc.c` builds BOOTP DISCOVER/REQUEST with magic cookie
  `0x63825363`, options 53/55/61 (and 50/54 on REQUEST), UDP 68→67 broadcast,
  IPv4 0.0.0.0→255.255.255.255. Replies are parsed for msgtype 2 (OFFER) and 5
  (ACK), yiaddr, options 1/3/6/54. QEMU SLIRP really leases 10.0.2.15 with
  mask, gateway 10.0.2.2 and DNS 10.0.2.3.
* **ICMP echo:** `ping.c` sends 3 echoes (id `0x4B45`, "EKA2PING" payload) and
  validates real replies: EtherType IPv4, proto 1, source 10.0.2.2, correct IP
  and ICMP checksums, matching id/seq.
* **TCP + HTTP:** `httpget.c` performs SYN → SYN-ACK (ack = ISN+1) → ACK, sends
  `GET / HTTP/1.0\r\nHost: example.com\r\n\r\n`, ACKs each returned segment,
  reassembles the body, reports `status 200` and `<title>Example Domain</title>`.
  `wbrowser.c` adds URL parsing and an HTML-to-text renderer; `wget.c` saves the
  body after the first `\r\n\r\n` to `DOWNLOAD.TXT` via PortFile IPC `file_save`;
  `telnet.c` is an interactive line client on a chosen `host port`.

**DNSRES is real too (R7).** The `build/user/DNSRES.E32` rule in `user/user.mk`
builds DNSRES from `dnsnet.elf` — i.e. the same `user/dnsnet.c` client described
above: ARP the `10.0.2.2` gateway, send UDP/53 to SLIRP's DNS proxy `10.0.2.3`
and parse the reply from the RX ring. `example.com` therefore returns a real
public A record (e.g. `172.66.147.243`), never a hardcoded `93.184.216.34`.
The former loopback `DnsResolverServer`/IPC responder in `user/dnsresolv.c` is
historical and **not built**; any note calling DNSRES a loopback fake is stale.

**Markers** are emitted through genuine `EUSER.DLL` ordinal 915
(`RDebug::Printf`). Each ELF is linked with
`--defsym=<app>_printf_slot=0x408000` (the PE2 IAT slot); `elf2e32.py
--euser-import 915` emits the import and `E32Run` overwrites the slot before
resuming. If no DLL is bound, `marker()` falls back to kernel `print()`.

## 7. Status vs. the original design / gaps

Delivered: real e1000 TX/RX, kernel frame bridge, and userland ARP, IPv4/UDP,
DHCP, DNS, ICMP and TCP/HTTP — proven with real SLIRP replies.

Not delivered (from `r1_net_design_v1/v2.md` and `r1_dns_design_v1.md`):

* No BSD-socket / ESOCK API — ESOCK.DLL is absent; sockets were to be new IPC
  function numbers on a `!SocketServer` (fd = subsession, payload via
  `ipc_read8`/`ipc_write8`). Instead each E32 hand-builds raw frames.
* No in-kernel netstack process, ARP cache, defrag, TTL handling or demultiplexer;
  no per-socket RX queues.
* **Polled NIC only** — no PCI interrupt, ISR or DFC; the `netbridge` shell loop
  is the pump and `e1000_irq()` merely ACKs ICR.
* **Minimal TCP** — a fixed single handshake with cumulative ACKs and no
  retransmit timer, congestion/window management, out-of-order reassembly or
  TIME_WAIT; the receive window is a constant 8192.
* No T1/T2 DHCP renewal, no ARP-probe conflict detection, no DNS cache beyond a
  single query, and no IPv6.

For how each of the ten QEMU tests exercises these layers, see
`docs/NETWORK_TESTING.md`.
