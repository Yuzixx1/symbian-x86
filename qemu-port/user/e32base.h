#ifndef QPORT_USER_E32BASE_H
#define QPORT_USER_E32BASE_H

/*
 * e32base.h — EUSER CBase/CServer2/CSession2/RMessage2 shim for S-04/I-08.
 * Freestanding, no STL. CServer2/CSession2 are now genuine virtual classes
 * with compiler-generated vtables (RTTI is on by default for the user build);
 * the fake RTTI/vtable arrays were removed from euser_shim.cpp.
 * All methods defined inline.
 */
#include "e32cmn.h"
#include "f32ver.h"

/* ipc.h and runtime.h are C translation units without extern "C" guards. */
extern "C" {
#include "ipc.h"
#include "runtime.h"
}

extern "C" int exec0(unsigned number);
extern "C" int exec2(unsigned, unsigned, unsigned);
extern "C" int ipc_wait_any(void);

/* Symbian placement-new tag: new(ELeave) T maps to CBase::operator new(size,TLeave).
 * An enum needs no out-of-line ELeave definition, so it is safe in a header. */
enum TLeave { ELeave };

/* Forward declarations */
class CSession2;
class CServer2;
class CActive;

/* ========================================================== TRequestStatus */
#define KRequestPending 0x80000001

class TRequestStatus {
public:
    TInt iInt;
    TRequestStatus() : iInt(KRequestPending) {}
    explicit TRequestStatus(TInt aInit) : iInt(aInit) {}
    TBool IsComplete() const { return iInt != (TInt)KRequestPending; }
};

/* EPriorityNormal / TThreadPriority come from e32cmn.h. */

/* ===================================================================== User */
class User {
public:
    static TInt LeaveIfError(TInt aError) {
        if (aError != KErrNone) Leave(aError);
        return aError;
    }
    [[noreturn]] static void Leave(TInt aError) { process_exit(aError); }
    static TBool QueryVersionSupported(const TVersion& aCurrent,
                                       const TVersion& aRequested) {
        if (aRequested.iMajor < aCurrent.iMajor) return ETrue;
        if (aRequested.iMajor == aCurrent.iMajor &&
            aRequested.iMinor <= aCurrent.iMinor) return ETrue;
        return EFalse;
    }
    static void FsRegisterThread() { exec0(81); }
    static TInt WaitForRequest(TRequestStatus& aStatus) {
        ipc_wait_any();
        return aStatus.iInt;
    }
    static void WaitForAnyRequest() { ipc_wait_any(); }
    /* P-06: block until the process with kernel id aPid dies. aStatus completes
       with the target's exit reason (or KErrTimedOut / an open error). The
       genuine kernel logon executives (116/123/117/124) do the blocking. */
    static TInt WaitForExit(TUint aPid, TRequestStatus& aStatus, TInt aTimeoutMs);
    static void RequestComplete(TRequestStatus& aStatus, TInt aRequestStatus) {
        aStatus.iInt = aRequestStatus;
        exec2(58, 1, 0);
    }
private:
    static TInt iTlsIndex;
};

/* ===================================================================== CBase */
class CBase {
public:
    CBase() : iNext(nullptr) {}
    /* Virtual so CServer2 can delete a CSession2 through the base pointer and
     * still run FileSession::~FileSession (which disconnects its Core token). */
    virtual ~CBase() {}
    static void* operator new(TUint aSize) {
        void* p = malloc(aSize ? aSize : 1u);
        if (!p) User::Leave(KErrNoMemory);
        return p;
    }
    /* new(ELeave) T -- the genuine Symbian allocation idiom. */
    static void* operator new(TUint aSize, TLeave) {
        void* p = malloc(aSize ? aSize : 1u);
        if (!p) User::Leave(KErrNoMemory);
        return p;
    }
    static void operator delete(void* aPtr) { if (aPtr) free(aPtr); }
    CBase* iNext;
};

/* ============================================================= TPriQueLink */
/* Genuine EUSER layout: CActive::iLink is a TPriQueLink carrying the priority
 * and intrusive next/prev pointers used by CActiveScheduler's priority queue. */
class TPriQueLink {
public:
    TPriQueLink() : iPriority(0), iNext(nullptr), iPrev(nullptr) {}
    TInt iPriority;
    TPriQueLink* iNext;
    TPriQueLink* iPrev;
};

/* ==================================================================== CActive */
/* Genuine ABI: derives from CBase, owns a TRequestStatus, has virtual RunL()/
 * DoCancel()/RunError(), and is dispatched only through the scheduler vptr. */
