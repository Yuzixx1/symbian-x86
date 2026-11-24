/* Genuine-source DLL: code/data bases still 0x400000/0x500000 so elf2e32.py
   accepts the ELF; --type dll flips UID1/flags and writes the .def exports.
   Provide an ENTRY symbol (_start or --defsym/_start stub) in the sources. */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS
{
    . = 0x00400000;
    .text : { *(.text.start) *(.text .text.*) *(.rodata .rodata.*) *(.genuine_exp) }
    . = 0x00500000;
    .data : { *(.data .data.*) }
    .bss : { *(.bss .bss.*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.gcc_except_table*) }
}
