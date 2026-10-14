#!/bin/sh
# check_env.sh — Phase 2 environment verification (Linux, no ActivePerl/bldmake).
# Fails fast with a clear message if anything is missing.
set -e
fail() { echo "ENV-FAIL: $1" >&2; exit 1; }
ok()   { echo "ENV-OK: $1"; }

command -v gcc >/dev/null 2>&1 || fail "gcc not found"
command -v as >/dev/null 2>&1 || fail "GNU as not found"
command -v ld >/dev/null 2>&1 || fail "GNU ld not found"
command -v qemu-system-i386 >/dev/null 2>&1 || fail "qemu-system-i386 not found"
command -v make >/dev/null 2>&1 || fail "make not found"
ok "gcc=$(gcc -dumpversion) as=$(as --version | head -n1) qemu=$(qemu-system-i386 --version | head -n1)"

# Freestanding -m32 must work WITHOUT 32-bit libc (bare-metal uses -nostdlib).
TMPD="${TMPDIR:-/tmp/opencode}"
mkdir -p "$TMPD"
cat > "$TMPD/env_test.c" <<'EOF'
int k_env_probe(void){return 42;}
EOF
gcc -m32 -march=i486 -ffreestanding -nostdlib -c "$TMPD/env_test.c" -o "$TMPD/env_test.o" \
  || fail "gcc -m32 -ffreestanding -nostdlib failed"
ok "freestanding -m32 compile works"

ld -m elf_i386 --help >/dev/null 2>&1 || fail "ld lacks elf_i386 emulation"
ok "ld supports -m elf_i386"

as --32 --version >/dev/null 2>&1 || fail "as lacks --32"
ok "as supports --32"
echo "ENV: all Phase 2 prerequisites satisfied."
