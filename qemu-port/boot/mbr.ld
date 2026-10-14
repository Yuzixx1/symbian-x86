/* mbr.ld — flat real-mode binary linked at its RUN address 0x6000.
 * (The BIOS loads sector 0 at 0x7C00; the code copies itself to 0x6000
 * first thing, so link-time labels address the relocated copy directly.) */
OUTPUT_FORMAT("binary")
OUTPUT_ARCH(i386)
ENTRY(_start)
SECTIONS
{
    . = 0x6000;
    .text : { *(.text) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    /DISCARD/ : { *(.note*) *(.eh_frame*) *(.comment*) }
}
