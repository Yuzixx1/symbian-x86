// qemu_variant.h — shared QEMU-variant declarations for the full Kern image.
//
// Genuine struct layouts (<e32rom.h>); everything else is our variant.

#ifndef __QEMU_VARIANT_H__
#define __QEMU_VARIANT_H__

#include <e32rom.h>

// Contiguous ROM synth blob (variant/qemu_variant_rom.cpp). Lives at
// qemu_rom_header_reserve so M::IsRomAddress() covers every image header.
struct QRomBlob
    {
    TRomHeader hdr;
    TRomEntry varEntry;
    TRomImageHeader varImg;
    TUint32 varExportDir[2];
    TRomEntry extEntry;      // dummy extension: product ROMs always have
    TRomImageHeader extImg;  // >=1, and zero extensions panic (RArray(0))
    TRomEntry kernEntry;
    TRomImageHeader kernImg;
    TRomRootDirectoryList rootList;  // iRootDir[1]
    TRomDir rootDir;                 // empty but valid (storage phase fills)
    };

// Identity-mapped RAM geometry (QEMU -m 128, link base 0x100000).
static const unsigned long kQemuKernDataAddress = 0x1000000ul;
static const unsigned long kQemuKernRegionSize  = 0x200000ul;
static const unsigned long kQemuUserDataAddress = 0x2000000ul;
static const unsigned long kQemuTotalRamSize    = 128ul * 1024ul * 1024ul;
static const int kQemuInitialHeapSize = 0x40000;

extern "C"
    {
    // variant/qemu_variant_rom.cpp
    void QemuRomInit();
    // variant/qemu_variant_asic.cpp
    TInt QemuVariantEntry(TInt aReason);
    TAny* QemuVariantInitialise(TInt aMode);
    // variant/qemu_variant_int.cpp (plain-C dispatcher, ISR-driven)
    void X86IrqDispatcher();
    // Binds the PIT tick + PS/2 ISRs. Called from Asic::Init1.
    void QemuIrqBindTickKbd();
    // PS/2 keyboard re-arm (variant/qemu_variant_int.cpp; idempotent).
    // Mirrors the mouse driver's shell-time re-arm: re-bind IRQ1, flush the
    // 8042 output buffer (so a stale byte cannot hold the edge-triggered IRQ1
    // line high), force config bit0 + 0xAE, then unmask IRQ1 via read-modify-
    // write so no other IRQ is clobbered. Called from Init3 and `kbd arm`.
    void QemuKbdArm();
    // Live PS/2 delivery counters + raw PIC/8042 state (diagnostic).
    void QemuKbdStats(TUint& aIsrCount, TUint& aScancodes, TUint& aKeys,
                      TUint& aPicMask, TUint& aKbdStatus);
    // RTC seconds since 2000 (variant/qemu_variant_asic.cpp); -1 if bad.
    TInt QemuRtcSeconds();
    // Set RTC from seconds since 2000; negative rejected. Returns Symbian error.
    TInt QemuSetRtcSeconds(TInt aSeconds);
    }

// PCI bus scan (variant/qemu_pci.cpp).
void PciList();
unsigned int PciRead32(unsigned int aBus, unsigned int aDev,
                       unsigned int aFn, unsigned int aReg);
void PciWrite32(unsigned int aBus, unsigned int aDev,
                unsigned int aFn, unsigned int aReg, unsigned int aVal);
TBool PciFindClass(unsigned int aClass, unsigned int aSub,
                   unsigned int* aBus, unsigned int* aDev, unsigned int* aFn);

// VBE linear framebuffer + display HAL (variant/qemu_display.cpp).
TInt display_init();                 // idempotent; boot-time VBE bookkeeping + DISPLAY: markers
                                     // (the shell parks the hardware in text mode 3 afterwards)
void display_ensure_graphics();      // canonical lazy VBE 0x118 activation; safe/idempotent
                                     // from any kernel exec handler or the shell launch path
