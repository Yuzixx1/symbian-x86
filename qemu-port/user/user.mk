# Included from the port Makefile; no hosted libc or cross SDK needed.
USER_FLAGS := -m32 -march=i486 -Os -ffreestanding -fno-pic -fno-pie -fno-builtin -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables -mpreferred-stack-boundary=2 -Wall -Wextra -Iuser -fno-rtti

.PHONY: userland
userland: build/user/ECHO.E32 build/user/TYPE.E32 build/user/CP.E32 build/user/FILECHK.E32
userland: build/user/IPCSRV.E32 build/user/IPCCLI.E32 build/user/CPSTRESS.E32 build/user/CWD.E32 build/user/CRASH.E32 build/user/MALLOC.E32 build/user/INTERACT.E32 build/user/SLEEP.E32 build/user/CHUNK.E32 build/user/CHKOPEN.E32
userland: build/user/PORTFILESRV.E32
userland: build/user/SVCMGR.E32
userland: build/user/WAITFOR.E32
userland: build/user/KILLPROC.E32
userland: build/user/NETSTACK.E32
userland: build/user/PING.E32
userland: build/user/NETBRIDGE.E32
userland: build/user/DHCPC.E32
userland: build/user/DNSRES.E32
userland: build/user/DNSNET.E32
userland: build/user/TEXTEDIT.E32
userland: build/user/FILEMGR.E32
userland: build/user/CALC.E32
userland: build/user/ca_test.e32
userland: build/user/HTTPGET.E32
userland: build/user/NTPC.E32
userland: build/user/GUISHELL.E32

# S-02: service manager and the F32 service it supervises at boot.
.PHONY: svcmgr
svcmgr: build/user/SVCMGR.E32 build/user/PORTFILESRV.E32

# P-06: user-mode blocking wait via the genuine ProcessLogon executive (123).
build/user/waitfor.elf: build/user/start.o build/user/runtime.o build/user/waitfor.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/WAITFOR.E32: build/user/waitfor.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

# P-08: peer killer fixture — a user process kills another by container id.
build/user/killproc.elf: build/user/start.o build/user/runtime.o build/user/killproc.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/KILLPROC.E32: build/user/killproc.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

# End-user text editor: line-oriented editing over an in-memory buffer, file
# open/save via PortFile IPC (portfilesrv), and a genuine PE2 import of
# EUSER.DLL ordinal 915 (RDebug::Printf) for its serial-console markers.
build/user/textedit.elf: build/user/start.o build/user/runtime.o build/user/textedit.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=euser_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/TEXTEDIT.E32: build/user/textedit.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# End-user text-mode file manager: a known-set directory listing plus
# dir/cat/help/quit commands, file I/O over PortFile IPC (portfilesrv), and a
# genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) for its markers.
build/user/filemgr.elf: build/user/start.o build/user/runtime.o build/user/filemgr.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=euser_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/FILEMGR.E32: build/user/filemgr.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# End-user integer calculator: recursive-descent evaluator (+, -, *, /, ())
# over wrapping 32-bit values, driven by the shell command line or a built-in
# self-test suite. Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf)
# for its serial markers; IAT slot calc_printf_slot at 0x408000 (textSize).
build/user/calc.elf: build/user/start.o build/user/runtime.o build/user/calc.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=calc_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/CALC.E32: build/user/calc.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# R1 netbridge proof E32: posts a raw ARP request into the kernel/user frame
# bridge via execs 247/248, waits for the real e1000 ARP reply in the RX ring.
# PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) carries the markers;
# the IAT slot is the absolute symbol nb_printf_slot at 0x408000 (textSize).
build/user/netbridge_test.o: user/netbridge_test.c user/runtime.h user/netbridge.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@

build/user/netbridge_test.elf: build/user/start.o build/user/runtime.o build/user/netbridge_test.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=nb_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/NETBRIDGE.E32: build/user/netbridge_test.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# GUI desktop shell E32: builds a display list in the low-RAM gui_shared
# buffer and asks the kernel (slow exec 251) to paint the VBE framebuffer.
# PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# guishell_printf_slot at 0x408000 (textSize).
build/user/guishell.o: user/guishell.c user/runtime.h user/guishost.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@

