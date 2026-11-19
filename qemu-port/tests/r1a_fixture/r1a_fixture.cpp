// Port-authored R1-A fixture; NOT an unchanged upstream application.
// Source only: UNCOMPILED / UNRUN. See README.md.
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>

#ifndef SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API
#error R1A fixture requires port-x86-rfile64-v1
#endif

static const char* gMode = "args";

static void CheckL(const char* aStep, TInt aActual, TInt aExpected)
    {
    if (aActual != aExpected)
        {
        RDebug::Printf("R1A:%s:%s:FAIL actual=%d expected=%d",
                       gMode, aStep, aActual, aExpected);
        User::Leave(aActual < 0 ? aActual : KErrGeneral);
        }
    }

static void Pass(const char* aStep)
    {
    RDebug::Printf("R1A:%s:%s:PASS", gMode, aStep);
    }

class CProbe : public CBase
    {
public:
    CProbe(TInt& aDestroyed) : iDestroyed(aDestroyed) {}
    ~CProbe() { ++iDestroyed; }
private:
    TInt& iDestroyed;
    };

static void LeaveProbeL(TInt& aDestroyed)
    {
    CProbe* probe = new(ELeave) CProbe(aDestroyed);
    CleanupStack::PushL(probe);
    User::Leave(KErrAbort);
    }

static void HeapL()
    {
    // Warm the cleanup-stack level before the allocation-count baseline.
    TInt destroyed = 0;
    TRAPD(warm, LeaveProbeL(destroyed));
    CheckL("warm-trap", warm, KErrAbort);
    destroyed = 0;
    const TInt before = User::CountAllocCells();
    TRAPD(left, LeaveProbeL(destroyed));
    CheckL("trap-code", left, KErrAbort);
    CheckL("leave-destructor", destroyed, 1);
    CProbe* probe = new(ELeave) CProbe(destroyed);
    CleanupStack::PushL(probe);
    CleanupStack::PopAndDestroy(probe);
    CheckL("normal-destructor", destroyed, 2);
    CheckL("allocation-balance", User::CountAllocCells(), before);
    Pass("heap-cbase-leave-trap");
    }

class CFixtureTimer : public CTimer
    {
public:
    static CFixtureTimer* NewLC()
        {
        CFixtureTimer* self = new(ELeave) CFixtureTimer;
        CleanupStack::PushL(self);
        self->ConstructL();
        CActiveScheduler::Add(self);
        return self;
        }
    ~CFixtureTimer() { Cancel(); }
    TInt iRuns;
    TInt iResult;
private:
    CFixtureTimer() : CTimer(EPriorityStandard), iRuns(0), iResult(KErrNotReady) {}
    void RunL()
        {
        ++iRuns;
        iResult = iStatus.Int();
        CActiveScheduler::Stop();
        }
    };

static void UninstallScheduler(TAny*)
    {
    CActiveScheduler::Install(NULL);
    }

static void SchedulerL()
    {
    CActiveScheduler* scheduler = new(ELeave) CActiveScheduler;
    CleanupStack::PushL(scheduler);
    CActiveScheduler::Install(scheduler);
    // Leaves do not unwind C++ automatic objects: explicitly uninstall on cleanup.
    CleanupStack::PushL(TCleanupItem(UninstallScheduler, NULL));
    CFixtureTimer* timer = CFixtureTimer::NewLC();
    timer->After(TTimeIntervalMicroSeconds32(10000));
    CActiveScheduler::Start();
    CheckL("timer-status", timer->iResult, KErrNone);
    CheckL("runl-count", timer->iRuns, 1);
    CleanupStack::PopAndDestroy(timer);
    CleanupStack::PopAndDestroy(); // UninstallScheduler
    CleanupStack::PopAndDestroy(scheduler);
    Pass("timer-runl");
    }

