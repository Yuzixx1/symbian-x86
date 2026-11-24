/* Link script for the genuine EWSRV E32.  .init_array is folded into .data so
   the PE2 packer sees only .text/.data/.bss; ewsrv_glue calls the constructors. */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS
{
    . = 0x00400000;
    .text : { *(.text.start) *(.text .text.*) *(.rodata .rodata.*) }
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
