# R1 Network Testing — how to run and interpret each test

**Scope.** All tests are standalone Python harnesses in `tools/`. Each builds a
32 MB raw disk with a bootable FAT32 partition at LBA 2048, stages a PE2 E32 plus
the genuine `EUSER.DLL` (for ordinal 915, `RDebug::Printf`), boots
`build/kernfull.elf` under QEMU user-mode SLIRP, drives commands over the serial
console with a timed `printf` feeder, then greps the serial log for markers and
checks for exception/panic lines (`Exc `, `PanicKernExec`, `MMU #PF`).

Common QEMU flags: `-M q35 -m 128 -netdev user,id=n0 -device e1000,netdev=n0
-display none -serial stdio -kernel <kern> -drive file=<disk>,format=raw,
snapshot=on,if=none,id=d0 -device ich9-ahci,id=sata -device ide-hd,drive=d0,
bus=sata.0 -monitor none -no-reboot`. `wget` deliberately omits `snapshot=on`
so the downloaded file persists on the image.

Build the images first: `make -C ..` (or the repo root) produces
`build/kernfull.elf` and the `build/user/*.E32` targets defined in
`user/user.mk`.

## Test inventory

| Script | E32 (staged name) | Inner QEMU timeout | Launches |
|--------|-------------------|--------------------|----------|
| `r1_qemu_nic_test.py` | none (kernel `nic` cmd) | 70 s | `nic arp 3` |
| `r1_netbridge_test.py` | `NETBRIDGE.E32` (`netbr`) | 80 s (×2 retries) | `netbr`, `netbridge` |
| `r1_dhcpc_test.py` | `DHCPC.E32` (`dhcpc`) | 80 s | `dhcpc`, `netbridge 30000` |
| `r1_dnsnet_test.py` | `DNSNET.E32` (`dnsnet`) | 110 s | `dnsnet`, `netbridge 25000` |
| `r1_qemu_dns_test.py` | `DNSRES.E32` (`dnsres`) | 90 s | `dnsresolv` |
| `r1_ping_test.py` | `PING.E32` (`ping`) | 80 s | `ping`, `netbridge 20000` |
| `r1_httpget_test.py` | `HTTPGET.E32` (`httpget`) | 90 s | `netbridge 1500`, `httpget`, `netbridge 40000` |
| `r1_wbrowser_test.py` | `WBROWSER.E32` (`wbrowser`) | 90 s | `netbridge 1500`, `wbrowser`, `netbridge 40000` |
| `r1_wget_test.py` | `WGET.E32` (`wget`) | 90 s | `netbridge 1500`, `wget`, `netbridge 40000` |
| `r1_telnet_test.py` | `TELNET.E32` (`telnet`) | 90 s | `netbridge 1500`, `telnet`, `netbridge 40000` |

E32 names are staged as 8.3 short names (`netbr`, `dhcpc`, `dnsnet`, `ping`,
`httpget`, `dnsres`, `wbrowser`, `wget`, `telnet`) because the shell's
`FatReadPath` cannot match longer names; several also stage the uppercase name
for reference. **Timeout note:** the HTTP-class scripts use an inner QEMU
`timeout 90`, but a full run (FAT image build + boot + retries) can take longer —
give these a ~150 s outer wall-clock budget. `r1_netbridge_test.py` retries a
cold boot twice, so allow ~160 s there too.

## Per-test detail

### r1_qemu_nic_test.py — D-06 e1000 bring-up
*Stages:* nothing on disk (kernel only). *Feeder:* `nic arp 3`.
*Markers:* boot `e1000: BAR0=`, `MAC=`, `link=1`; then `nic: e1000 MAC=`,
three `nic arp: try N tx=0 (OK)`, `RX reply`, and
`nic arp: RESULT PASS (3/3 ARP replies)`.
*Pass proves:* the polled e1000 TX/RX path sends repeated ARP requests and
receives genuine SLIRP ARP replies — i.e. the RX-tail reclaim fix works across
multiple packets, not just one.

