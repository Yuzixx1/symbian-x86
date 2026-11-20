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
userland: build/user/DNSRES.E32
userland: build/user/TEXTEDIT.E32
userland: build/user/ca_test.e32

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