void display_info();                 // print mode / LFB base / geometry
void display_clear(TUint aColor);    // 0x00RRGGBB
void display_rect(TUint aX, TUint aY, TUint aW, TUint aH, TUint aColor);
void display_putc(TUint aX, TUint aY, char aCh, TUint aFg, TUint aBg);
// Scaled stroke-font cell geometry, shared by display_putc() and the VBE
// teletype mirror.  Compile-time constants on purpose: the display boot
// constructor calls KPrintf -> VgaTeletype before dynamic initializers run.
#define QEMU_DISPLAY_FONT_SCALE 2u
#define QEMU_DISPLAY_CELL_W (8u * QEMU_DISPLAY_FONT_SCALE)        // 16
#define QEMU_DISPLAY_CELL_H (8u * QEMU_DISPLAY_FONT_SCALE + 2u)   // 18
TInt display_test();                 // clear + border + "EKA2 GUI READY" + markers
void QemuDisplayRegisterHal();       // register EHalGroupDisplay in Asic::Init3
TUint32 display_lfb();               // framebuffer physical base (u32)
TUint display_fb_size();             // framebuffer byte size
void display_geometry(TUint& aW, TUint& aH, TUint& aBpp);
TUint display_getpixel(TUint aX, TUint aY);  // 0x00RRGGBB readback
void display_mode_list();            // list supported VBE modes + current mode
TInt display_set_mode(TInt aVbeMode); // reprogram Bochs DISPI to another mode;
                                     // KErrNone on success, old mode kept on error
// Mode-switch notifications: modules that cache framebuffer geometry/cursor
// state must refresh from the updated display_* globals (defined in
// kern_shell.cpp / qemu_gui_exec.cpp / qemu_mouse.cpp).
void FbMirrorReset();                // reset the VBE text-mirror cursor/grid
// Mute/unmute the console -> LFB text mirror. While muted, KPrintf/DebugPrint
// output still goes to COM1 and 0xB8000 but never touches the framebuffer, so
// a full-screen GUI app's pixels cannot be scrolled away by console text.
void QemuFbMirrorSet(int aOn);       // aOn != 0 = suspend LFB mirror
void MouseGeometryReset();           // clamp/refresh the PS/2 cursor


// PS/2 auxiliary (mouse) driver (variant/qemu_mouse.cpp). IRQ12 ISR updates a
// clamped software cursor; the crosshair is painted through display_rect().
TInt mouse_init();                   // 8042 init + IRQ12 bind/unmask; idempotent
void mouse_info();                   // print x/y/buttons/packets/id/pktsize
void mouse_render_cursor();          // draw/move the crosshair on the framebuffer
void mouse_poll();                   // polled fallback: drain 8042 output once
TInt mouse_test();                   // inject one synthetic packet; verify ISR path
TInt mouse_inject_buttons(TUint aMask); // inject synthetic packet with button bits
                                    // (1=L,2=R,4=M) to drive ISR->shared path

// e1000 (82540EM) polled Ethernet driver (variant/qemu_nic.cpp).
TInt e1000_init();                                  // boot/hot init; idempotent
TInt e1000_reset();                                 // MAC reset + force link up
void e1000_read_mac(TUint8* aMac);                  // EEPROM words 0..2
TInt e1000_init_tx();                               // legacy TX descriptor ring
TInt e1000_init_rx();                               // legacy RX descriptor ring
TInt e1000_tx(const TUint8* aBuf, TUint aLen);      // transmit one frame (ARP at bring-up)
TInt e1000_send(const TUint8* aBuf, TUint aLen);    // alias of e1000_tx
TInt e1000_recv(TUint8* aBuf, TUint aMax);          // bytes read, or 0
TBool e1000_link_up();
TInt e1000_irq();                                   // poll stub: ACK ICR, return bits

