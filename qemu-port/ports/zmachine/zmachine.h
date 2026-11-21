#ifndef PORTS_ZMACHINE_ZMACHINE_H
#define PORTS_ZMACHINE_ZMACHINE_H

/* Minimal freestanding Z-machine interpreter core (Z-machine Standard 1.1,
 * versions 1-3, i.e. the original Infocom era).  No libc is used; the host
 * supplies the three I/O callbacks below.  The whole story file lives in a
 * static 128 KiB RAM image (v3 story files cannot exceed 128 KiB), so the
 * interpreter never touches the process malloc heap. */

typedef struct {
    void (*out_char)(unsigned zscii);        /* emit one ZSCII output code */
    int  (*read_line)(char *buf, unsigned cap); /* one command line, length or <0 */
    void (*flush)(void);                     /* flush a partial output line */
} zm_io;

#define ZM_GAME_CAPACITY 131072u   /* 128 KiB: the version 3 story-file limit */

/* The caller may read a story file directly into this buffer (in <=256 byte
 * chunks if it is using the PortFile IPC). */
unsigned char *zm_game_buffer(void);
unsigned zm_game_capacity(void);

/* Validate the header and initialise interpreter state.  Returns 0 on
 * success, -1 for an unsupported version or malformed header. */
int zm_init(const zm_io *io);

/* Run until @quit or a fatal error.  Returns 0 on a normal quit, -1 on a
 * fatal error (bad opcode, stack overflow, out-of-range memory access). */
int zm_run(void);

/* Diagnostics used by the host markers. */
unsigned long zm_instructions(void);
unsigned zm_game_version(void);
unsigned long zm_game_length(void);

#endif
