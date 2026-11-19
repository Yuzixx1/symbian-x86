#ifndef R1_FS_CHANNEL_EKA_H
#define R1_FS_CHANNEL_EKA_H
#include <e32std.h>
#include "r1_fs_channel.h"
namespace R1Fs {
class ChannelTransport;
// EKA-only user-side backend. Owns the private RBusLogicalChannel and the
// portable BackendCore. The service integrator constructs it after the trusted
// host process is pinned and before publishing FileServer.
class ChannelBackend : public Backend {
public:
    ChannelBackend();
    ~ChannelBackend();
    int32_t Connect();
    void Disconnect();
    int32_t Lookup(const Path&,Meta&);
    int32_t Open(const Path&,Disposition,Key&,Meta&);
    IoResult ReadAt(Key,uint64_t,uint8_t*,uint32_t);
    IoResult WriteAt(Key,uint64_t,const uint8_t*,uint32_t);
    IoResult Resize(Key,uint64_t);
    int32_t Flush(Key);
    int32_t Close(Key);
private:
    ChannelBackend(const ChannelBackend&);
    ChannelBackend& operator=(const ChannelBackend&);
    ChannelTransport* channel_;
    BackendCore core_;
};
}
#endif