// virtio-net (PCI 1AF4:1000/1041) alternative NIC backend
// (variant/qemu_virtio_net.cpp). Polled only; boot hook via constructor.
TInt vnet_init();                                  // boot/hot init; idempotent
TInt vnet_tx(const TUint8* aBuf, TUint aLen);      // enqueue + kick one TX frame
TInt vnet_recv(TUint8* aBuf, TUint aMax);          // non-blocking; bytes or 0
void vnet_info();                                  // print MAC, queues, features

// virtio-blk (PCI 1AF4:1001/1042, subsystem 2) polled block storage driver
// (variant/qemu_virtio_blk.cpp). NEW storage backend alongside ATA/AHCI.
// One split virtqueue; polled only; boot hook via constructor.
TInt vblk_init();                                  // boot/hot init; idempotent
TInt vblk_read(TUint32 aLba, TUint8* aBuf, TUint aCount);        // count sectors
TInt vblk_write(TUint32 aLba, const TUint8* aBuf, TUint aCount); // count sectors
void vblk_info();                                  // print capacity/features/queue

// virtio-RNG (PCI 1AF4:1005/1044, subsystem 4) polled entropy source
// (variant/qemu_virtio_rng.cpp). NEW. One split virtqueue; polled only;
// boot hook via constructor.
TInt vrng_init();                                  // boot/hot init; idempotent
TInt vrng_get(TUint8* aBuf, TUint aLen);           // bytes of random data received
void vrng_info();                                  // print PCI/features/queue state

// Kernel<->userland e1000 frame bridge (variant/qemu_netbridge.cpp). Shared
// struct nb_shared is described in user/netbridge.h. Base/size are discoverable
// from userland via slow execs 247/248.
struct nb_ring;
TInt netbridge_init();                                  // boot init (idempotent)
TInt netbridge_poll();                                  // one TX-drain / RX-fill pass
void NetBridgePollLoop(TInt aMaxMs);                    // shell-driven poll loop
struct nb_ring* netbridge_tx_ring();
struct nb_ring* netbridge_rx_ring();
TUint32 netbridge_base();
TUint netbridge_size();

// AC'97 PCM-OUT driver (variant/qemu_audio.cpp). Polled, no IRQ wiring.
TInt audio_init();                                  // boot/hot init; idempotent
void audio_info();                                  // BARS, vendor ID, codec status
TInt audio_play_test();                             // play a 1kHz test tone
TInt audio_tone(TInt aFreq, TInt aMs);              // play one note (freq Hz, ms)

// Unified AC'97 queue/pool (variant/qemu_audio.cpp). 8 x 16 KiB low-RAM slots
// feed the SAME multi-descriptor DMA engine as audio_pcm_play(); there is only
// one BDL and one completion poller. Drop-on-saturation (KErrInUse) is counted
// in stats. Small buffers (<= 16 KiB) are copied into a slot so the caller may
// reuse its buffer immediately; oversized ones go straight to the engine.
TInt audio_pool_init();                             // idempotent boot/hot init
TInt audio_pool_poll();                             // reclaim done, kick next
void audio_pcm_stats(TUint& aPlayed, TUint& aDropped, TUint& aBusy,
                     TUint& aCompleted);
TUint audio_pcm_busy();
void audio_pool_info();                             // print "AU: stats ..."

// --- AC'97 PCM sample playback (async, multi-descriptor BDL) ---------------
// THE canonical play API (the only audio_pcm_play overload). It copies/converts
// a user PCM buffer into the one internal 16-bit stereo DMA buffer, arms the
// one PCM-OUT BDL and returns IMMEDIATELY (async). Poll audio_pcm_status() for
// consumption / completion; audio_pcm_stop() halts and flushes the queue.
//   aBuf      : linear address of the source bytes (low, identity-mapped RAM).
//   aBytes    : source length in bytes.
//   aRate     : 11025 / 22050 / 44100 / 48000 Hz (front DAC is set to match).
//   aChannels : packed format -- low 16 bits = channel count (1 mono, 2 stereo),
//               high 16 bits = bits/sample (8 unsigned or 16 signed, 0 => 16).
// Supported inputs: 8-bit unsigned / 16-bit signed, mono / stereo.  Output is
// always 16-bit signed stereo (mono is duplicated to both channels).
TInt audio_pcm_play(const void* aBuf, TUint aBytes, TUint aRate, TUint aChannels);
TInt audio_pcm_selftest();                          // 1 s 440 Hz 16-bit stereo selftest

