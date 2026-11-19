#!/usr/bin/env python3
"""Create FAT32 disk image with E32 images for QEMU testing."""
import struct, subprocess, os

disk = '/tmp/opencode/test_fat.img'
fat_img = '/tmp/opencode/fat32_fs.img'

# Create standalone FAT32 image
with open(fat_img, 'wb') as f:
    f.write(b'\x00' * (32 * 1024 * 1024))

subprocess.run(['mkfs.fat', '-F', '32', fat_img],
               capture_output=True, check=True)

# Create main disk image with MBR + FAT32 partition at LBA 2048
with open(disk, 'wb') as f:
    f.write(b'\x00' * (32 * 1024 * 1024))

mbr = bytearray(512)
mbr[0x1BE] = 0x80       # bootable
mbr[0x1BE+4] = 0x0C     # FAT32 LBA
mbr[0x1BE+8:0x1BE+12] = b'\x00\x08\x00\x00'  # start LBA = 2048
mbr[0x1BE+12:0x1BE+16] = b'\x00\x40\x00\x00'  # 16384 sectors
mbr[510] = 0x55
mbr[511] = 0xAA

with open(disk, 'r+b') as f:
    f.seek(0)
    f.write(bytes(mbr))

with open(fat_img, 'rb') as f:
    fat_data = f.read()

with open(disk, 'r+b') as f:
    f.seek(2048 * 512)
    f.write(fat_data)

print('Disk created with MBR + FAT32 partition at LBA 2048')

# Copy E32 files
disk_path = disk + '@@2048'
subprocess.run(['mcopy', '-i', disk_path, '/tmp/opencode/test_hello.e32', '::hello.e32'],
               capture_output=True)
subprocess.run(['mcopy', '-i', disk_path,
                '/home/yuzi/.cache/sybian-r1/r1-portfilesrv-e32-v1/output/portfilesrv',
                '::portfilesrv'], capture_output=True)
subprocess.run(['mcopy', '-i', disk_path,
                '/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll',
                '::EUSER.DLL'], capture_output=True)
print('Files copied to disk')

result = subprocess.run(['mdir', '-i', disk_path, '::'], capture_output=True, text=True)
print(result.stdout)