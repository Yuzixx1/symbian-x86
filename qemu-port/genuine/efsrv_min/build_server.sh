#!/bin/bash
# Build the minimal EFSRV host E32 used by tools/r1_genuine_efsrv_test.py.
# The genuine sfile/sfsrv/sfat32 objects are built separately by
# genuine/f32_efsrv_build.py; this image is the integration stand-in that
# publishes !FileServer with the genuine TFsMessage opcode numbers.
set -e
unset LD_LIBRARY_PATH
cd "$(dirname "$0")/../.."   # qemu-port/
D=genuine/efsrv_min
mkdir -p "$D"
CXXFLAGS="-m32 -march=i486 -Os -ffreestanding -fno-pic -fno-pie -fno-builtin -fno-stack-protector -fno-exceptions -fno-rtti -mpreferred-stack-boundary=2 -Iuser -Ifs"
gcc $CXXFLAGS -c "$D/efsrv_min.cpp" -o "$D/efsrv_min.o"
# The genuine user-mode FS engine (R1Fs::Core + CServer2 FileServer) has no
# user/user.mk rule since the svcmgr retirement; build it here, as the sources
# under fs/ cannot match the build/user/%.o pattern (which only reaches user/).
gcc $CXXFLAGS -c fs/r1_fs_core.cpp -o "$D/r1_fs_core.o"
gcc $CXXFLAGS -c fs/r1_fs_server.cpp -o "$D/r1_fs_server.o"
LIBGCC=$(gcc -m32 -print-libgcc-file-name)
ld -m elf_i386 --emit-relocs -T user/link.ld -o "$D/EFSRVMIN.elf" \
   build/user/start.o build/user/runtime.o build/user/ipc.o \
   "$D/efsrv_min.o" build/user/malloc.o \
   "$D/r1_fs_core.o" "$D/r1_fs_server.o" "$LIBGCC"
python3 tools/elf2e32.py "$D/EFSRVMIN.elf" "$D/EFSRVMIN.E32"
python3 "$D/add_capability.py" "$D/EFSRVMIN.E32"
python3 tools/e32read.py "$D/EFSRVMIN.E32" --quiet
echo "EFSRVMIN OK: $D/EFSRVMIN.E32"

# B5 on-disk FAT I/O client: genuine TFsMessage opcodes over !FileServer IPC.
# Uses only the port print()/ipc_* bridge (no EUSER DllImport needed).
gcc $CXXFLAGS -c "$D/fatio_client.cpp" -o "$D/fatio_client.o"
ld -m elf_i386 --emit-relocs -T user/link.ld -o "$D/FATIOCLIENT.elf" \
   build/user/start.o build/user/runtime.o build/user/ipc.o \
   "$D/fatio_client.o" build/user/malloc.o "$LIBGCC"
python3 tools/elf2e32.py "$D/FATIOCLIENT.elf" "$D/FATIOCLIENT.E32"
python3 tools/e32read.py "$D/FATIOCLIENT.E32" --quiet
echo "FATIOCLIENT OK: $D/FATIOCLIENT.E32"