// Status/control transport shared with ring 3 (see qemu_audio_exec.cpp).
struct au_pcm_req
    {
    TUint32 iBuf;      // linear address of user PCM data (low RAM)
    TUint32 iBytes;    // source length in bytes
    TUint32 iRate;     // sample rate in Hz
    TUint32 iFormat;   // (bits<<16) | channels
    };
struct au_pcm_stat
    {
    TUint32 iState;    // 0 = idle, 1 = DMA busy
    TUint32 iPosition; // bytes consumed so far
    TUint32 iTotal;    // total bytes submitted
    };
struct au_pcm_stats
    {
    TUint32 iPlayed;    // buffers armed on the DMA engine
    TUint32 iDropped;   // rejected on queue saturation
    TUint32 iBusy;      // queued + in-flight slots
    TUint32 iCompleted; // buffers reclaimed after DMA completion
    };
TInt audio_pcm_status(au_pcm_stat* aStat);          // KErrNone fills *aStat
TInt audio_pcm_stop();                              // halt PCM-OUT DMA, flush queue

// Slow exec 240 op selectors: the high bit of a1 selects the new PCM ops;
// legacy calls (a1 = Hz < 0x80000000) keep driving audio_tone() unchanged.
// a1 = AU_PCM_PLAY     -> a2 = linear address of struct au_pcm_req
// a1 = AU_PCM_STATUS   -> a2 = linear address of struct au_pcm_stat
// a1 = AU_PCM_STOP     -> a2 ignored
// a1 = AU_PCM_SELFTEST -> a2 ignored (kernel-generated 440 Hz buffer)
// a1 = AU_PCM_STATS    -> a2 = linear address of struct au_pcm_stats
#define AU_PCM_PLAY     ((TInt)0x80000001u)
#define AU_PCM_STATUS   ((TInt)0x80000002u)
#define AU_PCM_STOP     ((TInt)0x80000003u)
#define AU_PCM_SELFTEST ((TInt)0x80000004u)
#define AU_PCM_STATS    ((TInt)0x80000005u)

// Process-viewer census exec (variant/qemu_ps_exec.cpp). Slow exec slot 241:
// op 0 -> identity-mapped ps_census base (user/pshost.h), op 1 -> refresh.
TUint PsExecSlot();                                 // == 241, for diagnostics


// --- Genuine EWSRV raw-input bridge (variant/qemu_variant_int.cpp) ----------
// Opt-in via \KBINJ.TXT on the boot FAT volume (presence = enable, same idea
// as \PTRINJ.TXT for the one-shot boot pointer bridge in qemu_display.cpp).
// When ON, the PS/2 ISRs (IRQ1 keyboard here, IRQ12 mouse in qemu_mouse.cpp)
// turn each scancode/packet into a genuine TRawEvent (EKeyDown/EKeyUp carrying
// a TStdScanCode from <e32keys.h>, EPointerMove/EButtonNDown/Up carrying a
// position) and queue it in a small ISR ring; a TDfc on Kern::DfcQue0() drains
// the ring and feeds every event to the EKA2 kernel event queue through the
// public Kern::AddEvent API under NKern::ThreadEnterCS — exactly where a
// genuine keyboard/pointer driver's DFC puts it (cf. the bsptemplate
// keyboard_interrupt.cpp EventDfc).  Kern::AddEvent is never called from ISR
// context (its documented preconditions forbid it: thread context, CS held,
// kernel unlocked, interrupts enabled); the ISR only fills the ring and queues
// the DFC.  Genuine EWSRV, as the kernel event-hook owner (K::EventThread),
// then consumes the events in CEvent::RunL (ws_main.cpp: pending
// UserSvr::RequestEvent -> EKeyDown/EKeyUp -> ScanCode()/TranslateKey ->
// CWsWindow::KeyPress).  OFF (default, no \KBINJ.TXT) leaves every boot on the
// old ws-ring path, byte-for-byte.
void QemuKbdInjArm();        // resolve gate + DFC queue; called from QemuKbdArm()
TBool QemuInjGateActive();   // any injection enabled (\KBINJ.TXT present)
// Gate mask: bit0 (1) = pointer ISR events, bit1 (2) = keyboard ISR events.
// \KBINJ.TXT content selects: "ptr" -> 1, "key" -> 2, anything else -> 3 (both).
TUint QemuInjGateMask();
void QemuEwsrvPushKey(TBool aDown, TUint aStdScanCode);          // ISR-safe
void QemuEwsrvPushPointerMove(TInt aX, TInt aY);                 // ISR-safe
// aPs2ButtonBit: 1=left, 2=right, 4=middle (PS/2 packet flag bits).
void QemuEwsrvPushButton(TUint aPs2ButtonBit, TBool aDown, TInt aX, TInt aY);
TUint QemuInjStats(TUint& aQueued, TUint& aDelivered, TUint& aDropped);

