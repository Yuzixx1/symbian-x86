# Included from the port Makefile; no hosted libc or cross SDK needed.
USER_FLAGS := -m32 -march=i486 -Os -ffreestanding -fno-pic -fno-pie -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -mpreferred-stack-boundary=2 -Wall -Wextra

.PHONY: userland
userland: build/user/ECHO.E32 build/user/TYPE.E32 build/user/CP.E32 build/user/FILECHK.E32
userland: build/user/IPCSRV.E32 build/user/IPCCLI.E32 build/user/CWD.E32 build/user/CRASH.E32 build/user/MALLOC.E32 build/user/INTERACT.E32 build/user/SLEEP.E32

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

build/user/malloc.elf: build/user/start.o build/user/runtime.o build/user/malloc.o build/user/malloctest.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)

build/user/MALLOC.E32: build/user/malloc.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@
	python3 tools/e32read.py $@ --quiet
