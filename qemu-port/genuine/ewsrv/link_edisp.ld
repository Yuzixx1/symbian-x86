/* EDISP.DLL link script: same layout as genuine/link_dll.ld (code/data bases
   0x400000/0x500000 so tools/elf2e32.py accepts the ELF), plus a discard for
   .init_array/.fini_array, which elf2e32.py rejects as an extra allocated
   section (the driver has no static constructors; the port runs none). */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS
{
    . = 0x00400000;
    .text : { *(.text.start) *(.text .text.*) *(.rodata .rodata.*) *(.genuine_exp) }
    . = 0x00500000;
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.gcc_except_table*)
                 *(.init_array) *(.fini_array) *(.ctors) *(.dtors) }
}