build/user/guishell.elf: build/user/start.o build/user/runtime.o build/user/guishell.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=guishell_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/GUISHELL.E32: build/user/guishell.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# R1 real ICMP ping E32: builds a genuine Ethernet/IPv4/ICMP echo request,
# posts it through the netbridge TX ring to QEMU SLIRP (10.0.2.2), and reads
# the real echo reply out of the RX ring with checksum verification. The IAT
# slot ping_printf_slot at 0x408000 is seeded with EUSER.DLL ordinal 915
# (RDebug::Printf) by elf2e32.py --euser-import 915.
build/user/ping.o: user/ping.c user/runtime.h user/netbridge.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@

build/user/ping.elf: build/user/start.o build/user/runtime.o build/user/ping.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=ping_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/PING.E32: build/user/ping.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# R1 real TCP HTTP client E32: SYN/SYN-ACK/ACK handshake + HTTP/1.0 GET over
# the kernel<->userland e1000/SLIRP frame bridge. PE2 import of EUSER.DLL
# ordinal 915 (RDebug::Printf) carries the serial markers; IAT slot is the
# absolute symbol http_printf_slot at 0x408000 (textSize).
build/user/httpget.o: user/httpget.c user/runtime.h user/netbridge.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@

build/user/httpget.elf: build/user/start.o build/user/runtime.o build/user/httpget.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=http_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/HTTPGET.E32: build/user/httpget.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# R1 DHCP client E32: real DORA over the kernel<->user e1000 frame bridge.
# Builds a broadcast Ethernet/IPv4/UDP/BOOTP DISCOVER, reads the SLIRP OFFER,
# sends a REQUEST and reads the ACK. PE2 import of EUSER.DLL ordinal 915
# (RDebug::Printf) carries the markers; IAT slot dhcp_printf_slot at 0x408000.
build/user/dhcpc.o: user/dhcpc.c user/runtime.h user/netbridge.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@

build/user/dhcpc.elf: build/user/start.o build/user/runtime.o build/user/dhcpc.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=dhcp_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/DHCPC.E32: build/user/dhcpc.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# R1 DNS resolver E32: RFC1035 UDP/53 client + DnsResolverServer IPC service.
# PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) carries the markers;
# the IAT slot is the absolute symbol dns_printf_slot at 0x408000 (textSize).
build/user/dnsresolv.o: user/dnsresolv.c user/runtime.h user/ipc.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@

build/user/dnsresolv.elf: build/user/start.o build/user/runtime.o build/user/ipc.o build/user/dnsresolv.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=dns_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/DNSRES.E32: build/user/dnsresolv.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# R1 REAL DNS client E32: posts a genuine ARP + UDP/53 A query for example.com
# through the kernel<->userland e1000 netbridge and parses the reply from the
# RX ring. PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# dnsnet_printf_slot at 0x408000 (textSize).
build/user/dnsnet.o: user/dnsnet.c user/runtime.h user/netbridge.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@

build/user/dnsnet.elf: build/user/start.o build/user/runtime.o build/user/dnsnet.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=dnsnet_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/DNSNET.E32: build/user/dnsnet.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# R1 REAL NTP time-sync client E32: ARP gateway 10.0.2.2, then build a genuine
# Ethernet/IPv4/UDP/123 48-byte NTP v4 client request with the transmit
# timestamp from exec 68 (TTime -> 1900 epoch). Parses the real SLIRP reply for
# stratum/mode/T2/T3, reads local T4 and reports the T1/T2/T3/T4 offset in ms.
# PE2 import of EUSER.DLL ordinal 915; IAT slot ntpc_printf_slot at 0x408000.
build/user/ntpc.o: user/ntpc.c user/runtime.h user/netbridge.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@

