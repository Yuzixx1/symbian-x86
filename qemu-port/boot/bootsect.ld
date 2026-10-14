/* bootsect.ld — flat real-mode binary at 0x7C00. */
OUTPUT_FORMAT("binary")
OUTPUT_ARCH(i386)
ENTRY(_start)
SECTIONS
{
    . = 0x7C00;
    .text : { *(.text) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    /DISCARD/ : { *(.note*) *(.eh_frame*) *(.comment*) }
}