class CActive : public CBase {
public:
    enum {
        EPriorityIdle = -100, EPriorityLow = -10, EPriorityStandard = 0,
        EPriorityUserInput = 10, EPriorityHigh = 20
    };
    CActive(TInt aPriority)
        : iPriority(aPriority), iActive(EFalse), iAdded(EFalse) {
        iStatus.iInt = KRequestPending;
        iLink.iPriority = aPriority;
    }
    virtual ~CActive() { Cancel(); Deque(); }
    /* Pure virtual: a concrete active MUST supply its own vtable entry. */
    virtual void RunL() = 0;
    virtual void DoCancel() = 0;
    virtual TInt RunError(TInt aError) { (void)aError; return KErrNone; }

    void SetActive() { iActive = ETrue; }
    void Cancel() {
        if (iActive) { DoCancel(); iStatus.iInt = KErrNone; iActive = EFalse; }
    }
    void Deque();
    TBool IsActive() const { return iActive; }
    TBool IsAdded() const { return iAdded; }
    void SetPriority(TInt aPriority) {
        iPriority = aPriority; iLink.iPriority = aPriority;
    }
    TInt Priority() const { return iLink.iPriority; }

    TRequestStatus iStatus;     /* completion word, written by the producer */
    TInt iPriority;             /* source-compatible mirror of iLink.iPriority */
    TPriQueLink iLink;          /* intrusive priority-queue link */
protected:
    TBool iActive;
    TBool iAdded;
    friend class CActiveScheduler;
};

/* ========================================================= CActiveScheduler */
/* Genuine wait loop: WaitForAnyRequest() blocks on the current thread's
 * request semaphore; completed actives are dequeued and RunL() is called
 * through the active's vptr. Stop() ends the loop. */
class CActiveScheduler : public CBase {
public:
    CActiveScheduler() : iActiveQ(nullptr), iStopping(EFalse) {}
    virtual ~CActiveScheduler() {
        while (iActiveQ) {
            CActive* a = iActiveQ;
            iActiveQ = (CActive*)a->iLink.iNext;
            a->iLink.iNext = nullptr; a->iAdded = EFalse;
        }
        if (iScheduler == this) iScheduler = nullptr;
    }
    static void Install(CActiveScheduler* aScheduler) { iScheduler = aScheduler; }
    static void Replace(CActiveScheduler* aScheduler) { iScheduler = aScheduler; }
    static CActiveScheduler* Current() { return iScheduler; }
    /* Priority-ordered intrusive queue (highest priority first). */
    static void Add(CActive* aActive) {
        if (!iScheduler || !aActive) return;
        aActive->iAdded = ETrue;
        CActive* prev = nullptr;
        CActive* cur = iScheduler->iActiveQ;
        while (cur && cur->iLink.iPriority >= aActive->iLink.iPriority) {
            prev = cur; cur = (CActive*)cur->iLink.iNext;
        }
        aActive->iLink.iNext = (TPriQueLink*)cur;
        if (prev) prev->iLink.iNext = (TPriQueLink*)aActive;
        else iScheduler->iActiveQ = aActive;
    }
    static void Start() {
        CActiveScheduler* s = iScheduler;
        if (!s) return;
        s->iStopping = EFalse;
        while (!s->iStopping) {
            s->WaitForAnyRequest();
            if (s->iStopping) break;
            s->DoRunL();
        }
    }
    static void Stop() { if (iScheduler) iScheduler->iStopping = ETrue; }
    static TInt RunIfReady(TInt& aError, TInt aMinimumPriority) {
        (void)aMinimumPriority; aError = KErrNone;
        if (!iScheduler || !iScheduler->iActiveQ) return 0;
        iScheduler->DoRunL();
        return 1;
    }
    virtual void WaitForAnyRequest() { User::WaitForAnyRequest(); }
    static void Error(TInt aError) { (void)aError; }
protected:
    void DoRunL() {
        CActive* prev = nullptr;
        CActive* cur = iActiveQ;
        while (cur) {
            CActive* next = (CActive*)cur->iLink.iNext;
            if (cur->iActive && cur->iStatus.IsComplete()) {
                if (prev) prev->iLink.iNext = (TPriQueLink*)next;
                else iActiveQ = next;
                cur->iLink.iNext = nullptr; cur->iAdded = EFalse;
                cur->iActive = EFalse;
                cur->RunL();                 /* vtable dispatch */
            }
            prev = cur; cur = next;
        }
    }
    CActive* iActiveQ;          /* priority-ordered queue head (instance) */
    TBool iStopping;
    static CActiveScheduler* iScheduler;
    friend class CActive;       /* CActive::Deque() unlinks from iActiveQ */
};

