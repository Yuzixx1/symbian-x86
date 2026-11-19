// Internal kernel backend, not an RFs/IPC ABI. Include after Symbian types.
#ifndef R1_FAT_OFFSET_H
#define R1_FAT_OFFSET_H
namespace R1Fat
    {
    struct File
        {
        TInt volume;
        TInt device;
        TUint32 partition;
        TUint32 dirCluster, dirSector, dirIndex;
        TUint8 name[11];
        TBool open;
        File() : volume(-1), device(-1), partition(0), dirCluster(0), dirSector(0), dirIndex(0), open(EFalse) {}
        };
    // ALL calls (including Close) require existing UserFilesLock held by caller.
    // Never acquire it recursively; never wait for IPC/call user code under it.
    // No mount/unmount/format/rename/delete while handles exist. FS owns sharing,
    // permissions and public namespace authorization; this backend is trusted.
    // Absolute volume-relative short-name path; no drive prefix, dot or dotdot.
    TInt Open(char aDrive, const char* aPath, TBool aCreate, File& aFile);
    TInt Size(File& aFile, TUint32& aSize);
    TInt Read(File& aFile, TUint32 aOffset, TUint8* aData, TUint aLength, TUint& aDone);
    // Offset beyond EOF clamps to EOF; no holes. Count is successfully written
    // DATA bytes, even if later metadata fails. Always inspect return code.
    TInt Write(File& aFile, TUint32 aOffset, const TUint8* aData, TUint aLength, TUint& aDone);
    TInt Resize(File& aFile, TUint32 aSize);
    // Default calls real AtaFlush, routing actual device 4..7 to AHCI 0..3.
    // Optional injected hook must synchronously flush the passed actual device
    // and propagate error, not wait for IPC. No power-loss guarantee.
    typedef TInt (*FlushDrive)(TInt aDrive);
    TInt Flush(File& aFile, FlushDrive aFlush = 0);
    void Close(File& aFile); // release token only, not a durability boundary
    // Mutation errors can leave leaked allocations, divergent FAT copies, changed
    // data or uncertain directory state. Affected volume is fail-stopped for ALL
    // device (all partition/letter aliases), including legacy FAT writes, until
    // reboot/offline repair; no clearing by close/drop/remount/format.
    // No transaction, crash recovery or media-sector atomicity is promised.
    // No handle generations across remounts: close all before topology changes.
    // Legacy mutation entry points and every FAT-sector write consult quarantine.
    // Resize growth contents unspecified (new clusters zeroed, retained tail
    // bytes may reappear); position belongs to FS and Resize never changes it.
    }
#endif