build/user/ntpc.elf: build/user/start.o build/user/runtime.o build/user/ntpc.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=ntpc_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/NTPC.E32: build/user/ntpc.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# R1 web-browser fixture: user/ca_test.c is the text-mode browser (DNS A lookup,
# TCP/HTTP via !SocketServer, HTML tag-strip renderer).  Built two ways:
#   ca_test_static.e32 -- compiled/import-free proof via elf2e32.py.
#   ca_test.e32        -- genuine PE2 image importing EUSER.DLL ordinal 915
#                         (RDebug::Printf), the fixture used by the QEMU test.
build/user/ca_test.o: user/ca_test.c user/runtime.h user/ipc.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@

build/user/ca_test.elf: build/user/start.o build/user/runtime.o build/user/ipc.o build/user/malloc.o build/user/euser_shim.o build/user/ca_test.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/ca_test_static.e32: build/user/ca_test.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

build/user/ca_test.e32: tools/mke32_webbrowser.py tools/mke32.py
	python3 tools/mke32_webbrowser.py --output $@
	python3 tools/e32read.py $@ --quiet

build/user/ipc.o: user/ipc.h
build/user/ipcsrv.o build/user/ipccli.o build/user/cpstress.o: user/ipc.h
build/user/netstack.o: user/ipc.h

build/user/ipcsrv.elf: build/user/start.o build/user/runtime.o build/user/ipc.o build/user/ipcsrv.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/ipccli.elf: build/user/start.o build/user/runtime.o build/user/ipc.o build/user/ipccli.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/IPCSRV.E32: build/user/ipcsrv.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

build/user/IPCCLI.E32: build/user/ipccli.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

# TCP/IP stack E32 (IPv4/IPv6 + ICMP echo + IPC result export) linked against
# EUSER.DLL through PE2 ordinal imports (915 markers, 617/626 IPC,
# 925/928/929 timing). The IAT lives at the end of the text run via
# user/link_netstack.ld; netstack_iat.S holds the 1-based ordinals.
build/user/netstack_iat.o: user/netstack_iat.S | build/user
	$(CC) -m32 -c $< -o $@

build/user/netstack.elf: build/user/start.o build/user/runtime.o build/user/ipc.o build/user/netstack_iat.o build/user/netstack.o user/link_netstack.ld
	ld -m elf_i386 --emit-relocs -T user/link_netstack.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/NETSTACK.E32: build/user/netstack.elf tools/mke32_netstack_elf.py tools/mke32.py
	python3 tools/mke32_netstack_elf.py $< $@
	python3 tools/e32read.py $@ --quiet

# P-08: long-lived concurrent IPC stress client.
build/user/cpstress.elf: build/user/start.o build/user/runtime.o build/user/ipc.o build/user/cpstress.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/CPSTRESS.E32: build/user/cpstress.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

build/user:
	mkdir -p $@

build/user/%.o: user/%.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@

# S-04: euser_shim.cpp and portfilesrv.cpp are C++ — they need $(CXX), not $(CC).
build/user/%.o: user/%.cpp user/e32base.h | build/user
	$(CXX) $(USER_FLAGS) -c $< -o $@

build/user/runtime.o: user/runtime.c
build/user/ipc.o: user/ipc.c user/ipc.h

build/user/start.o: user/start.S | build/user
	$(CC) -m32 -c $< -o $@

build/user/echo.elf: build/user/start.o build/user/runtime.o build/user/echo.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/ECHO.E32: build/user/echo.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

build/user/type.elf: build/user/start.o build/user/runtime.o build/user/type.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/TYPE.E32: build/user/type.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

build/user/cp.elf: build/user/start.o build/user/runtime.o build/user/cp.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/CP.E32: build/user/cp.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

build/user/filecheck.elf: build/user/start.o build/user/runtime.o build/user/filecheck.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/FILECHK.E32: build/user/filecheck.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

build/user/cwd.elf: build/user/start.o build/user/runtime.o build/user/cwd.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/CWD.E32: build/user/cwd.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

build/user/crash.elf: build/user/start.o build/user/runtime.o build/user/crash.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/CRASH.E32: build/user/crash.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

build/user/interact.elf: build/user/start.o build/user/runtime.o build/user/interact.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/INTERACT.E32: build/user/interact.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

