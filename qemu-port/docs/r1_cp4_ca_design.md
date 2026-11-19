# R1 CP4 CActive test fixture — design

## 1. Ordinal table (euseru.def, x86 GCC)

| Symbol | Ordinal | Exported | Used by ca_test |
|---|---|---|---|
| CActiveScheduler::Install(CActiveScheduler*) | 428 | yes | yes |
| CActiveScheduler::Add(CActive*) | 424 | yes | yes (via SetActive) |
| CActiveScheduler::Start() | 426 | yes | yes |
| CActiveScheduler::Stop() | 425 | yes | yes (in RunL) |
| CActiveScheduler::CActiveScheduler() | 430/431 | yes | yes |
| CActiveScheduler::WaitForAnyRequest() | 423 | yes | (base, overridden by real) |
| CActive::CActive(int) | 1091 | yes | yes |
| CActive::SetActive() | 1090 | yes | yes |
| CActive::Cancel() | 1088 | yes | yes (dtor) |
| CActive::Deque() | 1087 | yes | (Cancel path) |
| RDebug::Printf(char const*,...) | 915 | yes | yes (markers) |
| User::WaitForAnyRequest() | 626 | yes | (wait primitive) |
| memcpy | 1953 | yes | (optional) |
| CTimer::CTimer(int) / After / ConstructL | 887/885/881 | yes | (async completion) |
| CActiveScheduler vtable | 1932 | yes (data) | n/a (compiler emits) |
| CActive vtable | 1940 | yes (data) | n/a (compiler emits) |

All 8 ordinals in the task list (428,424,426,425,1091,1090,1088,1953) are present in euseru.def. No ordinal is missing.

## 2. Fixture C++ pseudocode

```cpp
#include <e32base.h>
#include <e32debug.h>

class MyActive : public CActive {
public:
    MyActive() : CActive(EPriorityStandard) {}
    void RunL() {
        RDebug::Printf("ca_test: RunL() reached");
        CActiveScheduler::Stop();
        RDebug::Printf("ca_test: Stop() called");
    }
    void DoCancel() {}
};

int main() {
    CActiveScheduler sched;            // ord 430
    CActiveScheduler::Install(&sched); // ord 428
    RDebug::Printf("ca_test: CActiveScheduler::Install ok");
    MyActive a;                        // ord 1091 (base ctor)
    a.SetActive();                    // ord 1090 -> enqueued via Add(424)
    // async completion: CTimer or 2nd thread completes a.iStatus
    RDebug::Printf("ca_test: Start() entered");
    CActiveScheduler::Start();         // ord 426 -> blocks in WaitForAnyRequest
    RDebug::Printf("ca_test: Start() returned");
    return 0;
}
```

Vtable dispatch proof: MyActive overrides RunL(); the genuine CActive ctor (ord 1091) stores the vptr, so scheduler's (*it)->RunL() lands in fixture code.

## 3. QEMU marker list (tools/r1_cp4_ca_test.py)

- "e32run: bind slot ..." for each ordinal (8+)
- "ca_test: CActiveScheduler::Install ok"
- "ca_test: Start() entered"
- "ca_test: RunL() reached"
- "ca_test: Stop() called"
- "ca_test: Start() returned"
- "ca_test: clean exit" (no Exc 06 / KERN-HEAP)

Feed: `mount\n` then `ca_test\n` (mirror r1_qemu_smoke.py).

## 4. Build commands

```sh
# compile (freestanding C++, vtables need real compiler)
g++ -m32 -march=i486 -Os -ffreestanding -fno-pic -fno-pie -fno-exceptions \
    -fno-rtti -fno-stack-protector -c user/ca_test.cpp -o build/user/ca_test.o
ld -m elf_i386 --emit-relocs -T user/link.ld -o build/user/ca_test.elf \
    build/user/start.o build/user/runtime.o build/user/ca_test.o
# pack to PE2 E32 with import ordinals 428,424,426,425,1091,1090,1088,1953,915
python3 tools/mke32_ca_test.py build/user/ca_test.elf build/user/CA_TEST.E32
```

mke32_ca_test.py: PE2 import section, IAT at textSize+4*k holding 1-based ordinals; name "euser{...}.dll".

## 5. Blockers

1. **Loader PE2 import cap = 4 slots** (variant/e32load.cpp `n > 4`, iSlots[4] in qemu_variant.h). ca_test needs >=8 (or 9 with RDebug::Printf). Requires raising cap + enlarging iSlots[] in qemu-port (not kernelhwsrv).
2. **User-mode wait primitive** for CActiveScheduler::WaitForAnyRequest() (plan §5): no request semaphore exposed to EUSER; Start() may spin/block forever without a completion source.
3. **Async completion source** (2nd thread or CTimer) needed so RunL fires after Start(); single-threaded fixture would never reach RunL.
4. **Flat relocs**: must avoid stale absolute addresses (Exc 06) like portfilesrv (plan §6).
5. Ordinal blockers: none — all needed symbols exported in euseru.def.

No source files modified; build not run.