/* Genuine EFILE.EXE (sfile/ + F32 + EFSRV client) E32 link layout.
   Mirrors genuine/link_exe.ld but folds .init_array into .data so the PE2
   packer sees only .text/.data/.bss; efile_glue calls the constructors. */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS
{
    . = 0x00400000;
    .text : { *(.text.start) *(.text .text.*) *(.rodata .rodata.*) *(.genuine_exp) }
    . = 0x00500000;
    .data : {
        *(.data .data.*)
        __init_array_start = .;
        *(.init_array)
        __init_array_end = .;
    }
    .bss : { *(.bss .bss.*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.gcc_except_table*) }
}
