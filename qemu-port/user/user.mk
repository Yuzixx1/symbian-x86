# Included from the port Makefile; no hosted libc or cross SDK needed.
USER_FLAGS := -m32 -march=i486 -Os -ffreestanding -fno-pic -fno-pie -fno-builtin -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables -mpreferred-stack-boundary=2 -Wall -Wextra -Iuser -fno-rtti

.PHONY: userland
userland: build/user/ECHO.E32 build/user/TYPE.E32 build/user/CP.E32 build/user/FILECHK.E32
userland: build/user/IPCSRV.E32 build/user/IPCCLI.E32 build/user/CPSTRESS.E32 build/user/CWD.E32 build/user/CRASH.E32 build/user/MALLOC.E32 build/user/INTERACT.E32 build/user/SLEEP.E32 build/user/CHUNK.E32 build/user/CHKOPEN.E32
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
userland: build/user/SELFTEST.E32

# End-user OS self-test: one program that exercises EUSER binding, the user
# heap, RChunk, PortFile file save/read, the IPC server/client round trip and
# the e1000 netbridge discovery, then prints a single PASS/FAIL verdict.
build/user/selftest.elf: build/user/start.o build/user/runtime.o build/user/ipc.o build/user/malloc.o build/user/selftest.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=selftest_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/SELFTEST.E32: build/user/selftest.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

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

# R1 DNS resolver E32: the REAL network resolver.  DNSRES.E32 is now the
# genuine RFC1035 A-record client over the kernel<->user e1000/SLIRP netbridge
# (user/dnsnet.c): ARP the 10.0.2.2 gateway, send UDP/53 to SLIRP's DNS proxy
# 10.0.2.3 and parse the real reply from the RX ring.  The old loopback-
# responder fake survives only as user/dnsresolv.c (historical; NOT built).
# PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) carries the markers;
# the IAT slot is the absolute symbol dnsnet_printf_slot at 0x408000 (textSize).
build/user/DNSRES.E32: build/user/dnsnet.elf tools/elf2e32.py tools/mke32.py
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

# S-04: EUSER shim object (still linked into ca_test.elf).
build/user/euser_shim.o: user/euser_shim.cpp user/e32base.h user/e32cmn.h user/f32ver.h

# I-08: genuine CServer2/CSession2 file server (fs/r1_fs_server.cpp).
build/user/r1_fs_server.o: fs/r1_fs_server.cpp fs/r1_fs_server.h fs/r1_fs_core.h user/e32base.h user/e32cmn.h user/f32ver.h | build/user
	$(CXX) $(USER_FLAGS) -Iuser -Ifs -c $< -o $@

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

# PCM sample playback proof E32: generates 1 s of 44.1 kHz 16-bit stereo 440 Hz
# audio in user low RAM, submits it via exec 240 op AU_PCM_PLAY (async), polls
# AU_PCM_STATUS until DMA completion, then AU_PCM_STOP. Genuine PE2 import of
# EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot audiopcm_printf_slot == 0x408000.
userland: build/user/AUDIOPCM.E32
build/user/audiopcm.o: user/audiopcm.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/audiopcm.elf: build/user/start.o build/user/runtime.o build/user/audiopcm.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=audiopcm_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/AUDIOPCM.E32: build/user/audiopcm.elf tools/elf2e32.py tools/mke32.py
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

