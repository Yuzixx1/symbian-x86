// qemu_assp.h — minimal QEMU ASSP interface (Phase 3).
// Mirrors the genuine contract in kernelhwsrv/kernel/eka/include/kernel/x86/assp.h:
//   class Asic { StartupReason/Init1/Init3/DebugOutput/Idle/MsTickPeriod/
//                SystemTimeInSecondsFrom2000/NanoWaitCalibration/VariantHal/
//                MachineConfiguration } + class Interrupt { Bind/Unbind/
//                Enable/Disable/Clear/SetPriority }.
// Full genuine headers can't compile yet (need bldmake-generated plat_priv.h),
// so this header defines the SAME method names on QemuAsic using only
// freestanding types. Phase 4 swaps the shim for the genuine assp.h.
#ifndef __QEMU_ASSP_H__
#define __QEMU_ASSP_H__

typedef unsigned int TUint;
typedef int TInt;
typedef unsigned char TUint8;

class QemuAsic
    {
public:
    // genuine Asic::StartupReason — QEMU has no reset-cause register: cold.
    static TInt StartupReason();
    // genuine Asic::Init1 — PIC remap + PIT setup + timer calibration report.
    static void Init1();
    // genuine Asic::Init3 — late init (nothing QEMU-specific yet).
    static void Init3();
    // genuine Asic::DebugOutput — single char to COM1.
    static void DebugOutput(TUint aChar);
    // genuine Asic::Idle — hlt with interrupts enabled.
    static void Idle();
    // genuine Asic::MsTickPeriod — NTimerQ::Init1 source (cf. cinit.cpp A::Init1).
    static TInt MsTickPeriod();
    // genuine Asic::NanoWaitCalibration — busy-loop calibration stub.
    static TUint NanoWaitCalibration();
    };

// genuine Interrupt::Enable/Disable — QEMU i8259 mask operations.
namespace QemuIrq
    {
    void RemapPic(TUint8 aMasterBase, TUint8 aSlaveBase);
    void Enable(TInt aIrq);
    void Disable(TInt aIrq);
    }

// COM1 + PIT primitives used by Init1 (cf. variant UART/timer drivers).
namespace QemuUart
    {
    void Init();
    void PutC(char aC);
    void Puts(const char* aS);
    void PutHex(TUint aV);
    void EnableRxIrq();  // RDA interrupt + OUT2 gate (variant ISR drains RBR)
    }
namespace QemuPit
    {
    void Setup(TUint aHz);
    TUint LatchCount();      // latched channel-0 down-counter (polling proof)
    TUint WaitTicks(TUint aN); // busy-wait ~aN ms using latch delta, returns measured
    }

#endif
