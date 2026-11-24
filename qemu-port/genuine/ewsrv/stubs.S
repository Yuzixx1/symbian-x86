/* Minimal port-layer stubs for three symbols the genuine EFSRV client header
   declares but whose implementations live in EFSRV.DLL (not available here).
   EWSRV only touches these on the notifier-plugin scan path, which our
   efsrv_shims.cpp short-circuits with KErrNotFound. */
.section .text
.globl _ZN10TParseBaseC2Ev
.type _ZN10TParseBaseC2Ev, @function
_ZN10TParseBaseC2Ev:
    ret
.size _ZN10TParseBaseC2Ev, .-_ZN10TParseBaseC2Ev

.globl _ZN6TEntryC1Ev
.type _ZN6TEntryC1Ev, @function
_ZN6TEntryC1Ev:
    ret
.size _ZN6TEntryC1Ev, .-_ZN6TEntryC1Ev

.section .data
.globl _ZTV6TParse
_ZTV6TParse:
    .long 0, 0, 0, 0, 0, 0

/* Exec::HeapSwitch(RAllocator*) -- fast exec 2 (arg in ECX, number in EAX). */
.section .text
.globl _ZN4Exec10HeapSwitchEP10RAllocator
.type _ZN4Exec10HeapSwitchEP10RAllocator, @function
_ZN4Exec10HeapSwitchEP10RAllocator:
    mov $2, %eax
    mov 4(%esp), %ecx
    int $0x20
    ret
.size _ZN4Exec10HeapSwitchEP10RAllocator, .-_ZN4Exec10HeapSwitchEP10RAllocator
.section .note.GNU-stack,"",@progbits
