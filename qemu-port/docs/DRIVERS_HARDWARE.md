# Hardware drivers in `variant/` (Kern-full image)

Scope: the Variant-owned, freestanding drivers under `qemu-port/variant/`. All of
them are plain C++ with raw port/MMIO access, no genuine `kernelhwsrv/` code and
no C runtime. They compile into `KERNFULL_OBJS` (Makefile line 312) from the
per-file rules at lines 223–283, and their public entry points are declared in
`variant/qemu_variant.h`. This document is a map of what exists, how each device
is reached, and how to exercise it.

## 1. Common driver pattern

* **PCI config access** — `qemu_pci.cpp` owns `PciRead32/PciWrite32` (address
  `0xCF8 = 0x80000000 | bus<<16 | dev<<11 | fn<<8 | reg&~3`, data `0xCFC`) and
  `PciFindClass(class, sub, &bus,&dev,&fn)` (bus 0, 32 devices, up to 8 functions
  when the header-type multifunction bit is set). All drivers scan bus 0 only;
  there is no secondary-bus walk. `PciList()` is the `pci` shell command.
* **BAR decoding** — each driver reads its BAR, masks bit 0 (I/O) vs. bit 3
  (64-bit) appropriately (`~3` / `~0xF`), and sets PCI command bits `0x06`
  (MEM space + bus master). `virtio-blk/rng` walk vendor capabilities in *any*
  BAR using byte-granular config reads because QEMU/q35 places the virtio
  capability block in BAR4, not BAR0.
* **Identity-mapped RAM** — `HwInit0` installs a flat 4 GiB map, so every DMA
  address programmed into a device is simply the virtual address of a static,
  aligned buffer (rings, PRDTs, HCCA, vrings, PCM buffers). High addresses
  (>= 0x80000000) are supervisor-only, which is why userland GUI/audio access
  is routed through slow execs.
* **Boot hooks** — every driver has a `__attribute__((constructor))` static
  function (`E1000BootInit`, `VnetBootInit`, `VblkBootInit`, `VrngBootInit`,
  `OhciBootInit`, `EhciBootInit`, `AudioBootInit`, `DisplayBootInit`,
  `MouseBootInit`, `GuiBootInit`, `AudioExecBootInit`, ...). `HwInit0` walks
  `.init_array` after paging and `TheAsic` are ready. Every init function is
  idempotent, so later shell commands just re-read the cached state.
* **Polling discipline** — storage, network, USB and audio drivers use bounded
  spin loops (status/CI/CC/DCH bits, ring indices) rather than device IRQs.

## 2. Interrupt infrastructure (what is *not* wired)

`qemu_variant_int.cpp` implements the genuine `Interrupt` class on the i8259.
IRQ gates live at vectors **0x30+** (0x20/0x21 are DPL3 exec-call gates). There
is a **16-slot** `gBindings[16]` table with `Bind/Unbind/Enable/Disable/Clear/
SetPriority`; `X86IrqDispatcher()` identifies the active IRQ from the 8259
In-Service Registers (OCW3 read of 0x20/0xA0), calls the bound `TIsr`, and
sends EOI (slave then master for IRQ >= 8). Only **IRQ0** (PIT → `QemuTickIsr` →
`NTimerQ::Tick`), **IRQ1** (PS/2 keyboard, Set-1 decode), **IRQ4** (COM1 RX) and
**IRQ12** (PS/2 mouse) are bound. IRQ12 is special: it is slave IR4, so
`mouse_init()` explicitly opens the master cascade IR2 (`In8(0x21) & ~4`) and
uses `QemuIrq::Enable(12)` (direct PIC read-modify-write), not
`Interrupt::Enable(12)`, whose shadow mask would re-mask live IRQs. **No PCI
INTx line is bound by any driver; there is no local APIC and no MSI/MSI-X.**

## 3. Storage map (FAT32 backend)