inline void CActive::Deque() {
    Cancel();
    if (iAdded && CActiveScheduler::Current()) {
        CActiveScheduler* s = CActiveScheduler::Current();
        CActive* prev = nullptr;
        CActive* cur = s->iActiveQ;
        while (cur && cur != this) { prev = cur; cur = (CActive*)cur->iLink.iNext; }
        if (cur == this) {
            if (prev) prev->iLink.iNext = iLink.iNext;
            else s->iActiveQ = (CActive*)iLink.iNext;
        }
    }
    iLink.iNext = nullptr; iLink.iPrev = nullptr; iAdded = EFalse;
}

/* CActiveSchedulerWait */
class CActiveSchedulerWait {
public:
    CActiveSchedulerWait() {}
    ~CActiveSchedulerWait() {}
    void Start() { CActiveScheduler::Start(); }
    void Stop() { CActiveScheduler::Stop(); }
};

/* ==================================================================== RMessage2 */
class RMessage2 {
public:
    enum { EConnect = -1, EDisConnect = -2 };
    RMessage2() : iMessage(nullptr) {}
    explicit RMessage2(struct IpcMessage* aMsg) : iMessage(aMsg) {}
    ~RMessage2() {}
    TUint32 Function() const { return iMessage ? iMessage->function : 0; }
    TInt Int0() const { return iMessage ? iMessage->args[0] : 0; }
    TInt Int1() const { return iMessage ? iMessage->args[1] : 0; }
    TInt Int2() const { return iMessage ? iMessage->args[2] : 0; }
    TInt Int3() const { return iMessage ? iMessage->args[3] : 0; }
    TInt GetDesLength(TInt aSlot) const {
        if (!iMessage) return KErrBadHandle;
        return ipc_length(iMessage, aSlot);
    }
    /* EExecMessageGetDesMaxLength (168) is the writable capacity; GetDesLength
     * (167) alone cannot distinguish a const package from a writable buffer. */
    TInt GetDesMaxLength(TInt aSlot) const {
        if (!iMessage) return KErrBadHandle;
        return exec2(168, (TUint)iMessage->handle, (TUint)aSlot);
    }
    /* 16-bit descriptors. */
    TInt Read(TInt aSlot, TDes& aDesc) const { return Read(aSlot, aDesc, 0); }
    TInt Read(TInt aSlot, TDes& aDesc, TInt aOffset) const {
        if (!iMessage) return KErrBadHandle;
        TInt r = ipc_read16(iMessage, aSlot, aDesc.Ptr(),
                            aDesc.MaxLength() * 2, aOffset);
        if (r < 0) return r;
        aDesc.SetLength(r / 2);
        return KErrNone;
    }
    /* 8-bit descriptors (packages, byte transfers) with byte offset. */
    TInt Read(TInt aSlot, TDes8& aDesc) const { return Read(aSlot, aDesc, 0); }
    TInt Read(TInt aSlot, TDes8& aDesc, TInt aOffset) const {
        if (!iMessage) return KErrBadHandle;
        TInt r = ipc_read8(iMessage, aSlot, aDesc.Ptr(),
                           aDesc.MaxLength(), aOffset);
        if (r < 0) return r;
        aDesc.SetLength(r);
        return KErrNone;
    }
    TInt Write(TInt aSlot, const TDesC& aDesc) const { return Write(aSlot, aDesc, 0); }
    TInt Write(TInt aSlot, const TDesC& aDesc, TInt aOffset) const {
        if (!iMessage) return KErrBadHandle;
        return ipc_write16(iMessage, aSlot, aDesc.Ptr(),
                           aDesc.Length() * 2, aOffset);
    }
    TInt Write(TInt aSlot, const TDesC8& aDesc) const { return Write(aSlot, aDesc, 0); }
    TInt Write(TInt aSlot, const TDesC8& aDesc, TInt aOffset) const {
        if (!iMessage) return KErrBadHandle;
        return ipc_write8(iMessage, aSlot, aDesc.Ptr(),
                          aDesc.Length(), aOffset);
    }
    void Complete(TInt aResult) const {
        if (iMessage) ipc_complete(iMessage, aResult);
    }
    void Panic(const TDesC16& aName, TInt aError) const {
        (void)aName; if (iMessage) ipc_complete(iMessage, aError);
    }
    TBool IsNull() const { return iMessage == nullptr || iMessage->handle == 0; }
    TSecureId SecureId() const { return TSecureId(iMessage ? (TUint32)iMessage->args[3] : 0u); }
    void SetMessage(struct IpcMessage* aMsg) { iMessage = aMsg; }
private:
    struct IpcMessage* iMessage;
};