static void ConnectLC(RFs& aFs, TChar aDrive, TChar aDirectory)
    {
    CheckL("connect", aFs.Connect(), KErrNone);
    CleanupClosePushL(aFs);
    TBuf<32> path;
    path.Append(aDrive);
    path.Append(_L(":\\R1TEST\\"));
    path.Append(aDirectory);
    path.Append(_L("\\"));
    CheckL("session-path", aFs.SetSessionPath(path), KErrNone);
    TFileName actual;
    CheckL("session-path-get", aFs.SessionPath(actual), KErrNone);
    CheckL("session-path-value", actual.CompareF(path), 0);
    }

static void Pattern(TDes8& aData, TInt aOffset, TInt aLength)
    {
    aData.SetLength(aLength);
    for (TInt i = 0; i < aLength; ++i)
        aData[i] = static_cast<TUint8>((aOffset + i) % 251);
    }

static void CompareL(const TDesC8& aData, TInt aOffset, TInt aLength)
    {
    CheckL("read-length", aData.Length(), aLength);
    for (TInt i = 0; i < aLength; ++i)
        {
        if (aData[i] != static_cast<TUint8>((aOffset + i) % 251))
            {
            RDebug::Printf("R1A:%s:byte:FAIL offset=%d actual=%d expected=%d",
                           gMode, aOffset + i, aData[i], (aOffset + i) % 251);
            User::Leave(KErrCorrupt);
            }
        }
    }

static void VerifyL(RFs& aFs, const TDesC& aName, TInt aSize)
    {
    RFile file;
    CheckL("reopen", file.Open(aFs, aName, EFileRead | EFileShareReadersOnly), KErrNone);
    CleanupClosePushL(file);
    TInt size = -1;
    CheckL("size", file.Size(size), KErrNone);
    CheckL("size-value", size, aSize);
    TBuf8<1024> data;
    for (TInt offset = 0; offset < aSize; )
        {
        const TInt length = Min(1024, aSize - offset);
        CheckL("read", file.Read(data, length), KErrNone);
        CompareL(data, offset, length);
        offset += length;
        }
    data.SetLength(1);
    CheckL("eof", file.Read(data, 1), KErrNone);
    CheckL("eof-length", data.Length(), 0);
    CheckL("zero-read", file.Read(data, 0), KErrNone);
    CheckL("zero-length", data.Length(), 0);
    CheckL("capacity-read", file.Read(data, 1025), KErrOverflow);
    CleanupStack::PopAndDestroy(&file);
    RDebug::Printf("R1A:%s:verify:PASS size=%d", gMode, aSize);
    }

static void WriteL(RFs& aFs, const TDesC& aName, TInt aSize)
    {
    RFile file;
    // Create deliberately requires an absent leaf; never silently replace old evidence.
    CheckL("create", file.Create(aFs, aName, EFileWrite | EFileShareExclusive), KErrNone);
    CleanupClosePushL(file);
    TBuf8<1024> data;
    for (TInt offset = 0; offset < aSize; )
        {
        const TInt length = Min(1024, aSize - offset);
        Pattern(data, offset, length);
        CheckL("write", file.Write(data), KErrNone);
        offset += length;
        }
    CheckL("flush", file.Flush(), KErrNone);
    CleanupStack::PopAndDestroy(&file); // Close is void: not a durability result.
    VerifyL(aFs, aName, aSize);
    }

static const TInt KSizes[] = { 0, 1, 65535, 65536, 65537, 1048576 };

static void BoundaryFilesL(RFs& aFs, TBool aPersist, TBool aLarge)
    {
    const TInt count = aLarge ? 6 : 5;
    for (TInt i = 0; i < count; ++i)
        {
        TBuf<12> name;
        name.Copy(_L("DATA"));
        name.Append(TChar('0' + i));
        name.Append(_L(".BIN"));
        if (aPersist)
            VerifyL(aFs, name, KSizes[i]);
        else
            WriteL(aFs, name, KSizes[i]);
        }
    }

