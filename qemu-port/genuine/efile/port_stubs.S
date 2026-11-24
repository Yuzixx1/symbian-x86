/* Port-layer exec stub: Exec::HeapSwitch(RAllocator*) -- fast exec 2.
   Genuine EUSER.DLL does not export this; the port kernel supplies exec 2. */
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
