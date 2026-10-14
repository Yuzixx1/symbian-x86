/* link.ld — QEMU i386 link for the Phase 2 bridge (and Phase 3 kernel).
 * Load address 1M matches QEMU -kernel ELF convention and QEMU_KERNEL_LOAD.
 * .multiboot MUST stay inside the first 8KiB of the image (Multiboot §3.1.1).
 */
OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_mb_entry)

BASE = 0x100000;

SECTIONS
{
    . = BASE;
    .multiboot : {
        KEEP(*(.multiboot))
    }
    .text : {
        *(.text.entry)
        *(.text .text.*)
    }
    .rodata : {
        *(.rodata .rodata.*)
    }
    .data : {
        *(.data .data.*)
    }
    /* Phase 9: static constructors (TScheduler, TheTimerQ, TDfc members).
     * GCC16 emits .init_array; our entry never ran them (win32crt covers only
     * the legacy __CTOR_LIST__). Kept + walked by QemuRunStaticCtors. */
    .init_array : {
        PROVIDE(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) .init_array*));
        PROVIDE(__init_array_end = .);
    }
    .bss : {
        *(.bss .bss.* COMMON)
    }

    /* Phase 8 Step 2: MSVC-underscore bridge. atomics.cia __redir__
     * emits `jmp _X' (MSVC mangling); on ELF the bodies are `__X'.
     * Aliases only — atomics.cia untouched. */
    PROVIDE(___e32_atomic_store_rel32 = __e32_atomic_store_rel32);
    PROVIDE(___e32_atomic_swp_ord32 = __e32_atomic_swp_ord32);
    PROVIDE(___e32_atomic_cas_ord32 = __e32_atomic_cas_ord32);
    PROVIDE(___e32_atomic_add_ord32 = __e32_atomic_add_ord32);
    PROVIDE(___e32_atomic_and_ord32 = __e32_atomic_and_ord32);
    PROVIDE(___e32_atomic_ior_ord32 = __e32_atomic_ior_ord32);
    PROVIDE(___e32_atomic_xor_ord32 = __e32_atomic_xor_ord32);
    PROVIDE(___e32_atomic_axo_ord32 = __e32_atomic_axo_ord32);
    PROVIDE(___e32_atomic_tau_ord32 = __e32_atomic_tau_ord32);
    PROVIDE(___e32_atomic_tas_ord32 = __e32_atomic_tas_ord32);
    PROVIDE(___e32_atomic_store_rel16 = __e32_atomic_store_rel16);
    PROVIDE(___e32_atomic_swp_ord16 = __e32_atomic_swp_ord16);
    PROVIDE(___e32_atomic_cas_ord16 = __e32_atomic_cas_ord16);
    PROVIDE(___e32_atomic_add_ord16 = __e32_atomic_add_ord16);
    PROVIDE(___e32_atomic_and_ord16 = __e32_atomic_and_ord16);
    PROVIDE(___e32_atomic_ior_ord16 = __e32_atomic_ior_ord16);
    PROVIDE(___e32_atomic_xor_ord16 = __e32_atomic_xor_ord16);
    PROVIDE(___e32_atomic_axo_ord16 = __e32_atomic_axo_ord16);
    PROVIDE(___e32_atomic_tau_ord16 = __e32_atomic_tau_ord16);
    PROVIDE(___e32_atomic_tas_ord16 = __e32_atomic_tas_ord16);
    PROVIDE(___e32_atomic_store_rel8 = __e32_atomic_store_rel8);
    PROVIDE(___e32_atomic_swp_ord8 = __e32_atomic_swp_ord8);
    PROVIDE(___e32_atomic_cas_ord8 = __e32_atomic_cas_ord8);
    PROVIDE(___e32_atomic_add_ord8 = __e32_atomic_add_ord8);
    PROVIDE(___e32_atomic_and_ord8 = __e32_atomic_and_ord8);
    PROVIDE(___e32_atomic_ior_ord8 = __e32_atomic_ior_ord8);
    PROVIDE(___e32_atomic_xor_ord8 = __e32_atomic_xor_ord8);
    PROVIDE(___e32_atomic_axo_ord8 = __e32_atomic_axo_ord8);
    PROVIDE(___e32_atomic_tau_ord8 = __e32_atomic_tau_ord8);
    PROVIDE(___e32_atomic_tas_ord8 = __e32_atomic_tas_ord8);
    PROVIDE(___e32_atomic_store_rel64 = __e32_atomic_store_rel64);
    PROVIDE(___e32_atomic_swp_ord64 = __e32_atomic_swp_ord64);
    PROVIDE(___e32_atomic_cas_ord64 = __e32_atomic_cas_ord64);
    PROVIDE(___e32_atomic_add_ord64 = __e32_atomic_add_ord64);
    PROVIDE(___e32_atomic_and_ord64 = __e32_atomic_and_ord64);
    PROVIDE(___e32_atomic_ior_ord64 = __e32_atomic_ior_ord64);
    PROVIDE(___e32_atomic_xor_ord64 = __e32_atomic_xor_ord64);
    PROVIDE(___e32_atomic_axo_ord64 = __e32_atomic_axo_ord64);
    PROVIDE(___e32_atomic_tau_ord64 = __e32_atomic_tau_ord64);
    PROVIDE(___e32_atomic_tas_ord64 = __e32_atomic_tas_ord64);
    /* Freestanding ctor anchor for win32crt.h constructStatics (empty: BSS zeros). */
    PROVIDE(__CTOR_LIST__ = .);
    /* MSVC-vs-Itanium hardcoded calls inside x86 .cia files: the asm strings
     * carry MSVC-era double-underscore names, GCC defines single-underscore.
     * Aliases only — .cia sources untouched. */
    PROVIDE(__ZN10TScheduler9QueueDfcsEv = _ZN10TScheduler9QueueDfcsEv);
    PROVIDE(__ZN11NThreadBase4ExitEv = _ZN11NThreadBase4ExitEv);
    PROVIDE(__ZN11NThreadBase21CallUserModeCallbacksEv = _ZN11NThreadBase21CallUserModeCallbacksEv);
    _end = .;

    /* Exported for the Phase-3 ResetEntry port + debuggers */
    __qemu_load_base = BASE;
    __qemu_handoff_sym = qemu_handoff;
}