build/user/sleep.elf: build/user/start.o build/user/runtime.o build/user/sleep.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/SLEEP.E32: build/user/sleep.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

build/user/chunk.elf: build/user/start.o build/user/runtime.o build/user/chunk.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/CHUNK.E32: build/user/chunk.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

build/user/chunkopen.elf: build/user/start.o build/user/runtime.o build/user/chunkopen.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/CHKOPEN.E32: build/user/chunkopen.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

build/user/malloc.elf: build/user/start.o build/user/runtime.o build/user/malloc.o build/user/malloctest.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/MALLOC.E32: build/user/malloc.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

# S-04: PORTFILESRV.E32 — EUSER shim user host (E32 file server process).
build/user/euser_shim.o: user/euser_shim.cpp user/e32base.h user/e32cmn.h user/f32ver.h
build/user/portfilesrv.o: user/portfilesrv.cpp user/e32base.h user/e32cmn.h user/f32ver.h user/ipc.h user/runtime.h fs/r1_fs_server.h fs/r1_fs_core.h | build/user
	$(CXX) $(USER_FLAGS) -Iuser -Ifs -c $< -o $@

# I-08: genuine CServer2/CSession2 file server (fs/r1_fs_server.cpp).
build/user/r1_fs_server.o: fs/r1_fs_server.cpp fs/r1_fs_server.h fs/r1_fs_core.h user/e32base.h user/e32cmn.h user/f32ver.h | build/user
	$(CXX) $(USER_FLAGS) -Iuser -Ifs -c $< -o $@

build/user/portfilesrv.elf: build/user/start.o build/user/runtime.o build/user/ipc.o build/user/euser_shim.o build/user/portfilesrv.o build/user/malloc.o build/user/r1_fs_core.o build/user/r1_fs_server.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/PORTFILESRV.E32: build/user/portfilesrv.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

# S-02: service manager E32 (ordered startup + supervision + shutdown, plus
# FileServer host mode). Links the EUSER/CServer2 shim and the R1 FS engine.
build/user/svcmgr.o: user/svcmgr.cpp user/e32base.h user/e32cmn.h user/f32ver.h user/ipc.h user/runtime.h fs/r1_fs_server.h fs/r1_fs_core.h | build/user
	$(CXX) $(USER_FLAGS) -Iuser -Ifs -c $< -o $@

build/user/svcmgr.elf: build/user/start.o build/user/runtime.o build/user/ipc.o build/user/euser_shim.o build/user/svcmgr.o build/user/malloc.o build/user/r1_fs_core.o build/user/r1_fs_server.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/SVCMGR.E32: build/user/svcmgr.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

# S-04: malloc.o for the EUSER shim
build/user/malloc.o: user/malloc.c
	$(CC) $(USER_FLAGS) -c $< -o $@

# S-04: r1_fs_core.o from fs/ directory
build/user/r1_fs_core.o: fs/r1_fs_core.cpp
	$(CXX) $(USER_FLAGS) -Ifs -c $< -o $@

