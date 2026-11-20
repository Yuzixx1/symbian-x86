/* Linker script for netstack.E32.  The .iat input section is emitted as its
   own output section directly after .text; elf2e32_pe2.py turns its length
   into the PE2 iTextSize and binds ord 915 into slot 0. */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS
{
    . = 0x00400000;
    .text : { *(.text.start) *(.text .text.*) *(.rodata .rodata.*) }
    .iat  : { *(.iat) }
    . = 0x00500000;
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.gcc_except_table*) }
}
