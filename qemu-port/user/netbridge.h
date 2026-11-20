#ifndef QPORT_USER_NETBRIDGE_H
#define QPORT_USER_NETBRIDGE_H

/* ---------------------------------------------------------------------------
 * Shared-memory layout for the kernel <-> userland e1000 frame bridge.
 *
 * The kernel allocates struct nb_shared in low, identity-mapped RAM and
 * exposes its physical address/size through two dedicated slow executives.
 * User page directories map all low RAM U/S=1 (mmu/pd.cpp flat 4M PDE map),
 * so the returned physical base is directly usable as a virtual pointer from
 * a ring-3 PE2 E32 process.  No named chunk backing is needed.
 *
 * Two single-producer/single-consumer rings:
 *   tx: userland producer -> kernel consumer (posted to e1000_tx)
 *   rx: kernel producer (from e1000_recv) -> userland consumer
 * A slot's volatile len is the ready flag (0 = empty); head/tail are ordered
 * cursors modulo NB_RING_SLOTS.  Memory barriers around len/head/tail clones
 * are the caller's responsibility (see netbridge_test.c / qemu_netbridge.cpp).
 * ------------------------------------------------------------------------ */

#define NB_MAGIC        0x4E425247u   /* "NBRG" */
#define NB_VERSION      1u
#define NB_FRAME_SIZE   2048u
#define NB_RING_SLOTS   16u

/* Slow exec slots repurposed from InvalidExecHandler at boot. The genuine
 * table has 271 entries (0..270); 271/272 are out of bounds. Slots
 * EExecSlowAtomicAxo64 (247) and EExecSlowAtomicCas64 (248) are 64-bit atomic
 * slow execs that are invalid in this build and have no EUSER stub/caller
 * (docs/r1_heap_exec_v0.md), so they are safe to repurpose. */
#define QE_EXEC_NETBRIDGE_BASE  247u
#define QE_EXEC_NETBRIDGE_SIZE  248u

struct nb_slot {
    volatile unsigned len;                 /* 0 = empty, else frame byte count */
    volatile unsigned reserved;
    volatile unsigned char data[NB_FRAME_SIZE];
};

struct nb_ring {
    volatile unsigned head;                /* producer cursor (mod NB_RING_SLOTS) */
    volatile unsigned tail;                /* consumer cursor (mod NB_RING_SLOTS) */
    volatile unsigned drops;               /* producer slot-busy overruns */
    volatile unsigned reserved;
    struct nb_slot slot[NB_RING_SLOTS];
};

struct nb_shared {
    volatile unsigned magic;               /* NB_MAGIC once netbridge_init ran */
    volatile unsigned version;             /* NB_VERSION */
    volatile unsigned done;                /* user sets 1 -> kernel poll loop exits */
    volatile unsigned char mac[6];         /* guest MAC, read from e1000 EEPROM */
    volatile unsigned char pad[2];
    struct nb_ring tx;
    struct nb_ring rx;
};

#endif /* QPORT_USER_NETBRIDGE_H */
