# R1 NIC design v1 — D-06 findings (read-only)

Read-only investigation; only this doc is created. `docs/full_os_checklist.md` (brief §9)
does NOT exist in this checkout (referenced only in r1_abi_contract_v0.md); D-06 is defined
in PROJECT_GOAL.md:62 and project_roadmap.md:231 as "D-06 -> N-01..N-04 NIC -> stack ->
sockets -> apps", after console/FS. kernelhwsrv/ untouched; no make/QEMU run.

## 1. NIC hardware QEMU presents
- Smoke test `tools/r1_qemu_smoke.py:144-154` boots `-M q35 -m 128` with **no explicit
  `-netdev`/`-nic`/`-device` NIC**. QEMU's default applies: one **Intel e1000** PCI NIC
  (class 02.00 ethernet, PCI 8086:10xx) on user-mode/SLIRP networking. README.md:404 confirms
  `pci` scan sees "e1000". virtio-net / pcnet / rtl8139 appear only if added explicitly.
- `variant/qemu_pci.cpp` already finds it: `PciFindClass(0x02,0x00,...)`, `PciRead32`/`PciWrite32`
  (0xCF8/0xCFC), `PciList()` classifies class 02 as "ethernet"/"network" (:69). No consumer of
  the network class exists.

## 2. NIC driver code: NONE
- Grep across qemu-port (cpp/c/h/S/py/md): only source hit is the "ethernet" string in
  qemu_pci.cpp:69. No e1000/virtio/pcnet driver, no MAC/ARP/IPv4/ICMP/TCP/UDP, no RX/TX rings,
  no packet buffers, no socket API. PROJECT_GOAL.md:43-44 marks NIC drivers + TCP/IP "Not started".
  build/export/exec_enum.h has zero network execs; no ESOCK/INET DLL or network EUSER ordinals.
- scheduler (`kernelhwsrv/.../csched.cpp`) has no network-related code.

## 3. Interrupt infrastructure (usable, needs one gap filled)
- i8259 PIC remapped to 0x20/0x28 (qemu_assp.cpp:51-58); 16-slot `Interrupt::Bind/Unbind/
  Enable/Disable/Clear/SetPriority` (qemu_variant_int.cpp:46-104); 8259 ISR-driven
  `X86IrqDispatcher` + EOI (:110-145). Only IRQ0(tick)/1(kbd)/4(COM1) bound (:217-223).
- GAP: legacy PCI INTA#->IRQ routing (PCI config 0x3C interrupt line/pin) is NOT wired. e1000
  supports INTx and MSI/MSI-X; INTx uses this line, MSI-X needs APIC (not set up). Start with INTx.

## 4. Device-driver templates & init
- qemu_ata.cpp (PIO, polled), qemu_ahci.cpp (BAR5 + MEM|bus-master at :248-254), qemu_xhci.cpp
  (BAR0 + MEM|BM at :138-142), qemu_usb.cpp / qemu_usb_ohci.cpp — all **polled**, no IRQ wiring.
  Good polled-bring-up base; BAR mapping pattern is reusable for e1000 MMIO.
- MMU is identity-mapped 0-128 MiB (r1_mmu_design_v1.md:10-11), so virtual==physical for DMA
  buffers. ckernel.cia is pure asm thread/exception glue — no device probe. Probing is driven by
  `Asic::Init1/Init3` (qemu_variant_asic.cpp) + shell commands (`pci`,`ata`,`usb`; README §14).
- Makefile: variant/*.cpp -> build/*.o -> KERNFULL_OBJS (Makefile:220-273). Add qemu_nic.o there
  plus a `nic` shell command.

## 5. Concrete next steps (N-01 = D-06 NIC)
1. Add explicit `-netdev user,id=n0 -device e1000,netdev=n0` to r1_qemu_smoke.py for a
   deterministic guest-visible NIC with host backend.
2. New `variant/qemu_nic.cpp`: `PciFindClass(0x02,0x00)`, read vendor/device + BAR0 (MMIO),
   set command reg MEM|bus-master; claim the NIC (never edit kernelhwsrv/).
3. e1000 init: MMIO BAR, device reset, MAC address, RX/TX descriptor rings (DMA buffers in
   identity-mapped RAM), enable receiver. Add `nic` shell command for probe/status.
4. Polled bring-up first: TX/RX a single crafted ARP frame (matches existing polled ATA/AHCI
   style), prove link/packet flow before interrupts.
5. Then wire INTx: read PCI 0x3C interrupt line, `Interrupt::Bind(irq, NicIsr, ctx)`, `Enable`;
   ISR acknowledges e1000 interrupt cause and posts a DFC for RX processing.
6. Above NIC: ARP/IPv4/ICMP echo (N-02), UDP/TCP + ESOCK-like socket server (N-03), apps +
   QEMU packet regression tests (N-04). Keep kernelhwsrv/ frozen; gate each step QEMU-ready.
