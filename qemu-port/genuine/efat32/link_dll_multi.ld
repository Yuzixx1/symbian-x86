/* Local copy of genuine/link_dll.ld for the multi-import DLL/FSY build.
   Bases stay 0x400000/0x500000 so pack_multi.py accepts the ELF; the final
   image is flipped to UID1=KDynamicLibraryUidValue + KImageDll by
   build_fsy_dll.py (which reuses tools/genuine_build.py:patch_exports).
   .genuine_exp is a zero-filled pad inside .text for the export directory. */
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