// ATA PIO driver (variant/qemu_ata.cpp). Devices 0..3 = pri-master,
// pri-slave, sec-master, sec-slave. LBA28, polled.
TInt AtaInit();
TInt AtaCount();
TBool AtaPresent(int aDev);
TUint32 AtaSectors(int aDev);
const char* AtaModel(int aDev);
TInt AtaRead(int aDev, TUint32 aLba, TUint aCount, TUint8* aBuf);
TInt AtaWrite(int aDev, TUint32 aLba, TUint aCount, const TUint8* aBuf);
TInt AtaFlush(int aDev); // synchronous correct-device cache flush; errors propagate

// GRUB Multiboot module -> RAM-backed read-only block device (dev 8).
// QemuRamdiskInit snapshots the module into a safe high-RAM buffer during
// HwInit0 (before the kernel data clear and the user-page allocator claim it).
void QemuRamdiskInit(TUint aModStart, TUint aModEnd);
TBool QemuRamdiskPresent();
TUint32 QemuRamdiskSectors();

// AHCI SATA driver (variant/qemu_ahci.cpp). Ports present as Fat devices
// 4..7 behind the Ata* dispatch below. DMA, polled.
TInt AhciInit();
TInt AhciCount();
TBool AhciPresent(int aDev);   // 0..3 = AHCI ports 0..3
TUint32 AhciSectors(int aDev);
const char* AhciModel(int aDev);
TInt AhciRead(int aDev, TUint32 aLba, TUint aCount, TUint8* aBuf);
TInt AhciWrite(int aDev, TUint32 aLba, TUint aCount, const TUint8* aBuf);
TInt AhciFlush(int aDev); // AHCI-local port index 0..3
void AhciInfo();

// xHCI USB3, polled command/event/transfer rings and one direct-root-port
// HID boot keyboard. No hubs, hotplug, multiple devices or mouse yet.
TInt XhciInit();
void XhciInfo();
TInt XhciStart();
TInt XhciNoOp();
TInt XhciPortReset(int aPort); // zero-based root port
TInt XhciEnableSlot(int aPort);
TInt XhciAddress(int aPort);
TInt XhciGetDescriptor(int aPort, TUint aType, TUint aLength);
TInt XhciKeyboardInit(int aPort);
void XhciKeyboardPoll();
TBool XhciKeyboardReady();
TInt XhciAutoKeyboard();
void XhciKeyboardInfo();
void XhciPorts();