### r1_netbridge_test.py — kernel↔user frame bridge
*Stages:* `NETBRIDGE.E32` as `netbr` + `EUSER.DLL`. Launches `netbr` (posts a
42-byte who-has 10.0.2.2 into TX) then runs the `netbridge` pump.
*Markers:* `netbridge: base=`, `ord 915`, `netbr started (user process)`,
`netbridge: polling`; `NB: bridge base=`, `NB: ARP request posted to TX ring`,
`NB: ARP reply from 10.0.2.2`, and critically
`NB: RESULT PASS - real ARP reply crossed kernel->user boundary`.
*Pass proves:* a real ARP reply from SLIRP traversed NIC → e1000_recv → kernel
RX ring → userland RX polling, and the exec 247/248 bridge discovery works. No
exception/panic may appear.

### r1_dhcpc_test.py — real DHCP lease
*Stages:* `DHCPC.E32` (`dhcpc`); feeder `dhcpc` then `netbridge 30000`.
*Markers:* `DHCP: bridge base=`, `DISCOVER sent`, `OFFER from ...`,
`REQUEST sent`, `ACK received`, `IP=`, `DONE`.
*Pass proves:* a full DORA exchange with QEMU SLIRP's DHCP server produced a
real 10.0.2.15 lease (with mask/gateway/DNS) and the OFFER/ACK crossed the
kernel→user boundary.

### r1_dnsnet_test.py — real DNS A query over the bridge
*Stages:* `DNSNET.E32` (`dnsnet`); feeder `dnsnet` then `netbridge 25000`.
*Markers:* `DNSNET: DNS client over netbridge start`, `ARP request for gateway
10.0.2.2 posted`, `query sent example.com`, `reply from 10.0.2.3`, `example.com
-> ...`, `DONE`; and no `DNSNET: no reply (timeout)`.
*Pass proves:* a genuine UDP/53 DNS query reached SLIRP's DNS proxy at 10.0.2.3
and the parsed A record came back through the RX ring.

### r1_qemu_dns_test.py — DNSRES loopback resolver
*Stages:* `DNSRES.E32` (`dnsres`); feeder `dnsresolv` (no kernel pump — this is
the earlier `dnsresolv.c` UDP/53 + `DnsResolverServer` IPC design).
*Markers:* `e32run: validate r=0`, `ord 915`, `started (user process)`, ≥3
`DNSRES:` lines including a "DNS resolver" start marker, a `:53`/`UDP` line and
an `->`/`resolved` result line.
*Pass proves:* the PE2 image validates and binds ordinal 915 and the RFC-1035
encode/parse path emits a resolved IP (this test does not require a NIC reply).

### r1_ping_test.py — real ICMP echo
*Stages:* `PING.E32` (`ping`); feeder `ping` then `netbridge 20000`.
*Markers:* `PING: ARP gateway 10.0.2.2`, `PING: echo request seq 1 sent`,
`PING: reply from 10.0.2.2 seq`, `PING: DONE`.
*Pass proves:* at least one real ICMP echo reply with verified IP/ICMP
checksums and matching id/seq crossed kernel→user. The script also requires
`3/3 replies` semantics in its full check.

### r1_httpget_test.py — real TCP handshake + HTTP GET
*Stages:* `HTTPGET.E32` (`httpget`); feeder `netbridge 1500`, `httpget`,
`netbridge 40000`.
*Markers:* bridge base, `SYN sent`, `SYN-ACK received`, `ACK sent`, `GET sent`,
`status 200`, `title Example Domain`, `received N bytes`, `DONE`. Rejects
`handshake timeout` / `HTTP: FAIL`.
*Pass proves:* a complete 3-way handshake with `104.20.23.154:80` through SLIRP
NAT and a real `HTTP/1.0 200` response with the example.com title were
reassembled in userland.

