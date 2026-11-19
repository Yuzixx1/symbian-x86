# Hardware worksheet — non-destructive smoke (R0-HW)

Card: **R0-HW** (see `docs/parallel_work.md`). Worksheet revision **1**, drafted
from commit `fa62323` + current uncommitted workspace, Oct 2026.

**Status of this file: a draft form, not a report.** Every field is blank and
nothing here asserts that the port, GRUB path, PS/2, VGA or storage works on
any physical machine. All "reference" observations below were captured in QEMU
and/or VirtualBox and are anchors for comparison only; they carry **no**
hardware-verified label (see `project_roadmap.md`, "Verification vocabulary").
Facts enter this worksheet only when the project owner records them.

Status labels to use in every result field: `PASS` / `FAIL` / `HANG` /
`UNKNOWN` / `NOT-TRIED` / `N-A`, plus an evidence pointer (photo number, or
transcribed text). Fill `UNKNOWN` freely — unknown facts are the point of this
worksheet; guessing is worse than blank.

Companion files: `hw_tests.md` (existing broader plan; USB/AHCI/VBox detail),
`manual_test_order.md` (functional shell checks, requires a prepared FAT test
disk — **not** part of this smoke), `full_os_checklist.md`, `project_roadmap.md`
(R0 gate wording).

---

## 0. Non-destructive policy (read before anything)

The reference machine is a **Pentium 4–era PC whose hard disk contains data
the owner must not lose**. This smoke needs **no test disk at all**, so the
default run has media-writes = zero. The rules below are hard requirements.

1. **Never write any media while attached to the reference PC's system disk
   chain.** Media prep (ISO burn, `dd`) happens on the build PC with the
   target blank/replaceable medium only. If in doubt, power down, disconnect,
   reconnect only a clearly labeled blank medium.
2. **Prefer a CD-R.** `build/symbian-grub.iso` is an El Torito image built with
   pycdlib (`tools/mkgrubiso.py`, no isohybrid). A USB stick written with `dd`
   from it is therefore an **unverified boot path** — `hw_tests.md`'s
   "dd to USB stick" advice is not known to boot on real BIOS. CD-R burn is
   the low-risk first medium. Record attempts of the USB path as facts, not
   failures.
3. **Do not run any mutating shell command in this smoke.** Prohibited here:
   `format`, `write`, `copy`, `ren`/`rename`, `del`/`erase`, `mkdir`/`md`,
   `rmdir`/`rd`, `date <set...>`, `heaptest`. `DATE` with no arguments is
   allowed (read; do not sync/adjust the CMOS clock a visible drift might
   suggest — record the drift instead).
4. `mount` with **no arguments** tries to mount every FAT32 partition it finds
   on every ATAPI/AHCI disk present — which could include the owner's real
   data partition. Rule: if more than one disk is present and they are not all
   labeled disposable, **skip mounting entirely** (Stage E stays
   `UNKNOWN`); if all attached disks are disposable, no-arg `mount` is
   allowed, and any FAT error is recorded, not retried.
5. Do not flash firmware, do not change BIOS/firmware settings to make a test
   pass. If a temporary change is strictly needed (e.g. boot order), note the
   original value first in the form below and revert afterwards; the owner
   decides acceptable changes, not this sheet.
6. No USB probing in this smoke (it is covered in `hw_tests.md`); PS/2 is the
   input path assumed throughout. Plug nothing into USB that isn't there
   already.
7. The only way out of the kernel is `reboot` (8042 controller pulse) or
   power-off. Both are allowed at any point; nothing else needs to be run to
   "clean up".

Sequencing rules: do the stages in order; each stage's result code decides
whether to stop (see §2 "stop conditions"). Two failed reproduced attempts
(`reboot` and retry once) for the same stage stop that stage permanently.

---

## 1. Preflight on the build PC (before the reference PC)

Nothing in this stage writes media attached to the reference PC.

1. Record the exact artifact fingerprint:
   - ISO used: `build/symbian-grub.iso` (BIOS+UEFI GRUB combo, primary) —
     optional second: `build/symbian-pc-full.iso` (classic El Torito
     reference; GRUB is still the primary).
   - `sha256sum` of the ISO chosen (write here): `________`
   - Build command and date: default `make` from `qemu-port/` — record any
     deviation: `________`