// FAT32 on ATA (variant/fat32.cpp). Up to 8 mounted volumes (C:, D:, ...)
// across drives/partitions, each with its own CWD. Mount, list/read/write.
TInt FatMount(int aDev, int aPart);   // aPart<0: first FAT32 partition
TInt FatMountAlias(int aDev, int aPart, char aLetter);  // extra letter for a mounted dev/part
TInt UserFilesInit();
TInt UserFilesChannelCount();
TInt UserConInit();          // transitional console-input service
TBool ConInputLeased();      // a user program owns the input stream
TBool ConOwnerAlive();       // lease owner still has a live thread
void ConForceRelease();      // reclaim a dead owner's lease
int ConPollChar();           // shared stream; caller must hold the lease
class DProcess;
TBool ProcExtAvailable(); // caller serializes process launch/reap
void ProcExtCapture(DProcess* aProc);
void ProcExtRemove(DProcess* aProc);
void UserFilesLock();
void UserFilesUnlock();
TInt FatParts(int aDev);              // list partitions, returns FAT32 count
char FatCurLetter();                  // active volume letter, 0 if none
TInt FatUse(char aLetter);            // activate a mounted volume
void FatList();                       // list mounted volumes (* = active)
void FatPrompt();                     // "C:/path> " or "EKA2> "
TInt FatDir();
TInt FatDirWide();                  // compact DOS-style /W listing
TInt FatDirPath(const char* aPath, TBool aWide); // temporary directory view
TInt FatCdPath(const char* aPath);     // persistent directory change
TInt FatTypePath(const char* aPath);
TInt FatReadPath(const char* aPath, TUint8* aBuf, TUint aMax, TUint& aOutLen);
TInt FatWritePath(const char* aPath, const TUint8* aData, TUint aLen);
TInt FatExistsPath(const char* aPath);
TInt FatDelPath(const char* aPath);
TInt FatMkdirPath(const char* aPath);
TInt FatRenamePath(const char* aOld, const char* aNew);
TInt FatType(const char* aName);
TInt FatRead(const char* aName, TUint8* aBuf, TUint aMax, TUint& aOutLen);
TInt FatExists(const char* aName); // KErrNone if file, KErrAlreadyExists if dir
TInt FatWrite(const char* aName, const TUint8* aData, TUint aLen);
TInt FatRename(const char* aOld, const char* aNew); // same-dir 8.3 metadata rename
TInt FatDel(const char* aName);
TInt FatMkdir(const char* aName);
TInt FatCd(const char* aName);        // ".." = up, NULL/empty = root
TInt FatFormat(int aDev);             // whole-disk single FAT32 partition
void FatGetCwdPath(char* aOut, TUint aMax);  // active volume CWD: "\" or "\A\B"

// USB HID on UHCI (variant/qemu_usb.cpp). Polled; `usb` enumerates.
TInt UsbInit();
void UsbPoll();   // feed USB keyboards into the shell input ring
void UsbList();   // list UHCI devices
void UsbDebugPoll();  // one-shot interrupt-IN dump (bring-up debug)

// OHCI backend (variant/qemu_usb_ohci.cpp). Same polled discipline.
TInt OhciPresent();
TInt OhciInit();
int OhciPorts();
TUint32 OhciPortStatus(int aPort);   // raw HcRHPortStatus; 0xFFFFFFFF = invalid
void OhciInfo();                     // revision / BAR / ports / per-port status
TInt OhciPortReset(int aPort, TBool& aLow);
TInt OhciControl(TUint8 aAddr, TUint8 aMaxPkt, TBool aLow,
                 TUint8 aReqType, TUint8 aReq, TUint16 aValue, TUint16 aIndex,
                 TUint16 aLen, TUint8* aData, TBool aIn);
TInt OhciIntPoll(TUint8 aAddr, TUint8 aEp, TUint8& aToggle,
                 TUint8* aDst, TUint aLen, TBool aLow, TUint8 aMaxPkt);

// EHCI USB 2.0 host controller (variant/qemu_usb_ehci.cpp). Polled only;
// boot hook via constructor. Complements the OHCI 1.1 backend above.
TInt ehci_init();                       // idempotent; BAR0 MMIO + schedule setup
int ehci_ports();                       // number of root ports (0 if absent)
TUint32 ehci_port_status(int aPort);    // raw PORTSC; 0xFFFFFFFF = invalid port
TInt ehci_port_reset(int aPort);        // zero-based root port reset + enable
void ehci_info();                       // version / BAR / ports / per-port status