# End-user system-information utility: CPUID (leaves 0/1), genuine tick exec,
# heap chunk probe, netbridge/e1000 probe. Genuine PE2 import of EUSER.DLL
# ordinal 915 (RDebug::Printf); IAT slot sysinfo_printf_slot at 0x408000.
userland: build/user/SYSINFO.E32
build/user/sysinfo.o: user/netbridge.h
build/user/sysinfo.elf: build/user/start.o build/user/runtime.o build/user/sysinfo.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=sysinfo_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/SYSINFO.E32: build/user/sysinfo.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# Sound player E32: plays a four-note melody by calling the kernel's AC97
# tone exec (slot 240, host variant/qemu_audio_exec.cpp). Genuine PE2 import
# of EUSER.DLL ordinal 915 (RDebug::Printf) for the serial markers; IAT slot
# soundplr_printf_slot at 0x408000 (textSize).
userland: build/user/SOUNDPLR.E32
build/user/soundplr.o: user/soundplr.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/soundplr.elf: build/user/start.o build/user/runtime.o build/user/soundplr.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=soundplr_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/SOUNDPLR.E32: build/user/soundplr.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# APPLICATION LAUNCHER / BOOT MENU: numbered E32 app list, PortCon selection,
# launches via trusted exec 268 (SvcMgrLaunch). Genuine PE2 import of EUSER.DLL
# ordinal 915 (RDebug::Printf) for markers; IAT slot applaunch_printf_slot.
userland: build/user/APPLAUNCH.E32
build/user/applaunch.o: user/applaunch.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/applaunch.elf: build/user/start.o build/user/runtime.o build/user/applaunch.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=applaunch_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/APPLAUNCH.E32: build/user/applaunch.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# END-USER FILE COPY / FILE UTILITY (FC): copy/cat/del/dir/help/quit plus a
# built-in copy self-test, all over PortFile IPC (portfilesrv). Genuine PE2
# import of EUSER.DLL ordinal 915 (RDebug::Printf) for the serial markers;
# the IAT slot fcopy_printf_slot is defined at 0x408000 (textSize).
userland: build/user/FCOPY.E32
build/user/fcopy.o: user/fcopy.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/fcopy.elf: build/user/start.o build/user/runtime.o build/user/fcopy.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=fcopy_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/FCOPY.E32: build/user/fcopy.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# CPU BENCHMARK E32: Dhrystone-style integer workload (struct copies, string
# ops, array indexing, recursion, integer mul/div), timed via exec 68
# (EExecTimeNow microseconds) and reported at an assumed HZ=100. Genuine PE2
# import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot bench_printf_slot.
userland: build/user/BENCH.E32
build/user/bench.o: user/bench.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/bench.elf: build/user/start.o build/user/runtime.o build/user/bench.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=bench_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/BENCH.E32: build/user/bench.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# End-user CLOCK / TIME utility: reads the genuine EExecTickCount executive
# (slow exec 5, 64 Hz system tick) and renders HH:MM:SS uptime
# plus three sampled segments driven by the genuine TimerAfter exec; an optional
# wall-clock face comes from EExecTimeNow (exec 68). Genuine PE2 import of
# EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot clock_printf_slot at 0x408000.
userland: build/user/CLOCK.E32
build/user/clock.o: user/clock.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/clock.elf: build/user/start.o build/user/runtime.o build/user/clock.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=clock_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/CLOCK.E32: build/user/clock.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# End-user CALENDAR utility: derives the Gregorian date/month grid from the
# genuine EExecTimeNow executive (exec 68, TTime microseconds since 0AD) with
# a 400-year-era civil-from-days algorithm and Gregorian 4/100/400 leap rules.
# The raw microseconds and assumed 0AD epoch are printed for transparency.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# cal_printf_slot at 0x408000 (textSize).
userland: build/user/CAL.E32
build/user/cal.o: user/cal.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/cal.elf: build/user/start.o build/user/runtime.o build/user/cal.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=cal_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/CAL.E32: build/user/cal.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# WGET-style downloader E32: real TCP/HTTP GET over the kernel<->userland
# e1000/SLIRP frame bridge (httpget engine), reassembled response body saved to
# a FAT file via PortFile IPC (file_save). Genuine PE2 import of EUSER.DLL
# ordinal 915 (RDebug::Printf) for the serial markers; IAT slot
# wget_printf_slot at 0x408000 (textSize).
userland: build/user/WGET.E32
build/user/wget.o: user/wget.c user/runtime.h user/netbridge.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/wget.elf: build/user/start.o build/user/runtime.o build/user/wget.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=wget_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/WGET.E32: build/user/wget.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# REAL web browser E32: takes an optional URL from PortCon (default
# http://example.com/), does a genuine TCP handshake + HTTP/1.0 GET over the
# e1000/SLIRP netbridge and renders the response as wrapped plain text (tags
# stripped, entities decoded). Genuine PE2 import of EUSER.DLL ordinal 915
# (RDebug::Printf); IAT slot wbrowser_printf_slot at 0x408000 (textSize).
userland: build/user/WBROWSER.E32
build/user/wbrowser.o: user/wbrowser.c user/runtime.h user/netbridge.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@