static void SemanticsL(RFs& aFs)
    {
    RFile file;
    CheckL("sem-create", file.Create(aFs, _L("SEEK.BIN"), EFileWrite | EFileShareAny), KErrNone);
    CleanupClosePushL(file);
    TBuf8<256> data;
    Pattern(data, 0, 200);
    CheckL("sem-write", file.Write(data), KErrNone);
    TInt pos = 210;
    CheckL("start", file.Seek(ESeekStart, pos), KErrNone);
    CheckL("start-value", pos, 210);
    Pattern(data, 200, 1);
    CheckL("append-no-hole", file.Write(data), KErrNone);
    TInt size = -1;
    CheckL("append-size", file.Size(size), KErrNone);
    CheckL("append-size-value", size, 201);
    Pattern(data, 201, 1);
    CheckL("positioned-append", file.Write(250, data), KErrNone);
    CheckL("positioned-size", file.Size(size), KErrNone);
    CheckL("positioned-size-value", size, 202);
    pos = 100;
    CheckL("end-clamp", file.Seek(ESeekEnd, pos), KErrNone);
    CheckL("end-value", pos, 202);
    pos = -300;
    CheckL("current-clamp", file.Seek(ESeekCurrent, pos), KErrNone);
    CheckL("current-value", pos, 0);
    CheckL("positioned-read", file.Read(10, data, 3), KErrNone);
    CompareL(data, 10, 3);
    CheckL("following-read", file.Read(data, 1), KErrNone);
    CompareL(data, 13, 1);
    pos = 0;
    CheckL("prefix-seek", file.Seek(ESeekStart, pos), KErrNone);
    CheckL("prefix-read", file.Read(data, 202), KErrNone);
    CompareL(data, 0, 202);

    pos = 150;
    CheckL("discriminator-seek", file.Seek(ESeekStart, pos), KErrNone);
    CheckL("shrink", file.SetSize(100), KErrNone);
    CheckL("shrink-size", file.Size(size), KErrNone);
    CheckL("shrink-value", size, 100);
    CheckL("regrow", file.SetSize(200), KErrNone);
    CheckL("regrow-size", file.Size(size), KErrNone);
    CheckL("regrow-value", size, 200);
    // Do not touch this share's position until discriminator read.
    RFile other;
    CheckL("other-open", other.Open(aFs, _L("SEEK.BIN"), EFileWrite | EFileShareAny), KErrNone);
    CleanupClosePushL(other);
    data.SetLength(1);
    data[0] = 0x31;
    CheckL("other-100", other.Write(100, data), KErrNone);
    data[0] = 0x72;
    CheckL("other-150", other.Write(150, data), KErrNone);
    CheckL("other-flush", other.Flush(), KErrNone);
    CleanupStack::PopAndDestroy(&other);
    CheckL("retained-position", file.Read(data, 1), KErrNone);
    CheckL("retained-position-length", data.Length(), 1);
    CheckL("retained-position-byte", data[0], 0x72);
    CheckL("retained-prefix", file.Read(0, data, 100), KErrNone);
    CompareL(data, 0, 100);
    // Async status and descriptor stay alive until completion; no cancellation/race claim.
    TRequestStatus status;
    file.Read(0, data, 16, status);
    User::WaitForRequest(status);
    CheckL("async-read", status.Int(), KErrNone);
    CompareL(data, 0, 16);
    CheckL("sem-flush", file.Flush(), KErrNone);
    CleanupStack::PopAndDestroy(&file);
    Pass("seek-resize-async-positive");
    }

static void MissingL(RFs& aFs)
    {
    RFile file;
    TInt result = file.Open(aFs, _L("ABSENT.BIN"), EFileRead);
    file.Close();
    CheckL("missing-leaf", result, KErrNotFound);
    result = file.Open(aFs, _L("NOEXIST\\ABSENT.BIN"), EFileRead);
    file.Close();
    CheckL("missing-parent", result, KErrPathNotFound);
    Pass("missing-errors");
    }