`qemu_ata.cpp` is the dispatch point for the FAT layer: **dev 0–3** = legacy IDE
(primary/secondary × master/slave, LBA28 PIO), **dev 4–7** = AHCI SATA ports
0–3, **dev 8** = GRUB Multiboot module snapshotted into high RAM as a
read-only RAM disk. `AtaCount/AtaPresent/AtaSectors/AtaModel/AtaRead/AtaWrite/
AtaFlush` forward `>=4` to `Ahci*` and special-case `8`. `Asic::Init3` mounts
dev 4 partition 0 as C: and aliases it (or dev 8 when diskless) as X:. Data
never lives in the guest RAM used by the heap: `QemuRamdiskInit()` copies the
module below `min(128 MiB, memUpperKB)` and above `0x2000000` during HwInit0.

## 4. Driver-by-driver reference

### qemu_pci.cpp — bus scanner (no hardware of its own)
* Identity: host bridge bus 0; class names in `ClassName()` (01.01 IDE, 01.06
  SATA, 02.00 ethernet, 03.00 VGA, 04.01 multimedia, 0C.03 USB).
* Data path: configuration-space read/write only; no BARs, no IRQ.
* Shell: `pci` prints `00:dev.fn vvvv:dddd class cc.ss <name>` and a device
  count. QEMU flags: none (always present). Test: visual check of `pci` output.

### qemu_ata.cpp — legacy IDE PIO
* Identity: ISA ports `0x1F0/0x3F6` (primary), `0x170/0x376` (secondary); no PCI
  ID. Commands `0xEC` IDENTIFY, `0x20/0x30` LBA28 read/write, `0xE7` flush.
* Init: `AtaInit()` selects each of 4 devices, issues IDENTIFY, detects ATAPI
  via the `0x14/0xEB` signature, decodes LBA28 sector count and byte-swapped
  model/serial words.
* Data path: polled `WaitNotBusy`/`WaitDrq` with 30M/10M spin budgets; 256-word
  PIO transfers via `inw/outw`; 400 ns delay via four status reads. Writes end
  with a cache flush. Fully polled, no IRQ.
* Shell: `ide` (lists disks and FAT partitions). Boot banner: `RAMDISK: GRUB
  module 0x... -> 0x... (N sectors)` for dev 8. QEMU flags: `-drive ...,if=ide`
  or none (AHCI test harness attaches its disk to `sata.0` instead). Test:
  `tools/test_cli_diskfull.py` exercises FAT read/write through this layer.

### qemu_ahci.cpp — SATA/AHCI DMA
* Identity: PCI class **01.06**, BAR5 = ABAR (32-bit memory). MEM|master set.
* Init: BIOS/OS handoff (BOHC OOS/BOS), GHC.AE, full HBA reset (GHC.HR — needed
  because SeaBIOS leaves stale CI/CR/FIS state), then set AE again. Per port:
  stop/clear errors/FRE/ST, program CLB/FB (no COMRESET: QEMU sets ERR_STAT on
  a DET pulse and never completes commands), check `PxSSTS DET == 3`, read
  `PxSIG` (0x00000101 ATA, 0xEB140101 ATAPI — skipped), issue IDENTIFY via
  `RunCmd()`, parse LBA48 words 83/100/102 or LBA28 words 60/61.
* Data path: one command slot (**slot 1**, slot 0 avoided for BIOS residue),
  1 KiB command list, 256 B FIS area, Register H2D FIS `0x27`, one PRDT entry
  (DBA = low 32 bits, DBC = len-1). Commands: `0xEC`, `0xC8/0xCA` (LBA28 DMA
  read/write), `0x25/0x35` (LBA48), `0xE7/0xEA` flush. Completion is polled on
  `PxCI`, with `PxIS/PxTFD` error decode and a diagnostic timeout print.
* Shell: `ahci` prints `ahci: ABAR %x PI %x ports %u` plus per-port model and
  `ahci: N disk(s)`. QEMU flags: `-device ich9-ahci,id=sata -device
  ide-hd,drive=d0,bus=sata.0`; works with VirtualBox AHCI and real HW in
  IDE/compatibility mode. Test: `tools/test_r1_g_boot.py`, most CLI tests.

### qemu_nic.cpp — e1000 (82540EM)
* Identity: PCI **8086:100E**, BAR0 32-bit MMIO. Discovered by vendor/device,
  not class. MEM|master set.
