.section .text.start,"ax"
.global _start
_start:
    call app_main
    push %eax
    call process_exit
    /* process_exit must never return; if it does, spin rather than fault. */
    jmp .

.section .text,"ax"
.global ipc_wait_any
ipc_wait_any:
    xor %eax, %eax
    int $0x20
    ret

.global exec0
exec0:
    /* No-argument executive (e.g. Math::Random): number in EAX only. */
    push %ebx
    push %esi
    push %edi
    push %ebp
    mov 20(%esp), %eax
    int $0x21
    pop %ebp
    pop %edi
    pop %esi
    pop %ebx
    ret

.global exec2
exec2:
    /* Preserve all cdecl callee-saved registers across executive entry. */
    push %ebx
    push %esi
    push %edi
    push %ebp
    mov 20(%esp), %eax
    mov 24(%esp), %ecx
    mov 28(%esp), %edx
    int $0x21
    pop %ebp
    pop %edi
    pop %esi
    pop %ebx
    ret

.global process_exit
.global exec4
exec4:
    push %ebx
    push %esi
    push %edi
    push %ebp
    mov 20(%esp), %eax
    mov 24(%esp), %ecx
    mov 28(%esp), %edx
    mov 32(%esp), %ebx
    mov 36(%esp), %esi
    int $0x21
    pop %ebp
    pop %edi
    pop %esi
    pop %ebx
    ret

process_exit:
    mov 4(%esp), %ebx
    mov $0xffff8001, %ecx
    xor %edx, %edx
    xor %esi, %esi
    mov $115, %eax
    int $0x21
    /* exec 115 kills this thread. If a kernel ever returns from it, spin
       forever instead of falling through into garbage / raising Exc 06. */
    jmp .
.section .note.GNU-stack,"",@progbits
