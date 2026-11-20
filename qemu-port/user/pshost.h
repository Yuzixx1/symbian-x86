#ifndef QPORT_PSHOST_H
#define QPORT_PSHOST_H

/* Shared process census published by the kernel PS exec
 * (variant/qemu_ps_exec.cpp, slow exec slot 241).
 *
 * The exec returns the identity-mapped base of this struct (op 0) and
 * refreshes it from the genuine EKA2 containers (op 1).  Low RAM below
 * 0x80000000 is identity-mapped U/S=1 in user page tables (mmu/pd.cpp), so a
 * ring-3 E32 process can read the snapshot directly -- the same contract the
 * netbridge (exec 247/248) relies on.
 */
#define PS_MAGIC     0x50535631u   /* "PSV1" */
#define PS_MAX_PROCS 32
#define PS_NAME_LEN  48

struct ps_process
    {
    int  pid;                     /* DProcess::iId (container id) */
    char name[PS_NAME_LEN];       /* NUL-terminated 7-bit FullName */
    };

struct ps_census
    {
    unsigned      magic;          /* PS_MAGIC once a snapshot is valid */
    int           processes;      /* K::Containers[EProcess]->Count() */
    int           channels;       /* UserFilesChannelCount() */
    int           segs;           /* DCodeSeg::GlobalList count */
    int           listed;         /* populated entries in proc[] (<=32) */
    struct ps_process proc[PS_MAX_PROCS];
    };

#endif /* QPORT_PSHOST_H */
