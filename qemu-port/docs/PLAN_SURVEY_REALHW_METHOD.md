# PLAN SURVEY — real-PC boot test method (CD-R boot of the combo ISO)

Local-only survey of the test logistics implied by
[PLAN_SURVEY_HW_USB.md](/home/yuzi/Documents/Sybian%20REAL/qemu-port/docs/PLAN_SURVEY_HW_USB.md):
what hardware/workflow already exists in `qemu-port/`, what the user must
provide, and the concrete first real-PC boot checklist. Read-only survey; no
web, no downloads, no QEMU runs, nothing committed.

## Existing local tooling/policy

- **Policy / procedure docs (the core asset):**
  - [hw_worksheet.md](/home/yuzi/Documents/Sybian%20REAL/qemu-port/docs/hw_worksheet.md) —
    card R0-HW non-destructive smoke worksheet (rev 1, draft, all fields blank).
    §0 hard non-destructive policy (media prep on build PC only, prefer CD-R,
    no mutating shell commands, mount rules, no BIOS changes, no USB probing,
    `reboot`/power-off are the only exits); §1 build-PC preflight (sha256 of
    ISO, QEMU parity boot, media creation); §2 smoke stages A–F (BIOS/GRUB,
    VGA, PS/2, read-only commands, storage identity, shutdown) with stop
    conditions; §3 machine-facts form; §4 result table; §5 failure-capture
    protocol; §7 stated limitations.
  - [project_roadmap.md](/home/yuzi/Documents/Sybian%20REAL/qemu-port/docs/project_roadmap.md) —
    verification vocabulary ("Hardware-verified" requires the owner's smoke
    report on the reference PC; otherwise "QEMU-ready"), R0 gate wording, and
    the reference-machine profile: Pentium 4–era Intel board, legacy BIOS,
    PS/2 input, IDE/PATA *suspected*, serial port *unknown*, "No serial
    capture is available now, so the smoke procedure must be observable on
    VGA".
  - [PLAN_SURVEY_HW_USB.md](/home/yuzi/Documents/Sybian%20REAL/qemu-port/docs/PLAN_SURVEY_HW_USB.md) —
    feasibility verdict: CD-R burn of `build/symbian-grub.iso` is the low-risk
    first medium; USB-stick `dd` of that ISO is an unverified boot path.
  - [r1_card_a5_phys_hw_test_v1.md](/home/yuzi/Documents/Sybian%20REAL/qemu-port/docs/r1_card_a5_phys_hw_test_v1.md) and
    [r1_e32_validate_phys_hw_prep_v1.md](/home/yuzi/Documents/Sybian%20REAL/qemu-port/docs/r1_e32_validate_phys_hw_prep_v1.md) —
    older card A-5/A-7 procedures that assume dd-to-USB; their USB advice
    predates the worksheet's no-isohybrid warning and should be treated as
    measurement, not the plan.
  - [r1_phys_hw_test_checklist_v1.html](/home/yuzi/Documents/Sybian%20REAL/qemu-port/docs/r1_phys_hw_test_checklist_v1.html) —
    printable HTML checklist (boot → FAT mount → hello.e32 → portfilesrv);
    extends beyond the R0-HW smoke and needs a prepared FAT disk.
- **ISO mastering tools:** `tools/mkiso.py` (standalone El Torito ISO **with**
  `write_hybrid_mbr()` — so `build/symbian-pc.iso` / `symbian-pc-full.iso` are
  USB-dd-bootable via MBR chainload) and `tools/mkgrubiso.py` (GRUB2 BIOS+UEFI
  combo via pycdlib, El Torito only, **no** isohybrid MBR — hence the CD-R
  preference for `build/symbian-grub.iso`). Makefile targets: `iso`, `isofull`,
  `isogrub`, `qemu-cdrom`, `qemu-cdfull`, `qemu-uefi`, `qemu-hd`, `qemu-capture`.
  Prebuilt ISOs already exist in `build/` (symbian-grub.iso 75.8 MB,
  symbian-grub-bios.iso, symbian-pc.iso, symbian-pc-full.iso).
- **Disk-image tools (virtual media only):** `tools/mkdisk.py` (blank/seeded
  HDD images for QEMU/VBox), `tools/make_fat_disk.py`, `tools/mkfat.py`,
  `tools/make_disk.py` — none write physical media.
- **Serial capture:** QEMU-only. `tools/r1_qemu_smoke.py` and
  `make qemu-capture` drive `-serial stdio` and grep the log for
  panic/exception lines. **No real-HW serial capture tool exists**; the
  reference PC's serial port is unknown, so the worksheet is VGA + camera
  only.
- **Burning / USB imaging scripts:** none in the repo — the worksheet
  deliberately leaves the CD-R burn to "the usual Windows/Linux tool" on the
  build PC and records tool + brand in the form.
- **Known reference-machine notes:** only the roadmap/worksheet profile above
  (Pentium 4–era Intel board, legacy BIOS, PS/2, VGA; hard disk holds owner
  data and must not be touched; no test disk needed). No separate inventory
  doc exists.
