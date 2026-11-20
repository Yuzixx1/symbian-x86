/* Linker script for netstack.E32.  The .iat input section is emitted as its
   own output section directly after .text; mke32_netstack_elf.py turns its
   length into the PE2 iTextSize.  The six slots bind EUSER ordinals 915, 617,
   626, 925, 928 and 929 (markers, IPC and timing). */
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
