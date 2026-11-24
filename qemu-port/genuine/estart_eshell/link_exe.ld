/* Link script for the genuine ESTART/ESHELL E32 images: same regions as
   genuine/link_exe.ld (elf2e32/pack_multi contract: .text at 0x400000,
   .data at 0x500000), with .init_array folded into .data so the PE2 packer
   sees only .text/.data/.bss and the glue can run the static constructors. */
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
        *(.init_array .init_array.*)
        __init_array_end = .;
    }
    .bss : { *(.bss .bss.*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.gcc_except_table*) }
}
