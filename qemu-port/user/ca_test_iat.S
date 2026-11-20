/* ca_test_iat.S -- EUSER.DLL import glue for the web browser fixture.
 *
 * The port's E32 loader supports PE2-style imports: the IAT lives at the end
 * of the text section and each slot initially holds a 1-based EUSER export
 * ordinal.  Before resuming the process the loader replaces the slot with the
 * resolved export address.  Slot 0 here is ordinal 915 == RDebug::Printf.
 * (The delivered build/user/ca_test.e32 is produced by mke32_webbrowser.py,
 * which emits the same IAT in its PE2 image; this stub lets the C source be
 * compiled with -DWEB_USE_EUSER against a PE2-aware linker.)
 */
    .section .text,"ax"
    .globl web_euser_printf
    .type web_euser_printf,@function
web_euser_printf:
    /* cdecl: [esp+4] = const char* text (no format specifiers -> no varargs).
       Tail-call the bound EUSER RDebug::Printf, leaving our argument in place. */
    mov $web_rdebug_printf_slot, %eax
    jmp *%eax
    .size web_euser_printf, .-web_euser_printf

    .section .iat,"a"
    .globl web_rdebug_printf_slot
web_rdebug_printf_slot:
    .long 915

    .section .note.GNU-stack,"",@progbits
