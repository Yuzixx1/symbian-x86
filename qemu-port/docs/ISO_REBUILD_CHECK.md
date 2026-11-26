# ISO rebuild check after the EFSRVMIN build fix

Verdict: **no ISO rebuild needed** — `build/symbian-grub.iso` does not carry EFSRVMIN at all.

- mtimes: iso 21:03:26, kernfull.elf 21:05:17, EFSRVMIN.E32 21:37:03, tools/mkgrubiso.py 19:50:06.
- `mkgrubiso.py` has zero references to EFSRVMIN/FATIOCLIENT; it stages only `build/user/*.E32` with size >= `E32_MIN_SIZE` (30000) plus EUSER.DLL and the two WADs.
- EFSRVMIN.E32 (16396 B) and FATIOCLIENT.E32 (6412 B) live only in `genuine/efsrv_min/`, are absent from `build/user/`, and are below the size cutoff anyway.
- Verified inside the ISO: `/programs.img` (64 MiB) contains no `EFSRVMIN`/`FATIO` FAT entries (control: `CALC.E32` present).
- These images are consumed only by `tools/r1_genuine_efsrv_test.py` and `tools/r1_genuine_fat_io_test.py`, which build their own FAT32 disk via mkfs.fat + mcopy.
- Kernel is not stale content-wise: ISO `/boot/kernfull.elf` md5 `12a32906ec25da87cc9150abc4aba2e3` == `build/kernfull.elf` (mtime bump only).
