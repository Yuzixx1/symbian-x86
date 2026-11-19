#!/usr/bin/env python3
"""R1 E32Validate test - builds FAT disk, boots QEMU, runs portfilesrv."""
import os, struct, subprocess, sys

KERN = "/home/yuzi/Documents/Sybian REAL/qemu-port/build/kernfull.elf"
DISK = "/tmp/opencode/test_fat.img"

def make_fat_disk(path):
    """Create FAT32 disk image with E32 images."""
    fat_img = disk + ".fat"
    # Create FAT32 image
    with open(fat_img, "wb") as f:
        f.write(b"\x00" * (32 * 1024 * 1024))
    subprocess.run(["mkfs.fat", "-F", "32", fat_img],
                   capture_output=True, check=True)
    # Create main disk with MBR
    with open(disk, "wb") as f:
        f.write(b"\x00" * (32 * 1024 * 1024))
    mbr = bytearray(512)
    mbr[0x1BE] = 0x80       # bootable
    mbr[0x1BE+4] = 0x0C     # FAT32 LBA
    mbr[0x1BE+8:0x1BE+12] = b"\x00\x08\x00\x00"  # LBA 2048
    mbr[0x1BE+12:0x1BE+16] = b"\x00\x40\x00\x00"  # 16384 sectors
    mbr[510] = 0x55; mbr[511] = 0xAA
    with open(disk, "r+b") as f:
        f.seek(0); f.write(bytes(mbr))
        # Copy FAT filesystem to partition at LBA 2048
        with open(fat_img, "rb") as f2:
            f.seek(2048 * 512)
            f.write(f2.read())
    # Copy files
    disk_path = disk + "@@2048"
    subprocess.run(["mcopy", "-i", disk_path, "/home/yuzi/.cache/sybian-r1/r1-portfilesrv-e32-v1/output/portfilesrv", "::portfilesrv"], capture_output=True)
    subprocess.run(["mcopy", "-i", disk_path, "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll", "::EUSER.DLL"], capture_output=True)

def run_qemu(kern_path, disk_path):
    cmd = (f"qemu-system-i386 -M q35 -m 128 -display none -serial stdio "
           f'-kernel "{kern_path}" '
           f"-drive file={disk_path},format=raw,snapshot=on,if=none,id=d0 "
           f"-device ich9-ahci,id=sata -device ide-hd,drive=d0,bus=sata.0 "
        "-monitor none -no-reboot")
    proc = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                           text=True)
    output = []
    try:
        proc.stdin.write("\n")
        proc.stdin.flush()
        time.sleep(2)
        for cmd_line in ["mount\n", "portfilesrv\n"]:
            proc.stdin.write(cmd_line)
            proc.stdin.flush()
            time.sleep(5)
        proc.stdin.close()
    except Exception as e:
        proc.kill()
        return -1, str(e), ""
    import time
    out_lines = []
    start = time.time()
    while time.time() - start < 30:
        line = proc.stdout.readline()
        if not line:
            time.sleep(0.1)
            continue
        out_lines.append(line)
        if "e32run: validate" in line or "portfilesrv (corrupt)" in line or "validate r=-20" in line:
            break
    proc.kill()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
    out = "".join(out_lines)
    err = ""
    return proc.returncode, out, err

def main():
    import sys
    print("=== Sybian REAL E32Validate Test ===")
    print(f"Kernel: {KERN}")
    print(f"Disk: {DISK}")
    print()
    # Build disk
    print("Building FAT disk...")
    make_fat_disk(DISK)
    print(f"Created: {disk_path}")
    # Run QEMU
    print(f"\nBooting QEMU...")
    rc, out, err = run_qemu(KERN, disk_path)
    print("\n=== Serial Output ===")
    print(out)
    print("\n=== Results ===")
    passed = "validate r=0" in out
    print(f"  E32Validate: {'PASS' if passed else 'FAIL'}")
    print(f"  process_created: {'PASS' if 'ProcessCreate r=0' in out else 'FAIL'}")
    print(f"  Loaded r=0: {'PASS' if 'Loaded r=0' in out else 'FAIL'}")
    print(f"  resumed: {'PASS' if 'Resume()...' in out else 'FAIL'}")
    print(f"  started: {'PASS' if 'started (user process)' in out else 'FAIL'}")
    print(f"  corrupt: {'FAIL' if 'corrupt' in out or 'validate r=-20' in out else 'PASS'}")
    all_pass = ("validate r=0" in out and "Loaded r=0" in out and "Resume()..." in out
                and "started (user process)" in out)
    print(f"\n{'ALL TESTS PASSED' if all_pass else 'SOME TESTS FAILED'}")
    return 0 if all_pass else 1

if __name__ == "__main__":
    import time
    sys.exit(main())