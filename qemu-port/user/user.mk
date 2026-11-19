# Included from the port Makefile; no hosted libc or cross SDK needed.
USER_FLAGS := -m32 -march=i486 -Os -ffreestanding -fno-pic -fno-pie -fno-builtin -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables -mpreferred-stack-boundary=2 -Wall -Wextra -Iuser -fno-rtti

.PHONY: userland
userland: build/user/ECHO.E32 build/user/TYPE.E32 build/user/CP.E32 build/user/FILECHK.E32
userland: build/user/IPCSRV.E32 build/user/IPCCLI.E32 build/user/CWD.E32 build/user/CRASH.E32 build/user/MALLOC.E32 build/user/INTERACT.E32 build/user/SLEEP.E32 build/user/CHUNK.E32 build/user/CHKOPEN.E32
userland: build/user/PORTFILESRV.E32
userland: build/user/SVCMGR.E32
userland: build/user/WAITFOR.E32
userland: build/user/ca_test.e32

# P-06: user-mode blocking wait via the genuine ProcessLogon executive (123).
build/user/waitfor.elf: build/user/start.o build/user/runtime.o build/user/waitfor.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/WAITFOR.E32: build/user/waitfor.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

# I-07: compiled userland CActive/CActiveScheduler fixture (genuine-ABI shim).
build/user/ca_test.elf: build/user/start.o build/user/runtime.o build/user/malloc.o build/user/euser_shim.o build/user/ca_test.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/ca_test.e32: build/user/ca_test.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet

build/user/ipc.o: user/ipc.h
build/user/ipcsrv.o build/user/ipccli.o: user/ipc.h

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

# S-02: service manager E32 (ordered startup + supervision + shutdown).
build/user/svcmgr.o: user/svcmgr.cpp user/runtime.h user/ipc.h | build/user
	$(CXX) $(USER_FLAGS) -Iuser -c $< -o $@

build/user/svcmgr.elf: build/user/start.o build/user/runtime.o build/user/ipc.o build/user/svcmgr.o user/link.ld
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