* Init: global reset (self-clearing RST), force link up (SLU) and ASDE, read
  MAC from EEPROM words 0–2 via EERD (DONE is bit 4), program RAL/RAH with the
  AV bit, set up 8-entry legacy TX/RX rings with 2048-byte buffers.
* Data path: `e1000_tx()` copies into `gTxBuf`, writes EOP/IFCS/RS descriptor
  flags and kicks TDT; `e1000_recv()` checks DD, copies out and *recycles the
  consumed index* into RDT (RDT is exclusive — writing the next cursor stalls
  RX after one packet). Non-blocking receive. Fully polled; `e1000_irq()` is
  only an ICR-ACK/link-state poll stub.
* Shell: `nic` prints `e1000: BAR0=... MAC=... link=...`; `nic arp [count]`
  builds ARP frames in the shell and waits for replies. Boot banner is the
  same `e1000:` line from `E1000BootInit()`. QEMU flags: `-netdev user,id=n0
  -device e1000,netdev=n0`. Test: `tools/r1_qemu_nic_test.py`.

### qemu_virtio_net.cpp — alternative polled NIC
* Identity: PCI vendor **1AF4**, device `1000` (legacy 0.9.5) or `1040..107F`
  (modern), subsystem device 1; fallback class 02.00 + subsystem 1. Reads the
  virtio-1.0 PCI capability list (types COMMON=1, NOTIFY=2, ISR=3, DEVICE=4) in
  BAR0; no caps means the fixed 0x9.5 I/O map (0x00/0x10/0x13/0x14).
* Init: reset, ACK→DRIVER, negotiate `VIRTIO_NET_F_MAC` (bit 5) and STATUS
  (bit 16), set up RX=0 / TX=1 split virtqueues (16 descriptors, 2048-byte
  buffers, 64 KiB reserved per queue for legacy), pre-post all RX buffers,
  read the 6-byte MAC from device config, set DRIVER_OK.
* Data path: `vnet_tx()` reclaims used slots, posts an avail entry and kicks
  `notify + queue_notify_off*notify_mul`; `vnet_recv()` reads the used ring and
  reposts the buffer. No IRQ/MSI-X. Boot banner: `virtio-net: BAR0=... MAC=...
  queues ready`. Shell: `vnet`, `vnet info`. QEMU flags: `-netdev user,id=v0
  -device virtio-net-pci,netdev=v0`. Test: `tools/r1_virtio_net_test.py`.

### qemu_virtio_blk.cpp — polled block storage
* Identity: PCI vendor **1AF4**, device `1001` (legacy) or modern `1040..107F`
  with subsystem 2 / class 01.80. Capabilities may be in BAR4.
* Init: reset, ACK→DRIVER, latch feature bits (SIZE_MAX, SEG_MAX, GEOMETRY,
  READ_ONLY, BLK_SIZE, SCSI, FLUSH) but negotiate none; one split queue (16
  slots, 64 KiB legacy vring reserve); read `le64` capacity from device config.
* Data path: every request is a 3-descriptor chain — 16-byte header (type,
  reserved, sector), data, 1-byte status (0 OK / 1 IOERR / 2 UNSUPP /
  3 WRITEERR). `kReqIn=0`, `kReqOut=1`, up to 128 sectors (64 KiB) per request,
  kick and poll used ring. READ/WRITE/FLUSH; writes rejected when RO.
* Shell: `vblk`, `vblk info`, `vblk test` (reads sector 0, writes/reads back
  sector 1). Banner: `virtio-blk: BAR0=... capacity=... sectors (... MB)`.
  QEMU flags: `-drive file=...,if=none,id=v0 -device virtio-blk-pci,drive=v0`.
  Test: `tools/r1_virtio_blk_test.py`.

### qemu_virtio_rng.cpp — polled entropy source
* Identity: PCI vendor **1AF4**, device `1005` (legacy) or modern with
  subsystem device 4 (VIRTIO_ID_RNG). Same capability walk as virtio-blk.
* Init: reset, ACK→DRIVER, latch (but do not negotiate) features, one split
  queue of 16 writable descriptors, DRIVER_OK. Device config is empty.