/* =================================================================== CSession2 */
class CSession2 : public CBase {
public:
    CSession2() : iServer(0), iHandle(0) {}
    ~CSession2() {}
    /* Virtual: CServer2::Start dispatches through CSession2* to FileSession. */
    virtual void ServiceL(const RMessage2& aMessage) { (void)aMessage; }
    virtual void ServiceError(const RMessage2& aMessage, TInt aError)
        { (void)aMessage; (void)aError; }
protected:
    const CServer2* iServer;
    TUint32 iHandle;
    friend class CServer2;
};

/* ==================================================================== CServer2 */
class CServer2 : public CBase {
public:
    enum TServerType {
        EUnsharableSessions = 0, ESharableSessions = 1,
        EGlobalSharableSessions = 2
    };
    CServer2(TInt aPriority, TServerType aType = EUnsharableSessions)
        : iPriority(aPriority), iType((TInt)aType), iServerHandle(0),
          iStatus(KRequestPending), iSessionCount(0) {
        for (TInt i = 0; i < 64; ++i) iName[i] = 0;
    }
    ~CServer2() {}
    /* const, matching genuine CServer2 and FileServer::NewSessionL. */
    virtual CSession2* NewSessionL(const TVersion& aVersion,
                                   const RMessage2& aMessage) const {
        (void)aVersion; (void)aMessage;
        return nullptr;
    }
    /* Blocks in the receive loop; returns a Symbian error if the server dies. */
    TInt Start(const TDesC16& aName);
    TInt Publish() { return KErrNone; } /* overridden by concrete servers */
protected:
    TInt iPriority;
    TInt iType;
    TInt iServerHandle;
    TInt iStatus;
    TInt iSessionCount;
    RMessage2 iMessage;
    TUint16 iName[64];
};

/* CServer2::Start -- genuine receive loop.
 * The kernel echoes the cookie passed to ipc_accept() in IpcMessage::session,
 * so the session pointer itself is the opaque cookie. Connect messages (RMessage2
 * ::EConnect) build a session with the derived NewSessionL; all later messages
 * are dispatched to that session's virtual ServiceL, which completes them. */
inline TInt CServer2::Start(const TDesC16& aName) {
    char name[64];
    const TInt n = aName.Length();
    if (n <= 0 || n >= (TInt)sizeof(name)) return KErrArgument;
    const TUint16* text = aName.Ptr();
    for (TInt i = 0; i < n; ++i)
        name[i] = text[i] <= 0x7f ? (char)text[i] : '?';
    name[n] = 0;

    const int server = ipc_server_create(name);
    if (server < 0) return server;
    iServerHandle = server;
    iStatus = KRequestPending;

    for (;;) {
        struct IpcMessage msg;
        const TInt r = ipc_receive(server, &msg);
        if (r < 0) { ipc_receive_cancel(server); return r; }

        RMessage2 rmsg(&msg);

        if (msg.function == RMessage2::EConnect) {
            const TVersion version(KF32MajorVersionNumber,
                                   KF32MinorVersionNumber,
                                   KF32BuildVersionNumber);
            CSession2* session = NewSessionL(version, rmsg);
            if (!session) { ipc_complete(&msg, KErrNoMemory); continue; }
            session->iServer = this;
            session->iHandle = ++iSessionCount;
            ipc_accept(&msg, session);          /* cookie == session pointer */
            ipc_complete(&msg, KErrNone);
            continue;
        }

        if (msg.function == RMessage2::EDisConnect) {
            CSession2* session = (CSession2*)msg.session;
            if (session) { delete session; --iSessionCount; }
            ipc_complete(&msg, KErrNone);
            continue;
        }

        CSession2* session = (CSession2*)msg.session;
        if (!session) { ipc_complete(&msg, KErrBadHandle); continue; }
        iMessage = rmsg;
        session->ServiceL(rmsg);
        /* Every message must receive exactly one terminal reply. */
        if (msg.handle) ipc_complete(&msg, KErrNone);
    }
}

/* CleanupStack */
class CleanupStack {
public:
    static void PushL(CBase* aPtr) {
        aPtr->iNext = iStack;
        iStack = aPtr;
    }
    static CBase* Pop(CBase* aPtr) {
        if (iStack != aPtr) return aPtr;
        iStack = iStack->iNext; aPtr->iNext = nullptr; return aPtr;
    }
    static CBase* PopAndDestroy(CBase* aPtr) { Pop(aPtr); delete aPtr; return nullptr; }
private:
    static CBase* iStack;
};

#endif // QPORT_USER_E32BASE_H