- **Referenced but missing companions:** `hw_worksheet.md`, `parallel_work.md`
  and `project_roadmap.md` cite `hw_tests.md`, `manual_test_order.md` and
  `full_os_checklist.md`; none of these files exist in the repo yet — the
  worksheet's "report section" has nowhere to land until they are created.

## Prerequisites user must provide

1. **Build PC** (separate from the reference PC) with a CD-R burner and
   blank, labeled CD-R media; media prep happens here only.
2. **Reference PC**: Pentium 4–era machine per the roadmap profile — legacy
   BIOS, PS/2 keyboard, VGA monitor. Recommended smoke config: **no internal
   disk connected** (Stage E becomes N-A); the owner's data disk stays
   untouched.
3. **Optional measurement media:** one dedicated, owner-approved disposable
   USB stick for a `dd` attempt (recorded as fact, not failure).
4. **Camera/phone** for screen photos (naming `HWWS1-stage-X-#N`) since no
   serial capture exists.
5. **The artifact:** a fresh `make` build of `build/symbian-grub.iso` plus
   its `sha256sum`, and a QEMU parity boot of the same ISO as reference.

## Boot test steps

Follow `hw_worksheet.md` §1–§2 in order at the reference PC (est. 30–60 min):

1. **Preflight (build PC):** build/default `make`; record sha256 of
   `build/symbian-grub.iso`; QEMU parity boot
   (`qemu-system-i386 -cdrom build/symbian-grub.iso -m 128 -display sdl -no-reboot`)
   to confirm GRUB menu → banner → `EKA2>`; burn the ISO to the labeled CD-R
   and record tool + brand.
2. **Stage A:** BIOS setup look-only (record firmware, RAM, disks, boot
   options; exit without changes); one-time boot menu → CD; expect GRUB menu
   (10 s countdown), select `Symbian EKA2 full kernel (genuine)`, expect
   "Booting full kernel…" / "Multiboot ok, booting…". Stop if no GRUB screen
   after a reproduced retry.
3. **Stage B:** VGA integrity — 80×25, attr 0x07, hardware cursor; run `CLS`
   and `HELP DIR`. Unreadable console → photograph, FAIL, stop.
4. **Stage C:** PS/2 keyboard — `HELP` echo, shift case, backspace, unmapped
   keys (arrow/F5), then ≥5 min idle + `UPTIME`.
5. **Stage D:** read-only fact commands in order: `VERSION UPTIME MEMINFO
   DATE PS THREADS PCI IDE AHCI` — transcribe verbatim.
6. **Stage E:** storage identity only (`drive`, `ide`, `ahci`); `mount`
   (no args) only if every attached disk is confirmed disposable; otherwise
   N-A.
7. **Stage F:** `reboot` (watch for the 8042 pulse line) or hard power-off;
   a second full boot through GRUB is free retry evidence.

## Evidence to capture

- Photo of every cited screen state, named `HWWS1-stage-X-#N`; where no photo
  is possible, a verbatim transcription of the last 5–10 visible lines.
- sha256 + build command/date of the ISO; parity QEMU result.
- Filled §3 machine-facts form (PC nickname, board, chipset, firmware, CPU,
  CPU feature word from `VERSION`, RAM, PS/2/COM/LPT presence, monitor,
  disk interface, boot media used, CMOS date) and §4 stage result table with
  PASS/FAIL/HANG/UNKNOWN/NOT-TRIED + evidence pointer per row.
- Verbatim transcripts: PCI list, MEMINFO/PS/THREADS, IDE/AHCI/drive.
- On any failure: immediate photo before reboot, last key typed, beep codes,
  and whether a known-good medium survives the same stage (§5 protocol).
- Archive boot mode, media, controller, visible error and last successful
  action (roadmap vocabulary requirement for any future "hardware-verified"
  claim).

## Fallback if no hardware

- QEMU-only CI remains the primary gate: `make qemu-cdrom` / `qemu-cdfull`
  and the `tools/test_stable.py` suite (`tools/r1_qemu_smoke.py` for
  serial-log panic scanning). Call the milestone **QEMU-ready, not
  complete**, per the roadmap vocabulary.
- VirtualBox is an allowed independent check (its BIOS hang on the
  hand-written EDD path is already noted) but is not a substitute for the
  physical-PC gate.
- If CD boot fails on the reference PC but a USB attempt is wanted, record it
  as a measurement; a durable fix is adding an isohybrid-style MBR to
  `tools/mkgrubiso.py` (variant-side only, mirroring `tools/mkiso.py`'s
  `write_hybrid_mbr()`), after which `symbian-grub.iso` becomes dd-bootable.
- If the reference PC itself is unavailable, do the BIOS-inventory and
  checklist dry-run against any available legacy PC, and keep all hardware
  claims at UNKNOWN.

## First steps

1. `make` in `qemu-port/`, record `sha256sum build/symbian-grub.iso`, run the
   QEMU parity boot to confirm the expected banner and `EKA2>` prompt.
2. Burn the ISO to a labeled blank CD-R on the build PC; record tool + brand
   in worksheet §1.
3. Run worksheet stages A–F at the reference PC (no internal disk attached,
   PS/2 keyboard, VGA + camera), fill the §3/§4 forms, and hand the filled
   sheet back per §6 — creating `hw_tests.md`'s report section if needed.
