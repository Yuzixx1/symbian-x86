/* nk_ctx_gas.S — SThreadStack-exact context switch (Phase 5).
 * Frame layout (low->high, ESP points at iCR0), cf. nkern/x86/nk_plat.h:59:
 *   iCR0,iEbx,iEsi,iEdi,iEbp,iGs,iFs,iReschedFlag,iEip
 * Save path pushes in layout order; restore pops in order and `ret`s to iEip.
 * Mirrors the save/restore set of genuine ncsched.cia Reschedule
 * (fs/gs/ebp/edi/esi/ebx/CR0), minus FPU (FXSAVE deferred with iCoprocessorState).
 */
.code32

.section .text.nkctx, "ax"
.code32

/* void NThread_Switch(TUint* oldEspOut, TUint newEsp) */
.globl NThread_Switch
.type NThread_Switch, @function
NThread_Switch:
    movl 4(%esp), %eax    /* oldEspOut */
    movl 8(%esp), %edx    /* newEsp */
    pop %ecx              /* return address -> ECX */
    push %ecx             /* iEip */
    push $0               /* iReschedFlag */
    push %fs              /* iFs */
    push %gs              /* iGs */
    push %ebp             /* iEbp */
    push %edi             /* iEdi */
    push %esi             /* iEsi */
    push %ebx             /* iEbx */
    mov %cr0, %ebx
    push %ebx             /* iCR0 */
    mov %esp, (%eax)      /* suspend current */
    mov %edx, %esp        /* resume next */
    pop %ebx
    mov %ebx, %cr0        /* restore CR0 (same value) */
    pop %ebx              /* iEbx */
    pop %esi              /* iEsi */
    pop %edi              /* iEdi */
    pop %ebp              /* iEbp */
    pop %gs               /* iGs */
    pop %fs               /* iFs */
    add $4, %esp          /* skip iReschedFlag */
    ret                   /* -> iEip */

/* TUint NThread_InitStack(top, entry, arg): build first-switch frame.
 * Resulting resume ESP, such that restore lands in entry(arg) with a
 * return to ThreadExitTrap. */
.globl NThread_InitStack
.type NThread_InitStack, @function
NThread_InitStack:
    push %ebp
    mov %esp, %ebp
    push %ebx
    mov 8(%ebp), %edx     /* stack top (high addr) */
    mov 12(%ebp), %eax    /* entry */
    mov 16(%ebp), %ecx    /* arg */
    /* thread argument + exit guard below the frame */
    sub $8, %edx
    mov %ecx, (%edx)      /* arg at [ESP] for entry */
    mov $ThreadExitTrap, %ebx
    mov %ebx, 4(%edx)     /* return addr for entry */
    /* SThreadStack frame (low->high): CR0 EBX ESI EDI EBP GS FS FLAG EIP */
    sub $36, %edx
    mov %cr0, %ebx
    mov %ebx, 0(%edx)     /* iCR0 */
    movl $0, 4(%edx)      /* iEbx */
    movl $0, 8(%edx)      /* iEsi */
    movl $0, 12(%edx)     /* iEdi */
    movl $0, 16(%edx)     /* iEbp */
    movl $0, 20(%edx)     /* iGs */
    movl $0, 24(%edx)     /* iFs */
    movl $0, 28(%edx)     /* iReschedFlag */
    mov %eax, 32(%edx)    /* iEip = entry */
    mov %edx, %eax        /* return resume ESP */
    pop %ebx
    pop %ebp
    ret

.globl ThreadExitTrap
.type ThreadExitTrap, @function
ThreadExitTrap:
    cli
    lea thread_exit_msg, %esi
    call bootcpu_com1_puts
1:  hlt
    jmp 1b

.section .rodata.nkctx, "a"
thread_exit_msg:
    .asciz "EKA2-QEMU NKern: FATAL thread returned\r\n"
