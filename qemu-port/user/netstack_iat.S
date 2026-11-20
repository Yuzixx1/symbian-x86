/* netstack_iat.S -- EUSER.DLL import glue for netstack.E32.
 *
 * The port's E32 loader supports PE2-style imports: the IAT lives at the end
 * of the text section and each slot initially holds a 1-based EUSER export
 * ordinal.  Before resuming the process the loader replaces the slot with the
 * resolved export address.  Slot 0 here is ordinal 915 == RDebug::Printf.
 */
    .section .text,"ax"
    .globl net_euser_printf
    .type net_euser_printf,@function
net_euser_printf:
    /* cdecl: [esp+4] = const char* text (no format specifiers -> no varargs).
       Tail-call the bound EUSER RDebug::Printf; our caller's return address
       and argument remain in place on the stack. */
    mov $net_rdebug_printf_slot, %eax
    jmp *%eax
    .size net_euser_printf, .-net_euser_printf

    /* Placed last in .text by user/link_netstack.ld.  The absolute address is
       kept as an R_386_32 relocation so the E32 loader adds codeDelta. */
    .section .iat,"a"
    .globl net_rdebug_printf_slot
net_rdebug_printf_slot:
    .long 915

    .section .note.GNU-stack,"",@progbits
