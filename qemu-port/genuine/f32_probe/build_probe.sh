#!/bin/bash
# Link a PE2 E32 that runs ACTUAL genuine F32 code from kernelhwsrv.
# Output stays inside qemu-port/genuine/f32_probe/.
set -e
unset LD_LIBRARY_PATH
cd "$(dirname "$0")/../.."   # qemu-port/
D=genuine/f32_probe
mkdir -p "$D"
UF="-m32 -march=i486 -Os -ffreestanding -fno-pic -fno-pie -fno-builtin -fno-stack-protector -fno-exceptions -fno-rtti -mpreferred-stack-boundary=2 -Iuser"
gcc $UF -c "$D/probe.cpp" -o "$D/probe.o"
gcc $UF -c "$D/euser_local.cpp" -o "$D/euser_local.o"
LIBGCC=$(gcc -m32 -print-libgcc-file-name)
ld -m elf_i386 --emit-relocs --defsym=f32probe_printf_slot=0x408000 -T user/link.ld \
   -o "$D/F32PROBE.elf" build/user/start.o build/user/runtime.o \
   "$D/probe.o" "$D/euser_local.o" \
   genuine/obj/F32/filesystem_utils.o genuine/obj/F32/sl_bpb32.o "$LIBGCC"
python3 tools/elf2e32.py "$D/F32PROBE.elf" "$D/F32PROBE.E32" --euser-import 915
python3 tools/e32read.py "$D/F32PROBE.E32" --quiet
echo "F32PROBE OK: $D/F32PROBE.E32"

# Genuine-opcode IPC client against !FileServer.
gcc $UF -fshort-wchar -c "$D/fsclient.cpp" -o "$D/fsclient.o"
ld -m elf_i386 --emit-relocs --defsym=f32client_printf_slot=0x408000 -T user/link.ld \
   -o "$D/F32CLIENT.elf" build/user/start.o build/user/runtime.o build/user/ipc.o \
   "$D/fsclient.o" "$LIBGCC"
python3 tools/elf2e32.py "$D/F32CLIENT.elf" "$D/F32CLIENT.E32" --euser-import 915
python3 tools/e32read.py "$D/F32CLIENT.E32" --quiet
echo "F32CLIENT OK: $D/F32CLIENT.E32"
