/* Port startup entry point for the genuine F32 DLL/FSY E32 images.
   The E32 loader only needs a valid in-code entry slot; these images are
   libraries, never started as processes.  Real work begins at the exported
   entry points (CreateFileSystem / EFSRV client ordinals). */
.section .text.start,"ax",@progbits
.globl _start
.type _start, @function
_start:
    ret
.size _start, .-_start
.section .note.GNU-stack,"",@progbits