* Data path: `vrng_get()` posts one `VRING_DESC_F_WRITE` buffer (up to 64 KiB),
  kicks, polls the used ring, returns `e.iLen` clamped to the caller's length.
* Shell: `rng`, `rng info`, `rng test` (fetches 16 bytes). Banner:
  `virtio-rng: BAR=... features=... queue ready (n=...)`. QEMU flags: `-object
  rng-random,filename=/dev/urandom,id=rng0 -device virtio-rng-pci,rng=rng0`.
  Test: `tools/r1_virtio_rng_test.py`.

### qemu_usb_ohci.cpp — USB 1.1 host (OHCI)
* Identity: PCI class **0C.03.10** (programming interface 0x10), BAR0 MMIO.
  Chosen over UHCI because VirtualBox has no UHCI.
* Init: HcReset (HCR), MIE off (polled), 256-byte aligned HCCA with a
  persistent interrupt ED in all 32 periodic slots, FmInterval=11999,
  Periodic=0x2A2F, LsThresh=0x0628, global power, operational enable
  (CLE|IE|PLE), port count from `HcRHDescriptorA[7:0]` (capped at 4).
* Data path: control ED rebuilt per transfer with SETUP + DATA + status TDs,
  CLF kick, poll each TD's CC field (0xF = not accessed, 0 = OK) with a 2 s
  tick budget; interrupt-IN uses one persistent TD with explicit data toggle.
  256-byte transfer buffer, no transfer crosses a 4 KiB page.
* Shell: `usb` / `usb info` prints `ohci: rev=xx bar=x pci=00:xx.x ports=N`
  and per-port connect/enable/reset/speed. Banner: same line from
  `OhciBootInit()`. QEMU flags: `-device pci-ohci,id=ohci -device
  usb-mouse,bus=ohci.0`. Test: `tools/r1_usb_ohci_test.py`.

### qemu_usb_ehci.cpp — USB 2.0 host (EHCI)
* Identity: PCI class **0C.03.20** (e.g. QEMU q35 ICH9 `8086:293A` at 00:1d.7),
  BAR0 MMIO; operational registers at `BAR0 + CAPLENGTH` (QEMU uses 0x20).
* Init: scan explicitly by prog-if since `PciFindClass` ignores it; halt,
  soft-reset (HCRST self-clearing), USBINTR=0, FrIndex=0, empty 1024-entry
  frame list, one self-linked idle async QH, CONFIGFLAG.0 to claim all root
  ports from companion controllers, then `Run|Async` (no periodic schedule).
  Reset uses a bounded spin because PIT ticks are not bound at constructor
  time. Port count from `HCSPARAMS[7:0]` (capped at 8).
* Data path: only PORTSC status/reset are exported — `ehci_port_status()` and
  `ehci_port_reset()` (RESET 1→0 edge, poll PED). Bulk/control qTD chains are
  not implemented. Polled only.
* Shell: `ehci`, `ehci info` prints `ehci: ver=x bar=x pci=... caplen=x hccp=x
  ports=N` plus per-port speed. QEMU flags: present by default on `-M q35`.
  Test: `tools/r1_usb_ehci_test.py`.

### qemu_xhci.cpp — USB 3.0 host (xHCI)
* Identity: PCI class **0C.03.30**, BAR0 capability MMIO (LE). Derives op/rt
  (doorbell) offsets from CAPLENGTH/Dboff/Rtsoff.
* Init: `XhciInit()` finds the controller; `XhciStart()` halts, resets,
  programs DCBAAP/CRCR/ERSTBA/ERDP, sets `Config.MaxSlotsEn`, disables
  interrupter IMAN.IE, and runs with no INTE. One command ring (16 TRBs) and
  one 32-entry event ring; scratch buffers (<= 16) and one pair of device
  contexts only.