build/user/wbrowser.elf: build/user/start.o build/user/runtime.o build/user/wbrowser.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=wbrowser_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/WBROWSER.E32: build/user/wbrowser.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# TELNET-style interactive TCP client E32: real SYN/SYN-ACK/ACK handshake over
# the kernel<->userland e1000/SLIRP frame bridge, then sends a greeting line and
# polls RX for echoed/response text. Optional "host port" (or "quit") from the
# shell command line / PortCon. Genuine PE2 import of EUSER.DLL ord 915;
# IAT slot telnet_printf_slot at 0x408000 (textSize).
userland: build/user/TELNET.E32
build/user/telnet.o: user/telnet.c user/runtime.h user/netbridge.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@

build/user/telnet.elf: build/user/start.o build/user/runtime.o build/user/telnet.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=telnet_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/TELNET.E32: build/user/telnet.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# MEMORY TEST / HEAP STRESS E32: walk (128 address-stamped blocks), churn
# (2000 mixed-size allocate/verify/free cycles), and a chunk-backed canary
# bounds check. Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf);
# IAT slot memtest_printf_slot at 0x408000 (textSize).
userland: build/user/MEMTEST.E32
build/user/memtest.o: user/memtest.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@

build/user/memtest.elf: build/user/start.o build/user/runtime.o build/user/malloc.o build/user/memtest.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=memtest_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/MEMTEST.E32: build/user/memtest.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# END-USER FILE SEARCH / GREP (FG): line-oriented literal substring search over
# a FAT text file via PortFile IPC, plus an embedded built-in self-test. Genuine
# PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) for the serial markers;
# the IAT slot fgrep_printf_slot is defined at 0x408000 (textSize).
userland: build/user/FGREP.E32
build/user/fgrep.o: user/fgrep.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/fgrep.elf: build/user/start.o build/user/runtime.o build/user/fgrep.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=fgrep_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/FGREP.E32: build/user/fgrep.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# END-USER HEX DUMP UTILITY (HD): canonical 16-byte hex + ASCII dumps of a FAT
# file via PortFile IPC, plus a built-in dump of an embedded byte array. Genuine
# PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) for the serial markers;
# the IAT slot hexdump_printf_slot is defined at 0x408000 (textSize).
userland: build/user/HEXDUMP.E32
build/user/hexdump.o: user/hexdump.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/hexdump.elf: build/user/start.o build/user/runtime.o build/user/hexdump.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=hexdump_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/HEXDUMP.E32: build/user/hexdump.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# SCREENSAVER / ANIMATION E32: genuine user-driven graphics on the real VBE
# LFB through the GUI slow executives 249/250/251. PE2 import of EUSER.DLL
# ordinal 915 (RDebug::Printf); IAT slot saver_printf_slot at 0x408000.
userland: build/user/SAVER.E32
build/user/saver.o: user/saver.c user/runtime.h user/guishost.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/saver.elf: build/user/start.o build/user/runtime.o build/user/saver.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=saver_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/SAVER.E32: build/user/saver.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# MINESWEEPER E32 -- genuine interactive 8x8/10-mine game driven from PortCon
# (new / open / flag / board / quit) with a no-input built-in auto-demo. Genuine
# PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# mines_printf_slot at 0x408000 (textSize).
userland: build/user/MINES.E32
build/user/mines.o: user/mines.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/mines.elf: build/user/start.o build/user/runtime.o build/user/mines.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=mines_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/MINES.E32: build/user/mines.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# PROCESS VIEWER (PS) E32 -- userland process census from genuine kernel exec
# 241 (variant/qemu_ps_exec.cpp): processes / channels / code segs plus a
# bounded process id+name list. Genuine PE2 import of EUSER.DLL ordinal 915
# (RDebug::Printf); IAT slot ps_printf_slot at 0x408000 (textSize).
userland: build/user/PS.E32
build/user/ps.o: user/ps.c user/runtime.h user/pshost.h | build/user
build/user/ps.elf: build/user/start.o build/user/runtime.o build/user/ps.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=ps_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/PS.E32: build/user/ps.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet
