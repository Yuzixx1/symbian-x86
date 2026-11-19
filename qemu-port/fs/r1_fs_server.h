#ifndef R1_FS_SERVER_H
#define R1_FS_SERVER_H
#include <e32base.h>
#include "r1_fs_core.h"
namespace R1Fs {
/* EUSER TThreadPriority spelling used by FileServer's CServer2 base
 * (kernelhwsrv e32const.h: EPriorityNormal == 0). Kept namespace-local so the
 * freestanding shim's global constant set cannot create an ambiguity. */
static const TInt EPriorityNormal = 0;
// Construct at B1 with an already authenticated injected backend. Integration
// owner alone calls UserSvr::FsRegisterThread at B2 and Start at B3. No E32Main
// here: this source is linked into the eventual shared portfilesrv.exe host.
class FileServer : public CServer2 {
public:
    static FileServer* NewL(Backend& backend);
    TInt Publish(); // call only after successful B2 registration
    Core& State() { return core_; }
private:
    explicit FileServer(Backend& backend);
    CSession2* NewSessionL(const TVersion&,const RMessage2&) const;
    Core core_;
};
}
#endif
