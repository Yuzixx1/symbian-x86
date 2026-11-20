# R1 NIC design v1 — D-06 findings (read-only investigation)

Scope: read-only investigation. `kernelhwsrv/` frozen; no build/QEMU run. Only
this doc is written. The implementation described in §2 was already present in
the checkout from prior work and is NOT modified by this investigation.
(The requested path `qemu-port/kernelhwsrv/.../ckernel.cia` does not exist: the
real file is at workspace root `kernelhwsrv/kernel/eka/kernel/x86/ckernel.cia`.)

## 1. NIC hardware QEMU presents
- Guest boots `qemu-system-i386 -M q35 -m 128 ... -netdev user,id=n0
  -device e1000,netdev=n0` (`tools/r1_qemu_smoke.py:144-156`). Earlier checkout
  relied on QEMU's default NIC; now explicit and deterministic (SLIRP user mode).
- Guest NIC = Intel **82540EM e1000**, PCI class 02.00 ethernet, PCI 8086:100E,
  BAR0 = 32-bit MMIO. `README.md:404` `pci` scan lists "e1000". virtio-net,
  pcnet and rtl8139 appear only if explicitly requested — none are present.
- `variant/qemu_pci.cpp`: `PciRead32/PciWrite32` via 0xCF8/0xCFC,
  `PciFindClass(0x02,0x00,...)`, `PciList()` prints class 02 as "ethernet"/
  "network" (:69). No other consumer of the network class existed before D-06.

## 2. NIC driver code — PRESENT (prior work, already wired)
- `variant/qemu_nic.cpp` (339 lines): polled e1000 driver — PCI vendor/device
  probe (8086:100E), BAR0 MMIO + MEM|bus-master, global reset (CTRL.RST), SLU,
  EERD EEPROM read (words 0..2) for MAC, RAL/RAH, 8-entry legacy TX/RX rings
  (2048-byte buffers, identity-mapped DMA), `e1000_tx/send/recv`,
  `e1000_link_up`, `e1000_irq` (ICR-ACK poll stub), and a
  `__attribute__((constructor))` boot hook.
- `qemu_variant.h:66-76` declares the full `e1000_*` API.
- `Makefile:232-233` builds `build/qemu_nic.o`; `Makefile:274` lists it in
  `KERNFULL_OBJS` (alongside qemu_pci/ata/ahci objects).
- `tools/r1_qemu_smoke.py:148-149` passes `-netdev user,id=n0` and
  `-device e1000,netdev=n0`.
- Boot hook runs via `HwInit0`'s `__init_array` walk
  (`kernfull_glue.cpp:119-120`) after paging + Asic creation.
- Gaps: no `nic` shell command (`kern_shell.cpp` has pci/ata/usb only) and no
  in-tree callers of `e1000_send`/`e1000_recv` — TX/RX is not yet exercised.

## 3. Interrupt infrastructure (usable, one gap)
- i8259 remapped to 0x20/0x28; `Interrupt::Bind/Unbind/Enable/Disable/Clear/
  SetPriority` (`qemu_variant_int.cpp:46-104`); ISR-driven `X86IrqDispatcher`
  reads the in-service register, calls the bound `TIsr`, sends EOI (:110-145).
  Only IRQ0 (tick), IRQ1 (kbd), IRQ4 (COM1) are bound (:217-223).
- GAP: legacy PCI INTA# routing (config 0x3C line/pin) is NOT wired. e1000
  supports INTx and MSI/MSI-X; MSI-X needs APIC (not set up). NIC is polled now;
  `e1000_irq()` is a stub. Next: read 0x3C, `Interrupt::Bind(irq, NicIsr)`,
  enable, ACK ICR in ISR and post a DFC for RX.

## 4. Driver pattern / early init
- qemu_ata/ahci/xhci/usb are all polled: BAR map + MEM|bus-master, no IRQ.
  `qemu_ahci.cpp:248-254` is the BAR5 pattern e1000 mirrors with BAR0.
- Identity map 0..128 MiB => virtual == physical for DMA buffers.
- `ckernel.cia` (genuine, workspace root) is pure asm thread/exception glue —
  no device probing. Probing is variant-owned (`Asic::Init1/3` + shell commands).
  kernelhwsrv/ was not touched.

## 5. QEMU command line (does it pass -netdev?)
- Yes. `r1_qemu_smoke.py:144-156`: `-M q35 -m 128`, `-netdev user,id=n0`,
  `-device e1000,netdev=n0`, `-display none -serial stdio -kernel <elf>`,
  ich9-ahci + ide-hd disk. User-mode SLIRP, no explicit host port forwards.

## 6. Concrete next steps
1. Add a `nic` shell command (probe/MAC/link/status) + a crafted ARP request
   using `e1000_send`/`e1000_recv`; prove polled TX/RX against QEMU SLIRP.
2. Wire INTx: read PCI 0x3C, `Interrupt::Bind`, `Enable`; ISR ACKs ICR and
   posts an RX DFC. Defer MSI-X until APIC init exists.
3. Then stack (N-02..N-04): ARP/IPv4/ICMP, UDP/TCP + `!SocketServer` IPC
   (`docs/r1_net_design_v2.md`), DNS/DHCP (`r1_dns_design_v1.md`), user apps
   and QEMU packet regression tests.
4. Keep kernelhwsrv/ frozen; gate every step on a QEMU smoke run.