static void File64L(RFs& aFs)
    {
    RFile64 file;
    CheckL("64-create", file.Create(aFs, _L("SMALL64.BIN"), EFileWrite | EFileShareExclusive), KErrNone);
    CleanupClosePushL(file);
    TBuf8<256> data;
    Pattern(data, 0, 200);
    CheckL("64-write", file.Write(TInt64(0), data), KErrNone);
    CheckL("64-flush", file.Flush(), KErrNone);
    CleanupStack::PopAndDestroy(&file);
    CheckL("64-open", file.Open(aFs, _L("SMALL64.BIN"), EFileWrite | EFileShareExclusive), KErrNone);
    CleanupClosePushL(file);
    TInt64 size = -1;
    CheckL("64-size", file.Size(size), KErrNone);
    CheckL("64-size-value", size == TInt64(200), ETrue);
    CheckL("64-read", file.Read(TInt64(0), data, 200), KErrNone);
    CompareL(data, 0, 200);
    TInt64 pos = 200;
    CheckL("64-seek", file.Seek(ESeekStart, pos), KErrNone);
    CheckL("64-seek-value", pos == TInt64(200), ETrue);
    CheckL("64-eof", file.Read(data, 1), KErrNone);
    CheckL("64-eof-length", data.Length(), 0);
    CheckL("64-setsize", file.SetSize(TInt64(100)), KErrNone);
    CheckL("64-resize-size", file.Size(size), KErrNone);
    CheckL("64-resize-value", size == TInt64(100), ETrue);
    CheckL("64-prefix", file.Read(TInt64(0), data, 100), KErrNone);
    CompareL(data, 0, 100);
    CheckL("64-final-flush", file.Flush(), KErrNone);
    CleanupStack::PopAndDestroy(&file);
    Pass("small-rfile64");
    }

static void DispatchL()
    {
    CheckL("command-length", User::CommandLineLength() <= 64, ETrue);
    TBuf<64> command;
    User::CommandLine(command);
    command.Trim();
    if (command.Compare(_L("heap")) == 0)
        { gMode = "heap"; HeapL(); return; }
    if (command.Compare(_L("sched")) == 0)
        { gMode = "sched"; SchedulerL(); return; }
    // Exact grammar: file D / file1m D / persist D / persist1m D / rfile64 D.
    const TInt length = command.Length();
    CheckL("file-args-length", length >= 3, ETrue);
    CheckL("file-args-space", command[length - 2], ' ');
    TInt drive = command[length - 1];
    if (drive >= 'a' && drive <= 'z') drive -= 'a' - 'A';
    CheckL("drive-letter", drive >= 'A' && drive <= 'Z', ETrue);
    command.SetLength(length - 2);
    TBool persist = EFalse;
    TBool large = EFalse;
    TBool file64 = EFalse;
    if (command.Compare(_L("file")) == 0) gMode = "file";
    else if (command.Compare(_L("file1m")) == 0) { gMode = "file1m"; large = ETrue; }
    else if (command.Compare(_L("persist")) == 0) { gMode = "persist"; persist = ETrue; }
    else if (command.Compare(_L("persist1m")) == 0)
        { gMode = "persist1m"; persist = ETrue; large = ETrue; }
    else if (command.Compare(_L("rfile64")) == 0) { gMode = "rfile64"; file64 = ETrue; }
    else { CheckL("unknown-mode", KErrArgument, KErrNone); }
    RFs fs;
    ConnectLC(fs, TChar(drive), TChar(file64 ? 'B' : 'A'));
    if (file64) File64L(fs);
    else
        {
        BoundaryFilesL(fs, persist, large);
        if (!persist) { SemanticsL(fs); MissingL(fs); }
        }
    CleanupStack::PopAndDestroy(&fs);
    }

GLDEF_C TInt E32Main()
    {
    RDebug::Printf("R1A:entry:port-authored:BEGIN");
    CTrapCleanup* cleanup = CTrapCleanup::New();
    if (!cleanup)
        {
        RDebug::Printf("R1A:entry:cleanup:FAIL actual=%d", KErrNoMemory);
        return KErrNoMemory;
        }
    TRAPD(result, DispatchL());
    delete cleanup;
    if (result == KErrNone) Pass("done");
    else RDebug::Printf("R1A:%s:done:FAIL actual=%d", gMode, result);
    return result;
    }
