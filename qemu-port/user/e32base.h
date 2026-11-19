#ifndef QPORT_USER_E32BASE_H
#define QPORT_USER_E32BASE_H

/*
 * e32base.h — EUSER CBase/CServer2/CSession2/RMessage2 shim for S-04.
 * C++98, -ffreestanding -fno-exceptions -fno-rtti, no STL.
 * ALL classes are non-virtual — no RTTI required.
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
    TBool IsComplete() const { return iInt != KRequestPending; }
};

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
    ~CBase() {}
    static void* operator new(TUint aSize) {
        void* p = malloc(aSize ? aSize : 1u);
        if (!p) User::Leave(KErrNoMemory);
        return p;
    }
    static void operator delete(void* aPtr) { if (aPtr) free(aPtr); }
    CBase* iNext;
};

/* ==================================================================== CActive */
class CActive {
public:
    enum { EPriorityStandard = 0 };
    CActive(TInt aPriority) : iPriority(aPriority), iActive(EFalse), iNext(nullptr) {}
    ~CActive() {}
    void RunL() {}
    void DoCancel() {}
    void SetActive() { iActive = ETrue; }
    void Cancel() { DoCancel(); iActive = EFalse; }
    TBool IsActive() const { return iActive; }
    TInt iPriority;
    CActive* iNext;
protected:
    TBool iActive;
};

/* ========================================================= CActiveScheduler */
class CActiveScheduler {
public:
    CActiveScheduler() : iCurrent(nullptr) {}
    ~CActiveScheduler() {}
    static void Install(CActiveScheduler* aScheduler) { iScheduler = aScheduler; }
    static CActiveScheduler* Current() { return iScheduler; }
    static void Add(CActive* aActive) {
        aActive->iNext = iActiveQ;
        iActiveQ = aActive;
    }
    static void Stop() { exec2(58, 1, 0); }
    static void Start() {
        while (true) {
            WaitForAnyRequest();
            if (!iActiveQ) break;
            CActive* a = iActiveQ;
            if (a->IsActive()) {
                iActiveQ = a->iNext;
                a->RunL();
                continue;
            }
            break;
        }
    }
    static TInt RunIfReady(TInt& aError, TInt aMinimumPriority) {
        (void)aMinimumPriority;
        if (!iActiveQ || !iActiveQ->IsActive()) { aError = KErrNone; return 0; }
        CActive* a = iActiveQ; iActiveQ = a->iNext; a->RunL(); return 1;
    }
    static void WaitForAnyRequest() { User::WaitForAnyRequest(); }
    static void Error(TInt aError) { (void)aError; process_exit(aError); }
protected:
    static CActiveScheduler* iScheduler;
    static CActive* iActiveQ;
    CActiveScheduler* iCurrent;
};

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
    TInt GetDesMaxLength(TInt aSlot) const { return GetDesLength(aSlot); }
    TInt Read(TInt aSlot, TDes& aDesc) const {
        if (!iMessage) return KErrBadHandle;
        TUint16* ptr = aDesc.Ptr();
        TInt maxLen = aDesc.MaxLength();
        TInt r = ipc_read16(iMessage, aSlot, ptr, maxLen * 2, 0);
        if (r < 0) return r;
        aDesc.SetLength(r / 2);
        return KErrNone;
    }
    TInt Write(TInt aSlot, const TDesC& aDesc) {
        if (!iMessage) return KErrBadHandle;
        const TUint16* ptr = aDesc.Ptr();
        TInt len = aDesc.Length();
        return ipc_write8(iMessage, aSlot, ptr, len * 2, 0);
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
    void ServiceL(const RMessage2& aMessage) {}
    void ServiceError(const RMessage2& aMessage, TInt aError)
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
    virtual CSession2* NewSessionL(const TVersion& aVersion,
                                   const RMessage2& aMessage) {
        return nullptr;
    }
    void Start(const TDesC16& aName);
    TInt Publish() { return KErrNone; }
protected:
    TInt iPriority;
    TInt iType;
    TInt iServerHandle;
    TInt iStatus;
    TInt iSessionCount;
    RMessage2 iMessage;
    TUint16 iName[64];
};

/* CServer2::Start */
inline void CServer2::Start(const TDesC16& aName) {
    (void)aName;
    int server = ipc_server_create("!FileServer");
    if (server < 0) return;
    iServerHandle = server;
    iStatus = KRequestPending;

    while (true) {
        struct IpcMessage msg;
        TInt r = ipc_receive(server, &msg);
        if (r < 0) break;

        RMessage2 rmsg(&msg);

        if (msg.handle == 0) {
            TVersion version(KF32MajorVersionNumber, KF32MinorVersionNumber, KF32BuildVersionNumber);
            CSession2* session = NewSessionL(version, rmsg);
            if (session) {
                session->iServer = this;
                session->iHandle = 1;
                ipc_accept(&msg, session);
                ipc_complete(&msg, KErrNone);
            } else {
                ipc_complete(&msg, KErrNoMemory);
            }
        } else if (msg.handle == 1) {
            iMessage = rmsg;
            // Existing session - complete with not supported for now
            ipc_complete(&msg, KErrNotSupported);
        } else {
            ipc_complete(&msg, KErrBadHandle);
        }
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
