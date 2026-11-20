# R1 Audio design v1 — findings (read-only)
Read-only; only this doc created. kernelhwsrv/ untouched; no make/QEMU run.
- **QEMU HW**: smoke cmd `tools/r1_qemu_smoke.py:144-156` boots `-M q35 -m 128`, e1000 + ich9-ahci,
  but **no `-audiodev`, no sound device** — no audio today. q35 supports AC97 (`-device AC97`, 04.01),
  Intel HDA (`-device intel-hda` + `hda-duplex`, 04.03), SB16. `qemu_pci.cpp` `PciFindClass(0x04,sub)`
  sees "multimedia" (:71), no consumer. ckernel.cia is pure asm thread/exception glue — no device
  probe; discovery is variant code via Asic::Init1/Init3 + shell.
- **Driver**: NONE in qemu-port (grep finds only HAL config `values.hda`); no soundsc.pdd linked, no
  audio decls in qemu_variant.h. Genuine shared-chunk PDD *stub* in kernelhwsrv/bsptemplate/.../
  template_variant/{specific/soundsc_tx.cpp,soundsc_rx.cpp,inc/soundsc_plat.h}, marked "Delete old
  sound driver" — not functional, not built.
- **Interrupts**: 8259 at 0x30+, 16-slot `Interrupt::Bind/Enable` + `X86IrqDispatcher`
  (qemu_variant_int.cpp:46-145), only IRQ0/1/4 bound (:217-223); PCI INTx (cfg 0x3C) not wired, MSI
  needs APIC (absent). Polled I/O matches ATA/AHCI/NIC/xHCI/USB; identity RAM suits DMA.
- **Next**: (1) add `-audiodev none,id=a0 -device AC97,audiodev=a0` (or intel-hda + hda-duplex) to
  smoke cmd, verify `pci` class 04; (2) new `variant/qemu_audio.cpp`: PciFindClass(0x04,...), BAR +
  MEM|bus-master, AC97 mixer/HDA CORB-RIRB probe, AudioInit/Play/Info in qemu_variant.h; (3) polled
  PCM first; (4) wire INTx -> Bind + ISR ACK + DFC; (5) Makefile rule + qemu_audio.o in KERNFULL_OBJS
  (:220-274) + `audio` shell cmd; (6) optional soundsc PDD port. kernelhwsrv/ frozen.
