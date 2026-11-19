#!/usr/bin/env python3
"""Quick E32Validate test - builds FAT disk, runs QEMU, tests portfilesrv."""
import os, struct, subprocess, sys

KERN = "/home/yuzi/Documents/Sybian REAL/qemu-port/build/kernfull.elf"
DISK = "/tmp/opencode/test_fat.img"

def make_disk():
    """Create FAT32 disk with E32 images."""
    fat_img = "/tmp/opencode/fat32.img"
    with open(fat_img, "wb") as f:
        f.write(b"\x00" * (32 * 1024 * 1024))
    subprocess.run(["mkfs.fat", "-F", "32", fat_img],
                   capture_output=True, check=True)
    with open(disk, "wb") as f:
        f.write(b"\x00" * (32 * 1024 * 1024))
    mbr = bytearray(512)
    mbr[0x1BE] = 0x80       # bootable
    mbr[0x1BE+4] = 0x0C     # FAT32 LBA
    mbr[0x1BE+8:0x1BE+12] = b"\x00\x08\x00\x00"  # LBA 2048
    mbr[0x1BE+12:0x1BE+16] = b"\x00\x40\x00\x00"  # 16384 sectors
    mbr[510] = 0x55; mbr[511] = 0xAA
    with open(disk, "r+b") as f:
        f.write(bytes(mbr))
        with open(fat_img, "rb") as f2:
            f.seek(2048 * 512)
            f.write(f2.read())
    dp = disk + "@@2048"
    subprocess.run(["mcopy", "-i", disk_path if False else disk_path_mount, "/tmp/opencode/test_hello.e32", "::hello.e32"], capture_output=True)
    subprocess.run(["mcopy", "-i", disk_path,
                    "/home/yuzi/.cache/sybian-r1/r1-portfilesrv-e32-v1/output/portfilesrv", "::portfilesrv"],
                   capture_output=True)
    subprocess.run(["mcopy", "-i", disk_path,
                    "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll", "::EUSER.DLL"],
                   capture_output=True)
    return disk_path

def run_qemu(kern, disk):
    cmd = (f"qemu-system-i386 -M q35 -m 128 -display none -serial stdio "
           f'-kernel "{kern}" '
           f"-drive file={disk},format=raw,snapshot=on,if=none,id=d0 "
           f"-device ich9-ahci,id=sata -device ide-hd,drive=d0,bus=sata.0 "
           f"-monitor none -no-reboot")
    proc = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True)
    out_lines = []
    try:
        proc.stdin.write("\n"); proc.stdin.flush(); time.sleep(2)
        for cmd in ["mount\n", "portfilesrv\n"]:
            proc.stdin.write(cmd_line); proc.stdin.flush(); time.sleep(5)
        proc.stdin.close()
    except Exception as e:
        proc.kill(); return -1, str(e), ""
    lines = []
    start = time.time()
    while time.time() - start < 30:
        line = proc.stdout.readline()
        if not line:
            time.sleep(0.1); continue
        out_lines.append(line)
        if "validate r=0" in line or "portfilesrv (corrupt)" in line or "validate r=-20" in line:
            break
    proc.kill()
    try: proc.wait(timeout=5)
    except subprocess.TimeoutExpired: proc.kill()
    out = "".join(out_lines)
    return proc.returncode, out, ""

def main():
    import time
    print("=== Card A-5: QEMU E32Validate Test ===\n")
    print(f"Kernel: {KERN}")
    print(f"Disk: {DISK}")
    print("\nBuilding FAT disk...")
    make_fat_disk(DISK)
    print(f"Built: {disk_path}")
    print(f"\nBooting QEMU...")
    rc, out, err = run_qemu(KERN, disk_path)
    print("\n=== Serial Output ===")
    print(out)
    print("\n=== Results ===")
    results = {
        "kernel_booted": "Symbian EKA2 x86 full kernel" in out,
        "mount_success": "mounted C:" in out,
        "validate_r0": "validate r=0" in out,
        "process_created": "ProcessCreate r=0" in out,
        "loaded_r0": "Loaded r=0" in out,
        "resumed": "Resume()..." in out,
        "started": "started (user process)" in out,
        "corrupt": "corrupt" in out or "validate r=-20" in out,
    }
    for name, passed in results.items():
        print(f"  [{'PASS' if passed else 'FAIL'}] {name}")
    all_pass = all(results.values())
    print(f"\n{'ALL PASSED' if all_pass else 'SOME FAILED'}")
    return 0 if all_pass else 1

if __name__ == "__main__":
    import time
    sys.exit(main())