// E32 image validation (variant/e32load.cpp). Stage 3a: validate + report.
struct TE32Info
    {
    TUint32 iUid1, iUid2, iUid3;
    TUint32 iEntry;
    TUint32 iCodeBase, iDataBase;
    TInt iCodeSize, iDataSize, iBssSize;
    TInt iHeapMin, iHeapMax, iStackSize;
    TInt iDllRefs;
    TUint32 iCodeOff, iDataOff;  // file offsets for the loader copy step
    TUint16 iPriority;           // header process priority (may be mapped)
    TUint32 iCodeRelocOff;       // file offset of code reloc section (0=none)
    TUint32 iDataRelocOff;       // file offset of data reloc section (0=none)
    TUint32 iImpOff;             // file offset of import section (0=none)
    TInt iImpN;                  // import count (0, or up to 512 PE2 ordinals)
    TBool iPe2;                  // PE2 import format (ordinals stored in IAT)
    TUint32 iSlots[512];           // code offsets of the IAT slots
    TUint32 iExpOff;             // file offset of export directory (0=none)
    TInt iExpCount;              // export count (DLLs only)
    };
TInt E32Validate(const TUint8* aBuf, TUint aLen, TE32Info& aInfo);
TInt E32Run(const TUint8* aFileBuf, TUint aFileLen, const char* aName,
            const char* aCmdLine = 0);  // shell tail, stored as UTF-16 bytes
TInt E32Kill(TUint aId);     // die by container id (never the kernel)
TUint E32LastPid();          // pid of the most recent successful E32Run (Ctrl+C abort)
TInt E32Reap();              // post-mortem code-seg cleanup, returns count
void E32PrintExits();        // ring of recent exit records (name/id/type/reason)
TBool E32FindExit(TUint aId, TUint8& aType, TInt& aReason, char* aName);
// S-02 port traps: exec-81 FsRegisterThread wrapper + slim ctl exec slot 270
// (ops 2/3/4: fbblit + PTRINJ). svcmgr launch/kill/boot execs retired.
void PortTrapExecInstall();  // wrap exec 81; install exec 270 ops 2/3/4 only
TBool E32ProcessAlive(TUint aId);  // still in the process container?
TBool E32ProcessDead(TUint aId);   // reaper predicate: husks count as dead
void VgaTeletype(char aC);
void ShellSpawnFull();

// Genuine pointer-event bridge (variant/qemu_display.cpp).  Opt-in: reads two
// decimal coords from \PTRINJ.TXT on the boot FAT volume, builds a genuine
// TRawEvent(EPointerMove) and feeds it to the EKA2 kernel event queue via the
// public Kern::AddEvent API.  This is the only port-side source of raw events
// for the genuine EWSRV, so its UserSvr::RequestEvent -> CEvent::RunL ->
// EUSER ord 1868 TRawEvent::Pos path actually executes under QEMU.  No-op when
// the marker file is absent (all other tests boot unchanged).
void GenuinePointerBridge();

// Bulk framebuffer blit exec (variant/qemu_fbblit.cpp).  Slots 274/275 would
// overlap EpocFastExecTable, so blit/info are ops 3/2 of genuine slow exec 270.
TUint display_pitch();                       // bytes per scanline (derived from geometry)
TInt fbblit_info_exec(TInt aArg);            // exec 270 op 2: struct fbblit_info out
TInt fbblit_blit_exec(TInt aArg);            // exec 270 op 3: struct fbblit_req in
TInt FbBlitInit();                           // idempotent boot hook

// Genuine G launch integration; separate from legacy E32Run/reaper ownership.
void E32EnsureUserTss();
TInt R1GCommand(const char* mode);

#endif // __QEMU_VARIANT_H__