### r1_wbrowser_test.py — real web page rendered
*Stages:* `WBROWSER.E32` (`wbrowser`); same pump/launch/pump feeder.
*Markers:* `WEB: GET http://example.com/`, `status 200`, `title Example
Domain`, the rendered body line `WEB: This domain is for use in documentation
examples without needing`, `bytes`, `DONE`. Rejects fetch-failed/`WEB: FAIL`.
*Pass proves:* the HTTP engine plus URL parsing and HTML-to-text rendering
produced readable real page content (tags stripped, entities decoded, wrapped).

### r1_wget_test.py — real TCP download saved to FAT
*Stages:* `WGET.E32` (`wget`); same feeder, **no `snapshot=on`**. After boot it
uses `mdir`/`mcopy` on the image to pull `::DOWNLOAD.TXT` back and validate it.
*Markers:* `WGET: GET http://example.com/`, bridge base, SYN/SYN-ACK/ACK,
`GET sent`, `status 200`, `body N bytes`, `saved DOWNLOAD.TXT (N bytes)`,
`DONE`. Rejects `fetch failed`/`save failed`.
*Pass proves:* a real HTTP body was reassembled and written through PortFile IPC
to a persistent FAT file that the host could read back.

### r1_telnet_test.py — interactive TCP client
*Stages:* `TELNET.E32` (`telnet`); same feeder. Default target is gateway
`10.0.2.2:80`; an optional `host port` (e.g. `104.20.23.154 80`) can be typed.
*Markers:* required `TNET: connecting to 10.0.2.2:80` and `TNET: DONE`;
connected path `SYN sent`, `SYN-ACK received`, `connected`, `ACK sent`,
`sent N bytes`, then `recv N bytes:` (or a graceful `connect timeout` / `no
data (timeout)`). Rejects `TNET: FAIL`.
*Pass proves:* the generic TCP engine performs a handshake and sends an
interactive line; because nothing listens on 10.0.2.2:80 by default, a clean
`connect timeout` is an accepted graceful outcome, not a failure.

## Adding a new network app

1. **Write the E32** — copy `user/httpget.c` (the canonical raw-frame pattern):
   include `runtime.h` + `netbridge.h`, declare the IAT slot
   (`extern unsigned int <app>_printf_slot;`), an `app_main()` that discovers the
   bridge via `exec2(247/248,0,0)`, validates `magic`/`version`, posts frames with
   a `tx_post()` fence + `head` update, polls RX with `rx_next()` (clear `len`,
   fence, advance `tail`), and sets `gSh->done = 1` before printing `<APP>: DONE`.
2. **Add three `user.mk` rules** (mirroring the HTTPGET block at lines 134–142):
   ```
   build/user/<app>.o: user/<app>.c user/runtime.h user/netbridge.h | build/user
   	$(CC) $(USER_FLAGS) -c $< -o $@
   build/user/<app>.elf: build/user/start.o build/user/runtime.o build/user/<app>.o user/link.ld
   	ld -m elf_i386 --emit-relocs --defsym=<app>_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
   build/user/<APP>.E32: build/user/<app>.elf tools/elf2e32.py tools/mke32.py
   	python3 tools/elf2e32.py $< $@ --euser-import 915
   	python3 tools/e32read.py $@ --quiet
   ```
3. **e32read validation** — the final `tools/e32read.py $@ --quiet` step verifies
   the produced PE2 image (header, IAT/ordinal 915 import) before it is accepted;
   do not skip it.
4. **Add a shell launch command** in `variant/kern_shell.cpp` if you want a
   one-word launcher (the built-in `E32Run` helper prints
   `"<name> started (user process)"` on success), plus a help-table entry.
5. **Add `tools/r1_<app>_test.py`** — clone `r1_httpget_test.py`: build the FAT
   disk and stage the E32 under an 8.3 short name + `EUSER.DLL`, use
   `netbridge 1500` / launch / `netbridge 40000`, define `BOOT_MARKERS` and an app
   marker list, scan `EXC_KEYS`, and print `RESULT: PASS/FAIL`. Give it a ~150 s
   outer timeout.

All builds and tests must leave `kernelhwsrv/` untouched; stage E32 files only on
the test FAT image, not in the repository tree.