# IFCONFIG E32: IFCONFIG-style network configuration utility. Probes the e1000
# through the netbridge exec (247) and reports the SLIRP lease (IP/mask/GW/DNS)
# plus the EEPROM MAC. Genuine PE2 import of EUSER.DLL ordinal 915
# (RDebug::Printf); IAT slot ifconfig_printf_slot at 0x408000 (textSize).
userland: build/user/IFCONFIG.E32
build/user/ifconfig.o: user/ifconfig.c user/runtime.h user/netbridge.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/ifconfig.elf: build/user/start.o build/user/runtime.o build/user/ifconfig.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=ifconfig_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/IFCONFIG.E32: build/user/ifconfig.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# ADVANCED NOTEPAD E32 -- multi-file editor with literal search/replace,
# driven from PortCon (new/open/insert/show/find/replace/save/quit/selftest).
# File I/O over PortFile IPC (portfilesrv); built-in insert/find/replace/save/
# open self-test. Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf)
# for the serial markers; IAT slot notepad_printf_slot at 0x408000 (textSize).
userland: build/user/NOTEPAD.E32
build/user/notepad.o: user/notepad.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/notepad.elf: build/user/start.o build/user/runtime.o build/user/notepad.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=notepad_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/NOTEPAD.E32: build/user/notepad.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# TASK MANAGER (TM) E32 -- interactive process killer. Reads the live census
# from genuine kernel exec 241 (user/pshost.h) and kills a pid via exec 269
# (SvcMgrKillExec -> E32Kill); PortCon commands: list / kill <pid> / selftest /
# help / quit. Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf);
# IAT slot taskmgr_printf_slot at 0x408000 (textSize).
userland: build/user/TASKMGR.E32
build/user/taskmgr.o: user/taskmgr.c user/runtime.h user/pshost.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/taskmgr.elf: build/user/start.o build/user/runtime.o build/user/taskmgr.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=taskmgr_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/TASKMGR.E32: build/user/taskmgr.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# SNAKE E32 -- genuine interactive 16x8 snake game driven from PortCon
# (w/a/s/d / board / new / quit) with a deterministic food layout, growth and
# a no-input built-in auto-demo that steers into the wall. Genuine PE2 import
# of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot snake_printf_slot at
# 0x408000 (textSize).
userland: build/user/SNAKE.E32
build/user/snake.o: user/snake.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/snake.elf: build/user/start.o build/user/runtime.o build/user/snake.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=snake_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/SNAKE.E32: build/user/snake.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# UNIT CONVERTER E32 -- interactive fixed-point (scale 1000) length/temperature/
# mass converter driven from PortCon (len/temp/mass/help/selftest/quit) with a
# built-in selftest (1000 m -> ft, 100 C -> F = 212, 1 kg -> lb). Genuine PE2
# import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot units_printf_slot
# at 0x408000 (textSize).
userland: build/user/UNITS.E32
build/user/units.o: user/units.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/units.elf: build/user/start.o build/user/runtime.o build/user/units.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=units_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/UNITS.E32: build/user/units.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# SCREENSHOT E32: discovers the real VBE framebuffer via GUI slow execs
# 249/250, paints a 64x64 patch through exec 251 (ring-0 LFB write) and saves
# the captured raw RGBA bytes to SCREEN.RAW over PortFile IPC. Genuine PE2
# import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# shot_printf_slot at 0x408000 (textSize).
userland: build/user/SHOT.E32
build/user/shot.o: user/shot.c user/runtime.h user/guishost.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/shot.elf: build/user/start.o build/user/runtime.o build/user/shot.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=shot_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/SHOT.E32: build/user/shot.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# PASSWORD MANAGER / CREDENTIAL VAULT (PV) E32 -- interactive credential vault
# driven from PortCon (unlock/lock/add/list/get/save/load/selftest/quit). Records
# are XOR-obfuscated and persisted as VAULT.DAT over PortFile IPC (portfilesrv);
# built-in unlock/add/list/save/load/verify self-test. Genuine PE2 import of
# EUSER.DLL ordinal 915 (RDebug::Printf) for the serial markers; IAT slot
# pv_printf_slot at 0x408000 (textSize).
userland: build/user/PWVAULT.E32
build/user/pwvault.o: user/pwvault.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/pwvault.elf: build/user/start.o build/user/runtime.o build/user/pwvault.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=pv_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/PWVAULT.E32: build/user/pwvault.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# MORSE CODE tool E32: text -> International Morse (A-Z, 0-9) with encode/play
# over PortCon plus a built-in SOS selftest. Play calls the AC97 tone exec 240
# (600 Hz, dot 100 ms / dash 300 ms) with a graceful fallback when unavailable.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# morse_printf_slot at 0x408000 (textSize).
userland: build/user/MORSE.E32
build/user/morse.o: user/morse.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/morse.elf: build/user/start.o build/user/runtime.o build/user/morse.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=morse_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/MORSE.E32: build/user/morse.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# END-USER ADDRESS BOOK (AB) E32: interactive add/list/find/save/load/quit over
# an in-memory (name, phone) book, persisted as ADDR.TXT on FAT via PortFile IPC
# (portfilesrv), plus a built-in add/save/load/print self-test. Genuine PE2
# import of EUSER.DLL ordinal 915 (RDebug::Printf) for the serial markers; IAT
# slot addrbook_printf_slot at 0x408000 (textSize).
userland: build/user/ADDRBOOK.E32
build/user/addrbook.o: user/addrbook.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/addrbook.elf: build/user/start.o build/user/runtime.o build/user/addrbook.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=addrbook_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/ADDRBOOK.E32: build/user/addrbook.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# PRIME NUMBER tool E32: genuine trial-division prime engine driven from PortCon
# (list <n> / check <num> / primes <lo> <hi> / selftest / help / quit) with a
# built-in self-test (first 10 primes = 2..29, 97 prime, 1-30 = 10 primes).
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) for the serial
# markers; IAT slot primes_printf_slot at 0x408000 (textSize).
userland: build/user/PRIMES.E32
build/user/primes.o: user/primes.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/primes.elf: build/user/start.o build/user/runtime.o build/user/primes.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=primes_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/PRIMES.E32: build/user/primes.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# FORTUNE COOKIE E32 -- end-user app: 16 built-in one-line fortunes selected via
# the genuine kernel MathRandom executive (exec 11) with a tick-count fallback
# (exec 5); PortCon commands next/selftest/help/quit, built-in 3-fortune selftest.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) for the serial
# markers; IAT slot fortune_printf_slot at 0x408000 (textSize).
userland: build/user/FORTUNE.E32
build/user/fortune.o: user/fortune.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/fortune.elf: build/user/start.o build/user/runtime.o build/user/fortune.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=fortune_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/FORTUNE.E32: build/user/fortune.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# ASCII ART generator E32: interactive banner (built-in 5x5 pixel font for
# A-Z/0-9) and cowsay over PortCon, with a built-in "EKA2" selftest. Genuine
# PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) for the serial markers;
# IAT slot asciiart_printf_slot at 0x408000 (textSize).
userland: build/user/ASCIIART.E32
build/user/asciiart.o: user/asciiart.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/asciiart.elf: build/user/start.o build/user/runtime.o build/user/asciiart.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=asciiart_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/ASCIIART.E32: build/user/asciiart.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# STOPWATCH / TIMER E32 -- interactive stopwatch driven from PortCon
# (start/stop/lap/reset/show/selftest/help/quit), timed off the genuine
# EExecTickCount executive (slow exec 5, 64 Hz system tick). Built-in selftest
# starts, busy-waits, records two laps and stops with a nonzero elapsed time.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) for the serial
# markers; IAT slot stopwatch_printf_slot at 0x408000 (textSize).
userland: build/user/STOPWATCH.E32
build/user/stopwatch.o: user/stopwatch.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/stopwatch.elf: build/user/start.o build/user/runtime.o build/user/stopwatch.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=stopwatch_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/STOPWATCH.E32: build/user/stopwatch.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# TIC-TAC-TOE E32 -- genuine interactive 3x3 game driven from PortCon
# (new / move r c / board / selftest / quit). Human X vs a simple CPU O that
# prefers the centre then takes the first free cell; the built-in selftest
# plays a full game to a scripted win. Genuine PE2 import of EUSER.DLL ordinal
# 915 (RDebug::Printf); IAT slot tictactoe_printf_slot at 0x408000 (textSize).
userland: build/user/TICTACTOE.E32
build/user/tictactoe.o: user/tictactoe.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/tictactoe.elf: build/user/start.o build/user/runtime.o build/user/tictactoe.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=tictactoe_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/TICTACTOE.E32: build/user/tictactoe.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# HANGMAN E32 -- genuine interactive word game driven from PortCon
# (new / guess <letter> / board / selftest / quit).  Words come from a built-in
# dictionary chosen with the genuine kernel MathRandom executive (exec 11) with
# a system-clock fallback; six wrong guesses are allowed.  Built-in selftest
# plays a winning game on the known word KERNEL.  Genuine PE2 import of
# EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot hangman_printf_slot at
# 0x408000 (textSize).
userland: build/user/HANGMAN.E32
build/user/hangman.o: user/hangman.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/hangman.elf: build/user/start.o build/user/runtime.o build/user/hangman.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=hangman_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/HANGMAN.E32: build/user/hangman.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# WORD SEARCH (E32) -- genuine interactive 10x10 word-find puzzle driven from
# PortCon (board / find <word> / words / selftest / quit).  A fixed grid hides
# five horizontal/vertical words; the built-in selftest finds all five.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# wordsearch_printf_slot at 0x408000 (textSize).
userland: build/user/WORDSEARCH.E32
build/user/wordsearch.o: user/wordsearch.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/wordsearch.elf: build/user/start.o build/user/runtime.o build/user/wordsearch.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=wordsearch_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/WORDSEARCH.E32: build/user/wordsearch.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# SUDOKU E32 -- end-user 9x9 sudoku game/validator driven from PortCon
# (board / set <r> <c> <1-9> / check / solve / selftest / quit) with a built-in
# classic puzzle, duplicate row/column/box detection and a completed solution
# grid.  Built-in selftest checks the prefilled puzzle, places r1c3=4, checks
# again and solves.  Genuine PE2 import of EUSER.DLL ordinal 915
# (RDebug::Printf); IAT slot sudoku_printf_slot at 0x408000 (textSize).
userland: build/user/SUDOKU.E32
build/user/sudoku.o: user/sudoku.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/sudoku.elf: build/user/start.o build/user/runtime.o build/user/sudoku.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=sudoku_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/SUDOKU.E32: build/user/sudoku.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# CURRENCY CONVERTER E32 -- interactive fixed-point (scale 1000) currency
# converter with a static exchange-rate table relative to USD. PortCon commands
# conv <amount> <from> <to> / rates / help / selftest / quit, with a built-in
# selftest (100 USD->EUR, 100 EUR->USD round trip, 1000 JPY->USD). Genuine PE2
# import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot currency_printf_slot
# at 0x408000 (textSize).
userland: build/user/CURRENCY.E32
build/user/currency.o: user/currency.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/currency.elf: build/user/start.o build/user/runtime.o build/user/currency.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=currency_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/CURRENCY.E32: build/user/currency.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# SOLAR SYSTEM fact viewer E32 -- interactive end-user educational app with a
# built-in table of 8 planets (diameter / distance from Sun / day / moons) driven
# from PortCon (planets / info <name> / selftest / help / quit).  The built-in
# selftest prints the Earth and Jupiter fact rows. Genuine PE2 import of
# EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot solar_printf_slot at 0x408000.
userland: build/user/SOLAR.E32
build/user/solar.o: user/solar.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/solar.elf: build/user/start.o build/user/runtime.o build/user/solar.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=solar_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/SOLAR.E32: build/user/solar.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# DICE ROLLER / RANDOM tool E32 -- end-user app driven from PortCon
# (roll <NdN> / flip / pick <a,b,c,...> / selftest / help / quit). Randomness
# comes from the genuine kernel MathRandom executive (exec 11) with an
# EExecTickCount fallback (exec 5), reduced modulo faces/choices. Built-in
# selftest rolls 2d6, flips a coin and picks from a list. Genuine PE2 import
# of EUSER.DLL ordinal 915 (RDebug::Printf) for the serial markers; IAT slot
# dice_printf_slot at 0x408000 (textSize).
userland: build/user/DICE.E32
build/user/dice.o: user/dice.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/dice.elf: build/user/start.o build/user/runtime.o build/user/dice.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=dice_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/DICE.E32: build/user/dice.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# BATTLESHIP E32 -- genuine interactive 8x8 game driven from PortCon
# (new / fire r c / board / selftest / quit).  Three fixed length-3 ships,
# deterministic CPU return fire, and a built-in selftest that sinks the whole
# fleet for a WIN.  Genuine PE2 import of EUSER.DLL ordinal 915
# (RDebug::Printf); IAT slot battle_printf_slot at 0x408000 (textSize).
userland: build/user/BATTLE.E32
build/user/battle.o: user/battle.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/battle.elf: build/user/start.o build/user/runtime.o build/user/battle.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=battle_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/BATTLE.E32: build/user/battle.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# END-USER RECIPE BOOK (RB) E32 -- five built-in recipes (name, ingredients,
# steps) driven from PortCon (list / show <name> / save / load / selftest /
# help / quit).  Names + ingredients persist as RECIPES.TXT on FAT via PortFile
# IPC (portfilesrv); built-in save/load/verify self-test. Genuine PE2 import of
# EUSER.DLL ordinal 915 (RDebug::Printf) for the serial markers; IAT slot
# recipes_printf_slot at 0x408000 (textSize).
userland: build/user/RECIPES.E32
build/user/recipes.o: user/recipes.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/recipes.elf: build/user/start.o build/user/runtime.o build/user/recipes.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=recipes_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/RECIPES.E32: build/user/recipes.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# MOVIE DATABASE E32 -- end-user viewer over a built-in table of eight classic
# films (title, year, director, genre); PortCon commands list / info <title-
# substring> / genre <name> / selftest / help / quit. The built-in selftest
# lists all eight entries and queries the Sci-Fi genre. Genuine PE2 import of
# EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot movies_printf_slot at
# 0x408000 (textSize).
userland: build/user/MOVIES.E32
build/user/movies.o: user/movies.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/movies.elf: build/user/start.o build/user/runtime.o build/user/movies.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=movies_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/MOVIES.E32: build/user/movies.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# PERIODIC TABLE viewer E32 -- interactive end-user educational app with a
# built-in table of the first 20 elements (symbol / name / Z / atomic mass)
# plus an iron fact row, driven from PortCon (list / info <symbol> / selftest /
# help / quit).  The built-in selftest prints the H, C, O and Fe fact rows.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# periodic_printf_slot at 0x408000 (textSize).
userland: build/user/PERIODIC.E32
build/user/periodic.o: user/periodic.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/periodic.elf: build/user/start.o build/user/runtime.o build/user/periodic.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=periodic_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/PERIODIC.E32: build/user/periodic.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# ZODIAC / HOROSCOPE viewer E32 -- end-user app with a built-in table of the 12
# western signs (name / dates / element) and a deterministic daily horoscope:
# a fixed seed mixed with the sign name through an LCG selects one of eight
# one-line readings. PortCon commands sign <name> / list / horoscope <name> /
# selftest / help / quit; the built-in selftest prints the Aries row + reading.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# zodiac_printf_slot at 0x408000 (textSize).
userland: build/user/ZODIAC.E32
build/user/zodiac.o: user/zodiac.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/zodiac.elf: build/user/start.o build/user/runtime.o build/user/zodiac.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=zodiac_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/ZODIAC.E32: build/user/zodiac.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# OCEAN / GEOGRAPHY fact viewer E32 -- interactive end-user educational app with
# a built-in table of 5 oceans (surface area / max depth) and 5 highest mountains
# (height / range), driven from PortCon (oceans / mountains / info <name> /
# selftest / help / quit). The built-in selftest prints the Pacific ocean and
# Everest fact rows. Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf);
# IAT slot ocean_printf_slot at 0x408000 (textSize).
userland: build/user/OCEAN.E32
build/user/ocean.o: user/ocean.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/ocean.elf: build/user/start.o build/user/runtime.o build/user/ocean.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=ocean_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/OCEAN.E32: build/user/ocean.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# FLIGHT SIMULATOR / altitude tracker E32 -- interactive aircraft state
# (climb / descend / throttle / turn / status / autopilot / selftest / help /
# quit) with a deterministic 5-tick autopilot and a built-in selftest.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# flight_printf_slot at 0x408000 (textSize).
userland: build/user/FLIGHT.E32
build/user/flight.o: user/flight.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/flight.elf: build/user/start.o build/user/runtime.o build/user/flight.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=flight_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/FLIGHT.E32: build/user/flight.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# BLACKJACK E32 -- genuine interactive card game driven from PortCon
# (new / hit / stand / board / selftest / quit).  A fixed-seed LCG shuffles a
# single 52-card deck; Aces count as 1/11 and faces as 10.  The built-in
# selftest plays one full scripted hand (new/hit/stand) ending in a WIN.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# blackjack_printf_slot at 0x408000 (textSize).
userland: build/user/BLACKJACK.E32
build/user/blackjack.o: user/blackjack.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/blackjack.elf: build/user/start.o build/user/runtime.o build/user/blackjack.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=blackjack_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/BLACKJACK.E32: build/user/blackjack.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# MUSIC SCALES E32 -- end-user educational app with a built-in equal-temperament
# note table (C4..B4, A4=440Hz) driven from PortCon: scale <root> prints the
# major-scale note names + frequencies, play <root> sounds the seven notes for
# 150 ms each via the AC97 tone exec 240, plus a built-in C-major selftest.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# music_printf_slot at 0x408000 (textSize).
userland: build/user/MUSIC.E32
build/user/music.o: user/music.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/music.elf: build/user/start.o build/user/runtime.o build/user/music.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=music_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/MUSIC.E32: build/user/music.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# CHESS E32 -- end-user chess opening moves viewer with the built-in 8x8
# starting position, driven from PortCon (board / moves <square> / opening
# <name> / help / selftest / quit).  `moves` lists pseudo-legal destinations
# and `opening` prints the Italian Game / Sicilian Defense / French Defense
# lines.  Built-in selftest renders the board, e2 pawn moves and Italian Game.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) for serial
# markers; IAT slot chess_printf_slot at 0x408000 (textSize).
userland: build/user/CHESS.E32
build/user/chess.o: user/chess.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/chess.elf: build/user/start.o build/user/runtime.o build/user/chess.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=chess_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/CHESS.E32: build/user/chess.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# ASTRONOMY / deep-sky fact viewer E32 -- interactive end-user educational app
# with a built-in table of 6 deep-sky objects (name / type / distance in light
# years / constellation), driven from PortCon (list / info <name> / selftest /
# help / quit).  The built-in selftest prints the Andromeda Galaxy and Polaris
# fact rows.  Markers use ord 915 (RDebug::Printf); IAT slot astro_printf_slot
# at 0x408000 (textSize).
userland: build/user/ASTRO.E32
build/user/astro.o: user/astro.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/astro.elf: build/user/start.o build/user/runtime.o build/user/astro.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=astro_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/ASTRO.E32: build/user/astro.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# TRIVIA QUIZ E32 -- genuine interactive multiple-choice trivia game driven
# from PortCon (start / answer <n> / score / selftest / quit) over six built-in
# questions.  Built-in selftest answers all six correctly for a 6/6 score.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) for the serial
# markers; IAT slot quiz_printf_slot at 0x408000 (textSize).
userland: build/user/QUIZ.E32
build/user/quiz.o: user/quiz.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/quiz.elf: build/user/start.o build/user/runtime.o build/user/quiz.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=quiz_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/QUIZ.E32: build/user/quiz.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# SYSTEM MONITOR E32 -- end-user system monitor: live process census via
# genuine kernel exec 241 (user/pshost.h, op0 base / op1 refresh), ticks via
# exec 5, simulated CPU load from tick deltas over a busy-wait, and simulated
# memory usage from the census. PortCon commands: sample / selftest / help /
# quit; the built-in selftest samples twice. Genuine PE2 import of EUSER.DLL
# ordinal 915 (RDebug::Printf) for the serial markers; IAT slot
# monitor_printf_slot at 0x408000 (textSize).
userland: build/user/MONITOR.E32
build/user/monitor.o: user/monitor.c user/runtime.h user/pshost.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/monitor.elf: build/user/start.o build/user/runtime.o build/user/monitor.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=monitor_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/MONITOR.E32: build/user/monitor.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# HANOI E32 -- end-user Tower of Hanoi puzzle: 4 disks / 3 pegs (A/B/C) solved
# by genuine recursion (solve [1-4] / board / help / selftest / quit) with each
# move applied to an in-memory peg state.  Built-in selftest solves 3 disks in
# 7 moves and renders the final peg state (all disks on C).  Genuine PE2 import
# of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot hanoi_printf_slot at
# 0x408000 (textSize).
userland: build/user/HANOI.E32
build/user/hanoi.o: user/hanoi.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/hanoi.elf: build/user/start.o build/user/runtime.o build/user/hanoi.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=hanoi_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/HANOI.E32: build/user/hanoi.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# END-USER WORD COUNT (WC) E32 -- counts lines, whitespace-delimited words and
# characters of a FAT text file via PortFile IPC (portfilesrv), driven from
# PortCon (wc <file> / selftest / help / quit), with a built-in count of the
# embedded string "Hello world\nSecond line" (2 lines, 4 words, 23 chars).
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) for the serial
# markers; IAT slot wc_printf_slot at 0x408000 (textSize).
userland: build/user/WC.E32
build/user/wc.o: user/wc.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/wc.elf: build/user/start.o build/user/runtime.o build/user/wc.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=wc_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/WC.E32: build/user/wc.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# TETRIS E32 -- genuine interactive 10x12 falling-block game driven from
# PortCon (new / left / right / drop / board / selftest / quit) with one
# deterministic 1x1 square and a built-in three-piece selftest. Genuine PE2
# import of EUSER.DLL ordinal 915 (RDebug::Printf) for the serial markers;
# IAT slot tetris_printf_slot at 0x408000 (textSize).
userland: build/user/TETRIS.E32
build/user/tetris.o: user/tetris.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/tetris.elf: build/user/start.o build/user/runtime.o build/user/tetris.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=tetris_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/TETRIS.E32: build/user/tetris.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# MAZE E32 -- end-user deterministic 9x9 maze generator/solver driven from
# PortCon (gen / show / solve / help / selftest / quit). A fixed-seed DFS carves
# a perfect maze; BFS marks the route from (1,1) to (7,7) with '*' and reports
# its length. Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf)
# for the serial markers; IAT slot maze_printf_slot at 0x408000 (textSize).
userland: build/user/MAZE.E32
build/user/maze.o: user/maze.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/maze.elf: build/user/start.o build/user/runtime.o build/user/maze.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=maze_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/MAZE.E32: build/user/maze.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# PACMAN / maze muncher E32 -- genuine interactive end-user game driven from
# PortCon (new / w a s d / board / selftest / quit).  An 11x9 maze of dots ('.')
# and walls ('#') with a pac character ('@') that eats a dot for 10 points on
# every step.  The built-in selftest performs three scripted east moves that
# eat three dots (score=30, dots left=38).  Genuine PE2 import of EUSER.DLL
# ordinal 915 (RDebug::Printf); IAT slot pacman_printf_slot at 0x408000.
userland: build/user/PACMAN.E32
build/user/pacman.o: user/pacman.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/pacman.elf: build/user/start.o build/user/runtime.o build/user/pacman.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=pacman_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/PACMAN.E32: build/user/pacman.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# CODEBREAKER / MASTERMIND E32 -- end-user game driven from PortCon
# (new / guess <4-letter-code> / show / selftest / quit).  The computer picks a
# 4-colour secret from the RGBYGP palette (repeats allowed); eight guesses with
# black (right place+colour) / white (right colour, wrong place) feedback.  The
# secret stays hidden in normal play; the built-in selftest reveals a known
# code and solves it in three guesses through WIN.  Genuine PE2 import of
# EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot codebreak_printf_slot at
# 0x408000 (textSize).
userland: build/user/CODEBREAK.E32
build/user/codebreak.o: user/codebreak.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/codebreak.elf: build/user/start.o build/user/runtime.o build/user/codebreak.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=codebreak_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/CODEBREAK.E32: build/user/codebreak.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# WORLD TIMEZONE clock E32 -- interactive end-user world clock driven from
# PortCon (list / now <city> / selftest / help / quit) over a built-in table of
# 8 cities with fixed UTC offsets in minutes (Mumbai's half-hour +5:30 is
# exact).  Local HH:MM is derived from the genuine EExecTimeNow executive
# (exec 68) UTC time-of-day plus the city offset.  The built-in selftest prints
# the London, Tokyo and Mumbai times.  Genuine PE2 import of EUSER.DLL ordinal
# 915 (RDebug::Printf); IAT slot worldtime_printf_slot at 0x408000 (textSize).
userland: build/user/WORLDTIME.E32
build/user/worldtime.o: user/worldtime.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/worldtime.elf: build/user/start.o build/user/runtime.o build/user/worldtime.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=worldtime_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/WORLDTIME.E32: build/user/worldtime.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# CONNECT-4 (E32) -- genuine interactive 6x7 game driven from PortCon
# (new / drop <col> / board / selftest / quit). Human X drops pieces into a
# column; a simple CPU O always plays the first column with a free cell.
# The built-in selftest suspends the CPU and stacks four X's in column 0 for
# a deterministic WIN. Genuine PE2 import of EUSER.DLL ordinal 915
# (RDebug::Printf); IAT slot connect4_printf_slot at 0x408000 (textSize).
userland: build/user/CONNECT4.E32
build/user/connect4.o: user/connect4.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/connect4.elf: build/user/start.o build/user/runtime.o build/user/connect4.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=connect4_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/CONNECT4.E32: build/user/connect4.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# SNAKE 2.0 E32 -- genuine interactive 12x8 snake with increasing difficulty
# (one level per 5 foods, faster auto-demo) and scoring (10 points per food),
# driven from PortCon (new / w a s d / board / selftest / quit).  The built-in
# selftest auto-steers through three foods and into the wall for a scripted
# GAMEOVER.  Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT
# slot snake2_printf_slot at 0x408000 (textSize).
userland: build/user/SNAKE2.E32
build/user/snake2.o: user/snake2.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/snake2.elf: build/user/start.o build/user/runtime.o build/user/snake2.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=snake2_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/SNAKE2.E32: build/user/snake2.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# REUSABLE GUI TOOLKIT DEMO (GH) -- guilib.c is a self-contained PE2 GUI
# toolkit (display-list double buffer, primitives, windows, buttons, keyboard
# and mouse input) built on GUI slow execs 249/250/251; guihello.c is its
# interactive demonstration (arrow-key focus, Enter press, Esc quit).  Genuine
# PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# guihello_printf_slot at 0x408000 (textSize).
userland: build/user/GUIHELLO.E32
build/user/guilib.o: user/guilib.c user/guilib.h user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/guihello.o: user/guihello.c user/guilib.h user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/guihello.elf: build/user/start.o build/user/runtime.o build/user/guilib.o build/user/guihello.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=guihello_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/GUIHELLO.E32: build/user/guihello.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# SMALL BASIC INTERPRETER E32 -- a genuine line-oriented interpreter with a
# recursive-descent expression evaluator (+ - * / MOD, parentheses, all six
# comparisons, variables A-Z), a 100-line program store, GOTO/IF/FOR/NEXT, and
# a built-in number-guessing game written in BASIC source (DEMO).  Driven from
# PortCon (LOAD DEMO / RUN / LIST / NEW / QUIT / numbered lines).  Genuine PE2
# import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot basic_printf_slot
# at 0x408000 (textSize).
userland: build/user/BASIC.E32
build/user/basic.o: user/basic.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/basic.elf: build/user/start.o build/user/runtime.o build/user/basic.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=basic_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/BASIC.E32: build/user/basic.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# BREAKOUT E32 -- genuine real-time graphical arcade game on the VBE LFB.
# Builds full animated frames (bricks/paddle/ball/HUD) in the low-RAM gui_shared
# display list and submits them through GUI slow exec 251; reads real keyboard
# input via the PortCon lease (A/D move, ENTER restart, ESC quit). PE2 import of
# EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot breakout_printf_slot at
# 0x408000 (textSize).
userland: build/user/BREAKOUT.E32
build/user/breakout.o: user/breakout.c user/runtime.h user/guishost.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/breakout.elf: build/user/start.o build/user/runtime.o build/user/breakout.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=breakout_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/BREAKOUT.E32: build/user/breakout.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# ---------------------------------------------------------------------------
# Genuine Symbian .EXE naming (append-only addition, 2026-11-21).
# Every user E32 gets a byte-identical <NAME>.EXE alias in build/user. The
# bytes are still real E32/PE2 images — only the directory entry name changes,
# so a FAT disk can present CALC.EXE / SNAKE.EXE instead of CALC.E32.
# The shell's RunImage opens an explicit .EXE name exactly, and for a bare
# name tries .EXE first, then .E32. `make userland-exe` runs after `userland`
# and copies every E32 (including legacy lowercase *.e32) to a .EXE alias.
.PHONY: userland-exe
userland-exe: userland
	@set -e; \
	n=0; \
	for f in build/user/*.E32 build/user/*.e32; do \
	    [ -f "$$f" ] || continue; \
	    base=$${f%.*}; \
	    if cmp -s "$$f" "$$base.EXE"; then continue; fi; \
	    cp -f "$$f" "$$base.EXE"; \
	    n=$$((n+1)); \
	done; \
	total=$$(find build/user -maxdepth 1 -type f -iname '*.EXE' | wc -l); \
	echo "USERLAND-EXE-OK: $$n new .EXE aliases ($$total total) in build/user"

# Per-file rule: `make build/user/CALC.EXE` works too and stays byte-identical.
build/user/%.EXE: build/user/%.E32
	cp -f $< $@

# GRAPHICAL APPLICATION LAUNCHER E32 -- full-screen desktop menu rendered to
# the VBE LFB through GUI slow exec 251.  Up/Down (or W/S) select, A/D switch
# columns, ENTER launches via trusted exec 268 (SvcMgrLaunch), ESC exits.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) for markers;
# IAT slot guimenu_printf_slot at 0x408000 (textSize).
userland: build/user/GUIMENU.E32
build/user/guimenu.o: user/guimenu.c user/runtime.h user/guishost.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/guimenu.elf: build/user/start.o build/user/runtime.o build/user/guimenu.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=guimenu_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/GUIMENU.E32: build/user/guimenu.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# GRAPHICAL DESKTOP / WINDOW MANAGER E32 -- a genuine PE2 E32 program that
# paints the VBE LFB through GUI slow execs 249/250/251: wallpaper, 8 clickable
# app icons, taskbar with a live clock, Start menu and draggable/focusable
# windows.  Click (gui_shared button word) or W/S/A/D + ENTER selects/launches
# via trusted exec 268; ESC returns to the shell.  Genuine PE2 import of
# EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot desktop_printf_slot at
# 0x408000 (textSize).
userland: build/user/DESKTOP.E32
build/user/desktop.o: user/desktop.c user/runtime.h user/guishost.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/desktop.elf: build/user/start.o build/user/runtime.o build/user/desktop.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=desktop_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/DESKTOP.E32: build/user/desktop.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# WINDOWED TEXT EDITOR E32 (DTEDIT) -- a second REAL GUI desktop application:
# a decorated "EKA2 Notepad" window on the VBE LFB with a clickable toolbar
# (SAVE/CLEAR/QUIT), a title-bar close box and a live text area with a blinking
# cursor.  Keyboard typing, Backspace, Enter and arrow keys edit the buffer;
# mouse presses are read from gui_shared (button word + button_seq) and
# hit-tested against the toolbar/close box/text cells.  Saving uses PortFile IPC
# (file_save -> NOTE.DAT).  Built on the reusable guilib.c toolkit over GUI slow
# execs 249/250/251.  Genuine PE2 import of EUSER.DLL ordinal 915
# (RDebug::Printf); IAT slot dtedit_printf_slot at 0x408000 (textSize).
userland: build/user/DTEDIT.E32
build/user/dtedit.o: user/dtedit.c user/guilib.h user/guishost.h user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/dtedit.elf: build/user/start.o build/user/runtime.o build/user/guilib.o build/user/dtedit.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=dtedit_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/DTEDIT.E32: build/user/dtedit.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# WINDOWED DESKTOP CALCULATOR E32 -- a real application window (title bar,
# close button, border, drop shadow) with a live display and a 20-key keypad,
# painted on the VBE LFB through the reusable guilib.c toolkit (GUI execs
# 249/250/251).  Clickable with the PS/2 mouse (gui_shared.buttons/button_seq)
# and typeable on the serial keyboard; expressions use a recursive-descent
# parser adapted from calc.c.  Genuine PE2 import of EUSER.DLL ord 915;
# IAT slot dtcalc_printf_slot at 0x408000 (textSize).
userland: build/user/DTCALC.E32
build/user/dtcalc.o: user/dtcalc.c user/runtime.h user/guilib.h user/guishost.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/dtcalc.elf: build/user/start.o build/user/runtime.o build/user/guilib.o build/user/dtcalc.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=dtcalc_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/DTCALC.E32: build/user/dtcalc.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# WINDOW SERVER E32 (WSV) -- a genuine PE2 E32 process that owns the VBE LFB,
# composites client windows from the kernel's struct ws_shared (slow execs
# WS_BASE / WS_SIZE / WS_OP), draws title bars / borders / focus highlight plus
# a desktop and taskbar, and dispatches MOVE / DOWN / KEY events from the
# shared input ring.  Falls back to a local shared instance when the kernel
# window-server executives are unavailable.  Genuine PE2 import of EUSER.DLL
# ordinal 915 (RDebug::Printf); IAT slot winsrv_printf_slot at 0x408000.
userland: build/user/WINSRV.E32
build/user/winsrv.o: user/winsrv.c user/runtime.h user/guishost.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/winsrv.elf: build/user/start.o build/user/runtime.o build/user/winsrv.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=winsrv_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/WINSRV.E32: build/user/winsrv.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# WINDOW-SERVER CLIENT LIBRARY + WINDOWED CALCULATOR E32 (WSDL) -- a real
# client of the window server: wsclient.c draws into private per-window content
# buffers from variant/qemu_windowsrv.h's struct ws_shared, while wsdl.c opens
# ONE calculator window and services ws_get_event() mouse/key events.  Genuine
# PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# wsdl_printf_slot at 0x408000 (textSize).
userland: build/user/WSDL.E32
build/user/wsclient.o: user/wsclient.c user/wsclient.h variant/qemu_windowsrv.h user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/wsdl.o: user/wsdl.c user/wsclient.h user/runtime.h variant/qemu_windowsrv.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/wsdl.elf: build/user/start.o build/user/runtime.o build/user/wsclient.o build/user/wsdl.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=wsdl_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/WSDL.E32: build/user/wsdl.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# ---------------------------------------------------------------------------
# LARGE USER HEAP PROOF (BIGHEAP) -- append-only addition, 2026-11-21.
# A genuine PE2 E32 that allocates 1/2/3 MiB from the 4 MiB chunk-backed heap,
# stamps and verifies every byte, frees/repeats, and checks calloc + realloc.
# Uses the same EUSER ord 915 marker pattern as MEMTEST.E32 / SELFTEST.E32.
# Named BIGHEAP (not HEAPTEST) because `heaptest` is a shell built-in command
# that would shadow the program image at launch.
userland: build/user/BIGHEAP.E32
build/user/heaptest.o: user/heaptest.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/heaptest.elf: build/user/start.o build/user/runtime.o build/user/malloc.o build/user/heaptest.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=heaptest_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/BIGHEAP.E32: build/user/heaptest.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet
# Lowercase `make build/user/heaptest.E32` alias still works.
build/user/heaptest.E32: build/user/BIGHEAP.E32
	cp -f $< $@

# ---------------------------------------------------------------------------
# ZLIB-COMPATIBLE GZIP / DEFLATE UTILITY (ZIP32) -- append-only addition.
# A genuine PE2 E32 (EUSER.DLL ordinal 915) driving a freestanding RFC1951
# codec vendored in ports/miniz/ (LZ77 + fixed-Huffman deflate, full
# stored/fixed/dynamic inflate, CRC-32).  PortFile IPC for file I/O.
userland: build/user/ZIP32.E32
build/user/miniz.o: ports/miniz/miniz.c ports/miniz/miniz.h | build/user
	$(CC) $(USER_FLAGS) -Iports/miniz -c $< -o $@
build/user/zip32.o: user/zip32.c user/runtime.h ports/miniz/miniz.h | build/user
	$(CC) $(USER_FLAGS) -Iports/miniz -c $< -o $@
build/user/zip32.elf: build/user/start.o build/user/runtime.o build/user/miniz.o build/user/zip32.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=zip32_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/ZIP32.E32: build/user/zip32.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# ---------------------------------------------------------------------------
# LARGE-FILE STREAMING TEST (LARGEFILE) -- append-only addition, 2026-11-21.
# Genuine PE2 E32 that opens a >2 MiB FAT file via the PortBigFile streaming
# device and verifies file_size / file_read_at / file_seek at start, middle and
# end (the path DOOM uses to read a multi-MB WAD). EUSER ord 915 marker pattern.
userland: build/user/LARGEFILE.E32
build/user/largefile.o: user/largefile.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/largefile.elf: build/user/start.o build/user/runtime.o build/user/largefile.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=largefile_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/LARGEFILE.E32: build/user/largefile.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet
# Lowercase spelling also works (`make build/user/largefile.E32`).
build/user/largefile.E32: build/user/LARGEFILE.E32
	cp -f $< $@

# ---------------------------------------------------------------------------
# WADPATH PROBE (WADPROBE) -- append-only diagnostic, 2026-11-21.
# Tiny E32 that prints file_open()/file_size()/file_read_at() results for every
# candidate spelling of FDOOM1.WAD, so the DOOM WAD-open failure can be pinned
# down with in-guest ground truth. Uses the same PE2 ord-915 marker path.
userland: build/user/WADPROBE.E32
build/user/wadprobe.o: user/wadprobe.c user/runtime.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/wadprobe.elf: build/user/start.o build/user/runtime.o build/user/wadprobe.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=wadprobe_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/WADPROBE.E32: build/user/wadprobe.elf tools/elf2e32.py tools/e32read.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet
build/user/wadprobe.E32: build/user/WADPROBE.E32
	cp -f $< $@

# ---------------------------------------------------------------------------
# BULK FRAMEBUFFER BLIT PROOF (FBBLITTEST) -- append-only addition, 2026-11-21.
# Ring-3 E32 that allocates a 320x200 8-bit indexed frame, paints a 16x16
# checkerboard and submits it in ONE kernel exec (slot 270 op 3) to the VBE LFB.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# fbblittest_printf_slot at 0x408000 (textSize).
userland: build/user/FBBLITTEST.E32
build/user/fbblittest.o: user/fbblittest.c user/runtime.h variant/qemu_fbblit.h | build/user
	$(CC) $(USER_FLAGS) -Ivariant -c $< -o $@
build/user/fbblittest.elf: build/user/start.o build/user/runtime.o build/user/malloc.o build/user/fbblittest.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=fbblittest_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/FBBLITTEST.E32: build/user/fbblittest.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# ---------------------------------------------------------------------------
# GC_BLIT proof E32 (append-only addition, 2026-11-21).  Allocates a 640x400
# RGBA frame, publishes its physical base/size via guishost's tail fields and
# submits ONE exec-251 GC_BLIT command (real bulk LFB blit).  Genuine PE2
# import of EUSER.DLL ordinal 915; IAT slot gcblittest_printf_slot at 0x408000.
userland: build/user/GCBLITTEST.E32
build/user/gcblittest.o: user/gcblittest.c user/runtime.h user/guishost.h | build/user
	$(CC) $(USER_FLAGS) -c $< -o $@
build/user/gcblittest.elf: build/user/start.o build/user/runtime.o build/user/malloc.o build/user/gcblittest.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=gcblittest_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/GCBLITTEST.E32: build/user/gcblittest.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# ---------------------------------------------------------------------------
# LUA 5.4.6 interpreter (append-only addition, Lua E32 port).
# Full Lua 5.4.6 core/VM plus base, table, string and reduced math libraries.
# Freestanding shims live in ports/lua/; stdout goes through EUSER ord 915.
LUA_GCC_INC := $(shell $(CC) -m32 -print-file-name=include)
LUA_FLAGS := $(USER_FLAGS) -ffunction-sections -fdata-sections -flto -nostdinc -isystem $(LUA_GCC_INC) -Iports/lua \
             -DLUA_USE_C89 -Dl_signalT=int
LUA_SRCS := lapi.c lauxlib.c lbaselib.c lcode.c lctype.c ldebug.c ldo.c \
            ldump.c lfunc.c lgc.c linit.c llex.c lmathlib.c lmem.c lobject.c \
            lopcodes.c lparser.c lstate.c lstring.c lstrlib.c ltable.c \
            ltablib.c ltm.c lundump.c lvm.c lzio.c e32_shim.c
LUA_OBJS := $(patsubst %.c,build/lua/%.o,$(LUA_SRCS))

build/lua:
	mkdir -p $@

build/lua/%.o: ports/lua/%.c ports/lua/lua.h ports/lua/luaconf.h ports/lua/e32_shim.h | build/lua
	$(CC) $(LUA_FLAGS) -c $< -o $@

build/user/luae32.o: user/luae32.c user/runtime.h ports/lua/lua.h ports/lua/lauxlib.h ports/lua/e32_shim.h | build/user
	$(CC) $(LUA_FLAGS) -c $< -o $@

build/user/luae32.elf: build/user/start.o build/user/runtime.o build/user/malloc.o $(LUA_OBJS) build/user/luae32.o user/link.ld
	$(CC) -m32 -nostdlib -flto -Wl,--emit-relocs,--gc-sections -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/LUA.E32: build/user/luae32.elf tools/elf2e32.py tools/mke32.py ports/lua/pack_lua_e32.py
	python3 ports/lua/pack_lua_e32.py $< $@ --euser-import-915
	python3 tools/e32read.py $@ --quiet

userland: build/user/LUA.E32

# ---------------------------------------------------------------------------
# BC32 -- the bc arbitrary-precision calculator language as a genuine E32.
# The interpreter is a faithful, from-scratch bc-compatible subset vendored in
# ports/bc/ (arbitrary-precision base-10 engine + recursive-descent parser).
# It is freestanding, needs no libc, and stays under the 64 KiB heap budget.
# Genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf); IAT slot
# bc32_printf_slot at 0x408000 (textSize).
BC_FLAGS := $(USER_FLAGS) -Iports/bc
BC_SRCS  := bignum.c bcinterp.c
BC_OBJS  := $(patsubst %.c,build/bc/%.o,$(BC_SRCS))

build/bc:
	mkdir -p $@

build/bc/%.o: ports/bc/%.c ports/bc/bignum.h ports/bc/bcinterp.h | build/bc
	$(CC) $(BC_FLAGS) -c $< -o $@

build/user/bc32.o: user/bc32.c user/runtime.h ports/bc/bcinterp.h | build/user
	$(CC) $(BC_FLAGS) -c $< -o $@

build/user/bc32.elf: build/user/start.o build/user/runtime.o $(BC_OBJS) build/user/bc32.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=bc32_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/BC32.E32: build/user/bc32.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

userland: build/user/BC32.E32
# 8.3 short-name alias: the guest FAT driver (variant/fat32.cpp) only resolves
# single-entry LFNs, so the 11-char GCBLITTEST stem cannot be opened by name.
# This byte-identical alias lets the shell launch it as `gcblit`.
build/user/GCBLIT.E32: build/user/GCBLITTEST.E32
	cp -f $< $@

# Tiny launcher for the full Lua image (shell load buffer is only 64 KiB).
build/lua/luaboot.o: ports/lua/luaboot.c user/runtime.h | build/lua
	$(CC) $(USER_FLAGS) -c $< -o $@

build/user/luaboot.elf: build/user/start.o build/user/runtime.o build/lua/luaboot.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/LUABOOT.E32: build/user/luaboot.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

userland: build/user/LUABOOT.E32

# ---------------------------------------------------------------------------
# REAL DOOM via doomgeneric (append-only, 2026-11-21).  The engine core is
# copied verbatim into ports/doom/ from the codesuki doomgeneric-kernel mirror
# (GPL-2.0-or-later).  Only the platform layer (e32_platform.c), the freestanding
# libc shim (e32_shim.c) and the PE2 launcher (user/doome32.c) are E32-specific.
#  --gc-sections + -ffunction-sections keep the image smaller.  The PE2 IAT
#  slot for EUSER.DLL ordinal 915 sits at codeBase+IAT_OFF; elf2e32.py's
#  --iat-off option (backward compatible) widens it for this large image.
userland: build/user/DOOM.E32

DOOM_CORE := dummy am_map doomdef doomstat dstrings d_event d_items d_iwad d_loop d_main d_mode d_net f_finale f_wipe g_game hu_lib hu_stuff info i_cdmus i_endoom i_joystick i_scale i_sound i_system i_timer memio m_argv m_bbox m_cheat m_config m_controls m_fixed m_menu m_misc m_random p_ceilng p_doors p_enemy p_floor p_inter p_lights p_map p_maputl p_mobj p_plats p_pspr p_saveg p_setup p_sight p_spec p_switch p_telept p_tick p_user r_bsp r_data r_draw r_main r_plane r_segs r_sky r_things sha1 sounds statdump st_lib st_stuff s_sound tables v_video wi_stuff w_checksum w_file w_main w_wad z_zone w_file_stdc i_input i_video doomgeneric mus2mid
DOOM_ENGINE_OBJS := $(addprefix build/user/doom_,$(addsuffix .o,$(DOOM_CORE)))
DOOM_ALL_OBJS := $(DOOM_ENGINE_OBJS) build/user/doom_e32_shim.o build/user/doom_e32_platform.o build/user/doome32.o
DOOM_FLAGS := $(USER_FLAGS) -std=gnu99 -Iports/doom -Iports/doom/sysinc -Ivariant -ffunction-sections -fdata-sections
DOOM_IAT_OFF := 0x40000

build/user/doom_%.o: ports/doom/%.c user/runtime.h | build/user
	$(CC) $(DOOM_FLAGS) -c $< -o $@

build/user/doome32.o: user/doome32.c ports/doom/doomgeneric.h ports/doom/e32_shim.h user/runtime.h | build/user
	$(CC) $(DOOM_FLAGS) -c $< -o $@

build/user/doome32.elf: build/user/start.o build/user/runtime.o $(DOOM_ALL_OBJS) user/link.ld
	ld -m elf_i386 --emit-relocs --gc-sections --defsym=doom_printf_slot=0x440000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/DOOM.E32: build/user/doome32.elf tools/elf2e32.py tools/e32read.py
	python3 tools/elf2e32.py $< $@ --euser-import 915 --iat-off $(DOOM_IAT_OFF)
	python3 tools/e32read.py $@

# Z-MACHINE INTERPRETER E32 (ZM) -- a genuine freestanding Z-machine v3
# interpreter (ports/zmachine/zmachine.c, ~930 LOC) running real Infocom
# Z-code.  It parses v3 headers, ZSCII text with abbreviations, the object
# and property tables, the dictionary with lexical analysis and the full
# v1-3 opcode set (calls/locals/stack, branches, print, sread, status line).
# Story bytes are staged as ZORK.Z3 or MINIZORK.Z3 on FAT and read over
# PortFile IPC straight into the interpreter's static 128 KiB image; no
# malloc() is used.  Serial markers cross a genuine PE2 import of EUSER.DLL
# ordinal 915 (RDebug::Printf); IAT slot zm_printf_slot at 0x408000.
userland: build/user/ZMACHINE.E32
build/zmachine:
	mkdir -p $@
build/zmachine/zmachine.o: ports/zmachine/zmachine.c ports/zmachine/zmachine.h | build/zmachine
	$(CC) $(USER_FLAGS) -Iports/zmachine -c $< -o $@
build/user/zmachine32.o: user/zmachine32.c user/runtime.h ports/zmachine/zmachine.h | build/user
	$(CC) $(USER_FLAGS) -Iports/zmachine -c $< -o $@
build/user/zmachine32.elf: build/user/start.o build/user/runtime.o build/zmachine/zmachine.o build/user/zmachine32.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=zm_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/ZMACHINE.E32: build/user/zmachine32.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet

# ---------------------------------------------------------------------------
# REAL DOOM SOUND (append-only).  i_e32sound.c supplies DG_sound_module and
# DG_music_module for FEATURE_SOUND (enabled in ports/doom/doomfeatures.h):
# DMX sound lumps -> 16-bit stereo PCM -> AC97 exec 240, plus a small MUS synth.
# The new object is added as an extra prerequisite of doome32.elf; the original
# rules above stay untouched.  Mix_Flush() in e32_platform.c (owned by another
# agent) may call E32Snd_Pump()/E32Snd_Flush() from this module.
build/user/doom_i_e32sound.o: ports/doom/i_e32sound.c ports/doom/e32_audio.h \
		ports/doom/i_sound.h ports/doom/e32_shim.h ports/doom/w_wad.h \
		ports/doom/sounds.h user/runtime.h | build/user
	$(CC) $(DOOM_FLAGS) -c $< -o $@

build/user/doome32.elf: build/user/doom_i_e32sound.o