* Data path: `RunCommand()` posts one TRB, rings doorbell 0 and consumes events
  by toggling cycle bits (`NextEvent`). Port reset/enable-slot/address-device
  (BSR=0) establish EP0; `Control()` uses a 2/3-TRB Setup/Data/Status TD.
  `XhciKeyboardInit()` parses device+config descriptors, finds a HID
  boot-keyboard interrupt-IN endpoint, issues SET_CONFIGURATION and
  SET_PROTOCOL, then `XhciKeyboardPoll()` re-posts 8-byte reports and pushes
  new key-down events into the shared shell ring via `QemuKbdPushUsb()`.
* Shell: `xhci`, `xhcistart`, `xhciports`, `xhcislot N`, `xhciaddr N`,
  `xhcidesc N`, `xhcikbd N`, `xhcinoop`, plus `usb` (HID status). Banner:
  `xhci: HID boot keyboard port N slot N ep N iface N`. QEMU flags: `-device
  qemu-xhci -device usb-kbd`. Test: `tools/test_xhci_keyboard.py` (injects
  `version` via HMP `sendkey`).

### qemu_audio.cpp — AC'97 PCM-OUT (see also GUI_AUDIO.md)
* Identity: PCI class **04.01**, QEMU 82801AA with vendor IDs `8384:7600`
  (Creative). BAR0 = NAM (mixer), BAR1 = NAB (bus master); both are **I/O
  BARs** on QEMU (typically `0xC000`/`0xC400`), but the helpers select in/out
  vs. MMIO from the BAR type bit. MEM|IO|bus-master are all enabled.
* Init: reset PCM-OUT engine (PoCR.RR), cold reset (GLOBCTL.CR), codec reset
  via NAM reset=0, poll vendor ID ready, set 48 kHz front DAC, unmute master
  and PCM-out volumes.
* Data path: one 2-entry buffer-descriptor list (addr + ctl_len, IOC bit 31)
  feeding a static PCM buffer; `audio_play_test()` emits a 0.1 s 1 kHz sine
  (48-entry integer table duplicated to both channels), `audio_tone(freq,ms)`
  phase-accumulates the same table with 16.16 interpolation for up to 500 ms.
  Both write BDBAR, LVI=0, clear status and set RPBM, then poll `PoSR.DCH` and
  `PoPICB == 0`. Fully polled; no PCI IRQ.
* Shell: `audio` shows BARs/vendor ID/GLOB_STA; `audio play` emits the 1 kHz
  tone. Banner: `audio: AC97 00:xx.x BAR0=...(IO) BAR1=...(IO)` and
  `audio: vendor ID 8384:7600 codec ready`. QEMU flags: `-audiodev none,id=a0
  -device AC97,audiodev=a0`. Test: `tools/r1_qemu_audio_test.py`.

## 5. Test scripts at a glance

| Driver | Script | Shell verbs fed |
|---|---|---|
| PCI / IDE | `tools/test_cli.py`, `tools/test_cli_diskfull.py` | `pci`, `ide`, FAT verbs |
| AHCI | `tools/test_r1_g_boot.py` | `ide`, `dir`, `exec` |
| e1000 | `tools/r1_qemu_nic_test.py` | `nic`, `nic arp 3` |
| virtio-net | `tools/r1_virtio_net_test.py` | `vnet`, `vnet info` |
| virtio-blk | `tools/r1_virtio_blk_test.py` | `vblk`, `vblk info`, `vblk test` |
| virtio-RNG | `tools/r1_virtio_rng_test.py` | `rng`, `rng info`, `rng test` |
| OHCI | `tools/r1_usb_ohci_test.py` | `usb`, `usb info` |
| EHCI | `tools/r1_usb_ehci_test.py` | `ehci`, `ehci info` |
| xHCI | `tools/test_xhci_keyboard.py` | `xhcikbd 5`, `usb` |
| AC'97 | `tools/r1_qemu_audio_test.py` | `audio`, `audio play` |

All scripts use `qemu-system-i386 -M q35 -m 128 -display none -serial stdio
-kernel build/kernfull.elf` with a 32 MB raw FAT32 disk and `-monitor none
-no-reboot`, adding only the device-specific flags listed above. The host test
macros `R1_STORAGE_HOST_TEST` (ATA/AHCI) replace port/MMIO access with injected
register stubs so the storage layer can be unit-tested off-target.