2. QEMU parity boot of the same ISO so the owner has a fresh reference for
   what Stage A/B/C should look like (this is a smoke of the copied media;
   it does not run `test_stable.py`, requires no disk, and must not be run
   against a writable image someone else uses):
   ```
   qemu-system-i386 -cdrom build/symbian-grub.iso -m 128 -display sdl -no-reboot
   ```
   Checkpoint (reference, QEMU-verified in this branch's build):
   GRUB menu (10 s countdown) → entry `Symbian EKA2 full kernel (genuine)` →
   VGA clears → banner `Symbian EKA2 x86 full kernel -- interactive console`
   and `sched by TheScheduler, IRQs via genuine vectors + variant Bind` →
   prompt `EKA2>`; typed text echoes.
   Parity QEMU result: `________`
3. Media creation (on build PC only): burn the ISO to a labeled blank CD-R
   with the usual Windows/Linux tool. Record tool + brand: `________`
   Optional (low priority, treat as measurement): `dd` of the ISO to a
   dedicated stick *after* confirming with the owner which stick is
   disposable; stick name: `________`

---

## 2. Smoke stages (run at the reference PC; observe on VGA)

Estimate 30–60 min plus retries. All observation is by eye + camera:
**no serial capture exists on the reference machine**, so it is not used here.
Photograph any screen state you cite; a transcription of the last 5–10 visible
lines is acceptable when a photo is impossible.

### Stage A — BIOS boot and GRUB

1. Power up, enter BIOS setup by the usual key **but only to look**. Record:
   - Firmware vendor/version/date: `________`
   - Installed RAM as reported: `________`
   - Disk controller/drives shown in setup: `________`
   - Boot menu options available (CD / USB-HDD / USB-CD / PATA / PXE / …): `________`
   - Exit without changes: code `________`
2. Insert the CD-R (or the tested stick), select CD boot in the one-time boot
   menu if possible (persistent boot-order changes count under policy §0.5).
   Record:
   - What the BIOS boot menu offered and what you picked: `________`
   - GRUB screen appearance: menu text visible / countdown visible /
     nothing at all: `________`
   - Did you select the default entry (`Symbian EKA2 full kernel (genuine)`)
     or the nanokernel entry? (Smoke report should be on the **full kernel**.)
     `________`
   - Reference text printed by GRUB after selection ("Booting full kernel…"
     then "Multiboot ok, booting…"): did these appear? `________`

**Stop conditions:** no GRUB screen at all after BIOS handoff with a valid
boot medium → stop. Record BIOS model + medium. Reproduce once via power
cycle; a twofold-reproduced empty handoff is the final result for Stage A —
do not fall back to the second ISO except as a separate, clearly recorded run.

### Stage B — VGA/console integrity

1. Measure what you actually see and record, using "approximately" freely:
   - Chars per row ≈, visible rows ≈ (reference is 80×25): `________`
   - Attribute/color of text and background (reference: light-grey on black,
     attr code 0x07): `________`
   - Hardware text cursor: blinking block at the prompt position? `________`
   - Any interleaved BIOS/GRUB graphics remnants, blur, or a resolution that
     is clearly not text-port (possible if the board's BIOS left a graphics
     mode; the port writes directly to 0xB8000 text VRAM): `________`
2. At the `EKA2>` prompt, run `CLS` (screen clears, prompt redrawn at top).
   Then run `HELP DIR` (its usage text spans several lines and exercises
   scrolling). Record both:
   - `CLS` behavior: `________`
   - `HELP DIR` output readable + scroll moves cursor to bottom row: `________`

**Stop conditions:** unreadable/odd-resolved console → photograph it, code
`FAIL`, stop; the rest of the sheet can then only be filled from photos.

### Stage C — PS/2 keyboard

Ensure a **PS/2** keyboard is the live input (reference PC should have one; a
USB keyboard is *not* a substitute for this stage — if only USB exists, record
`N-A` here and route back to `hw_tests.md`).

1. Type `HELP` and observe **character echo as typed**, then Enter:
   - Echo of typed characters: `________`
   - HELP list printed: `________`
2. Shift behavior: type `HELP DIR` normally again, then `heLp DiR` holding
   shift irregularly. Record: were uppercase shown for shifted letters,
   lowercase otherwise (reference: the port implements Shift per shifts
   table, not CAPS-lock state)? `________`
3. Backspace: type `ABC`, press Backspace 3×, confirm the line is empty
   visually (reference: blanks erase backwards via text VRAM), then press
   Enter on the empty line (harmless). Record: `________`
4. Keys not mapped by the port: press one arrow key and one F5 key.
   Reference (port code `kMap`): those scancodes are unmapped and print
   nothing; CAPS-lock/extended-from-0xE0 behavior is what this step records.
   Record what each printed/behaved: `________`
5. Idle-check: leave the machine at the `EKA2>` prompt for ≥5 minutes doing
   nothing, then type `UPTIME` + Enter. Record whether input still worked
   after the idle period (a partial predictor for IRQ-loss problems):
   `________`, and the uptime readout (should exceed your idle time):
   `________`

**Stop conditions:** no echo at all → HANG/FAIL, stop; try nothing further
until keyboard is resolved.

### Stage D — read-only machine-fact commands

Run in this exact order at the `EKA2>` prompt; transcribe outputs verbatim
into the form in §3 (or attach transcriptions). All are non-destructive
reads/probes; their intent here is *fact capture*, not functional testing:
`VERSION`, `UPTIME`, `MEMINFO`, `DATE`, `PS`, `THREADS`, `PCI`, `IDE`, `AHCI`.
For each: result code + photo/transcript pointer in §3. Not-yet-known
behaviors to watch out for and record honestly on real silicon:

- `VERSION` prints a CR0 value and a CPU-feature word captured at boot —
  transcribe it fully (the port's CPU-detection path was written against
  QEMU's CPU; odd values on real hardware are facts, not faults).
- `PCI` scans bus 0 only via 0xCF8/0xCFC reads; a real chipset may have
  bridges, buses 1+, or quirks — record all device lines and the total count.
- `IDE` reads ATA IDENTIFY-ish state from the port's driver.
  ATAPI devices ("ATAPI CDs skipped" in shipped code) may behave unexpectedly
  on a desktop with an optical drive attached — record any lines where the
  count/behavior differs from QEMU.
- `AHCI` is only meaningful if an AHCI-capable controller and SATA disk are
  present (Pentium 4-era: unlikely, expect `UNKNOWN`/error).
- `DATE` read-only: transcribe; if the printed date/time is manifestly wrong
  (dead CMOS battery), record the drift, do not set the clock.

### Stage E — storage identity (identity only)

Goal here is **identity only** — no FAT detection is guaranteed and mounting
rules under §0.4 govern everything.

1. `drive`, `ide` (already done in Stage D), `ahci`. Transcribe all.
2. Boot device: the boot came from the optical medium, so whether the port
   can see that CD/ATAPI drive at all is unknown by design. Record what
   ATAPI devices appeared in the drive listing, transcribing model/sector
   fields exactly as printed (zeros or absent values are facts): `________`
3. Only if **every** attached internal disk is confirmed disposable do you
   proceed to `mount` (no args), then `vol` + `dir`. Record what was mounted
   (letters, devices, partitions) and the directory listing only — no files
   are opened beyond directory listing; no writes of any kind may be
   attempted. `________`
4. If no internal disk was connected at all (recommended smoke config), mark
   Stage E `N-A`.

### Stage F — shutdown

Option 1: `reboot` and confirm the board actually reboots (8042 keyboard-
controller pulse; some modern boards ignore it, the port then prints
"reboot failed (8042 ignored), halting"). Record which line printed and
what the machine did: `________`
Option 2: hard power-off — allowed at any point in this smoke, since it
issued no disk writes. Record which option was used and, for `reboot`,
whether a second full boot through GRUB completed (free retry evidence):
`________`

Save all photos/transcripts with the naming `HWWS1-stage-X-#N`.

---

## 3. Hardware facts form

Owner fills at the reference PC from BIOS setup + Stage D transcripts.
Allowed value = any string, or `UNKNOWN`. Do not infer chipset facts not
visible; the PCI transcript is the in-kernel source of truth where
transcribed.

```
PC nickname:                     ____
Motherboard model / retail name: ____
Chipset (north/south or hub):    ____   # e.g. 845G / ICH4 — from BIOS text or silkscreen only
Firmware vendor & version/date:  ____
CPU (as reported in setup):      ____   # e.g. "P4 2.4G"
CPU feature word from VERSION:   ____   # feats=0x____ CR0=0x____
RAM total (setup):               ____   # MB
RAM total (from MEMINFO ram free-line, if printed): ____
PS/2 ports present (K/M):        ____   # both / only K / none
DB9 COM port present (rear):     ____   # yes / no / header unknown
LPT present:                     ____
Monitor input used:              ____   # analog CRT / LCD-DVI-analog / other
Disk interface in setup:         ____   # IDE-PATA / SATA-AHCI / SATA-compat / mixed / unknown
Disk drives in setup (model/cap):____
Optical/ATAPI drives present:    ____
Boot options seen in setup:      ____
Boot media actually used:        ____   # CD-R / USB-stick (attempt N)
Boot succeeded via:              ____   # GRUB-BIOS / GRUB-UEFI / other
Keyboard used in Stage C:        ____   # PS/2 / USB (recording why, if USB)
CMOS date/time readout (DATE):   ____
Serial console attempted?        ____   # no; this sheet is VGA-only
Photos roster (numbers ↔ stage): ____
```

PCI transcript (verbatim paste from Stage D):

```
<transcribe all lines here>
```

MEMINFO / PS / THREADS transcripts (verbatim):

```
<transcribe>
```

IDE / AHCI / drive transcripts (verbatim):

```
<transcribe>
```

---

## 4. Stage result table (fill per run)

| Stage | Run # | Result code | Evidence (photo/transcript) | Notes |
|---|---|---|---|---|
| A BIOS boot | 1 | | | |
| A GRUB menu | 1 | | | |
| B VGA clarity | 1 | | | |
| C PS/2 echo | 1 | | | |
| C idle input | 1 | | | |
| D each read-only cmd | | | | |
| E storage identity | | | | |
| F shutdown | | | | |

---

## 5. Failure capture protocol (any stage)

1. Photograph the screen immediately (do not reboot first).
2. Write down: last key typed, whether the prompt/echo had frozen visibly,
   whether the machine had already emitted beep codes at any point.
3. Power-cycle. Retry that stage once. If it reproduces, stop there.
4. If failure is at boot, also record: BIOS boot screen behavior before GRUB,
   and whether a different known-good boot medium on this machine survives
   the same stage (if a second medium is handy). This decides whether Stage A
   fails at firmware or GRUB-path level, which is the fact that matters.
5. Never assume VGA died because nothing prints — the failure could equally
   be input pull or kernel dispatch. Record what *did* print last; the last
   visible line is itself a fact.

---

## 6. Return/handoff (owner → workspace)

Paste the filled form + result table back into the workspace discussion;
the integrator records them into `hw_tests.md`'s report section (only after
the owner says so) and only updates the roadmap R0 wording when a stage has
both a fill in this sheet and the run's photos archived. Items deliberately
left for other cards: USB/xHCI probes, AHCI/SATA mounts, program execution —
see `hw_tests.md` and `manual_test_order.md`.

## 7. Outstanding limitations of this worksheet (assumptions/unknowns)

- USB-from-ISO boot via `dd` is assumed unverified (pycdlib El Torito has no
  isohybrid MBR; evidence: `tools/mkgrubiso.py`).
- Console behaviors are written against 80×25 VGA text mode shaped by the
  port's direct 0xB8000 writes; nothing here asserts the reference BIOS hands
  off in that mode (Stage B's whole purpose).
- GRUB handoff used `multiboot` (not multiboot2); its suitability for other
  firmware generations is itself one of the things Stage A measures.
- The card's "IDE **or unknown**" applies as written: storage bring-up beyond
  identity is out of this sheet's scope on purpose; deviations are facts, not
  failures.
- No hardware claims live in this draft, including in the results table:
  it stays empty until the owner records runs.
