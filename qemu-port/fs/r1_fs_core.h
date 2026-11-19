#ifndef R1_FS_CORE_H
#define R1_FS_CORE_H
#include "r1_fs_backend.h"
namespace R1Fs {
// Status codes live in r1_fs_backend.h (shared with the private channel).
enum { ShareExclusive=0, ShareReaders=1, ShareAny=2, ShareFlexible=3,
       ModeWrite=0x200, ModeAsyncAll=0x400, ModeBig=0x40000 };
enum { D0=0x10000, D1=0x20000, D2=0x40000 };
enum { SessionPath=14, SetSessionPath=15, FileClose=28, FileOpen=30,
       FileCreate=31, FileReplace=32, FileRead=34, FileWrite=35,
       FileSeek=38, FileFlush=39, FileSize=40, FileSetSize=41,
       FileReadCancel=120 };
enum { KMaxSessions=16, KMaxShares=64, KFileMax=0xffffffffu };
struct Frame { uint32_t function; int32_t a[4]; uint8_t descriptor[4];
               uint32_t length[4], capacity[4]; }; // no raw IPC pointer
struct Decoded { uint16_t operation; uint32_t flags; bool widePosition;
                 uint32_t packageLength, resultLength; };
int32_t Decode(const Frame& frame, Decoded& out);
int32_t Canonical(const uint16_t* text, uint32_t length, const Path& cwd,
                  bool directory, Path& out);
class Core {
public:
    explicit Core(Backend& backend);
    int32_t Connect(uint32_t sid, uint32_t& session);
    int32_t Disconnect(uint32_t session);
    int32_t GetPath(uint32_t session, Path& path) const;
    int32_t SetPath(uint32_t session, const uint16_t* path, uint32_t length);
    int32_t Open(uint32_t session, const uint16_t* path, uint32_t length,
                 uint32_t mode, Disposition how, uint32_t& handle);
    // Open reserves a share and backend key; publish only AFTER writing the
    // four-byte handle reply. Abort unwinds failed reply; Commit publishes.
    int32_t Commit(uint32_t session, uint32_t handle);
    int32_t CheckShare(uint32_t session, uint32_t handle) const;
    int32_t Abort(uint32_t session, uint32_t handle);
    int32_t Close(uint32_t session, uint32_t handle);
    int32_t Read(uint32_t session, uint32_t handle, uint64_t position,
                 bool current, uint8_t* data, uint32_t count, uint32_t& actual);
    // Used by IPC bridge before copying into the client's descriptor. The
    // position advances only when ConfirmRead is called after successful copy.
    int32_t StageRead(uint32_t session, uint32_t handle, uint64_t position,
                      bool current, uint8_t* data, uint32_t count,
                      uint32_t& actual, uint64_t& next);
    int32_t ConfirmRead(uint32_t session, uint32_t handle, uint64_t next);
    int32_t Write(uint32_t session, uint32_t handle, uint64_t position,
                  bool current, const uint8_t* data, uint32_t count,
                  uint32_t& committed);
    int32_t Seek(uint32_t session, uint32_t handle, int64_t offset,
                 uint32_t mode, uint64_t& result);
    int32_t Size(uint32_t session, uint32_t handle, uint64_t& size);
    int32_t Resize(uint32_t session, uint32_t handle, int64_t size);
    int32_t Flush(uint32_t session, uint32_t handle);
    int32_t Writable(uint32_t session, uint32_t handle) const;
    int32_t Readable(uint32_t session, uint32_t handle) const;
    int32_t SetDrive(uint16_t drive);
private:
    Core(const Core&); Core& operator=(const Core&);
    struct Session { bool used; uint32_t generation; Path path; };
    struct Share { bool used, published, write, big, poisoned;
                   uint32_t owner, generation;
                   uint32_t mode; uint64_t position, size; Path path; Key key; };
    Backend& backend_;
    Session sessions_[KMaxSessions]; Share shares_[KMaxShares];
    uint32_t nextSession_, nextHandle_;
    uint16_t drive_;
    Share* Find(uint32_t session, uint32_t handle, bool pending=false);
    const Share* Find(uint32_t session, uint32_t handle) const;
    int32_t ValidateSession(uint32_t session) const;
    int32_t CloseShare(Share& share);
    void UpdateSize(const Path& path, uint64_t size, bool stable);
};
}
#endif
