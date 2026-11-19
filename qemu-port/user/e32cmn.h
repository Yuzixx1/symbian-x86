#ifndef QPORT_USER_E32CMN_H
#define QPORT_USER_E32CMN_H

/*
 * e32cmn.h -- fundamental EUSER types for the freestanding S-04 port.
 *
 * C++98, -ffreestanding -fno-exceptions -fno-rtti.  Header-only: every
 * function is inline, and no dynamic (constructor-based) static
 * initialisation is used, so the port linker script needs no .init_array.
 *
 * Field names/ordering follow the genuine e32des16.h/e32des8.h layouts
 * (iTypeLength/iLength, iMaxLength, data pointer) closely enough for the
 * descriptor-shaped code in fs/r1_fs_server.cpp, but this is a port shim,
 * not a binary-compatible drop-in for a real EUSER.DLL.
 */

/* ------------------------------------------------------------------ types */
typedef int             TInt;
typedef unsigned int    TUint;
typedef signed char     TInt8;
typedef unsigned char   TUint8;
typedef short           TInt16;
typedef unsigned short  TUint16;
typedef int             TInt32;
typedef unsigned int    TUint32;
typedef long long       TInt64;
typedef unsigned long long TUint64;
typedef bool            TBool;
typedef void            TAny;

typedef TUint8          TText8;
typedef TUint16         TText16;
typedef TUint16         TText;

const TBool ETrue  = true;
const TBool EFalse = false;

/* ------------------------------------------------------------ error values */
/* The values below are fixed by the S-04 port ABI.  The remaining constants
 * are the genuine system-wide codes used by fs/r1_fs_server.cpp. */
const TInt KErrNone          =   0;
const TInt KErrNotFound      =  -1;
const TInt KErrGeneral       =  -2;
const TInt KErrNoMemory      =  -4;
const TInt KErrBadName       =  -5;
const TInt KErrArgument      =  -6;
const TInt KErrBadHandle     =  -8;
const TInt KErrBadDescriptor =  -9;
const TInt KErrOverflow      = -11;
const TInt KErrPathNotFound  = -12;
const TInt KErrInUse         = -14;
const TInt KErrNotReady      = -18;
const TInt KErrCorrupt       = -20;
const TInt KErrAccessDenied  = -21;
const TInt KErrNotSupported  = -49;
const TInt KRequestPending   = (TInt)0x80000000u;

/* Genuine EUSER thread priorities */
enum TThreadPriority {
    EPriorityNull      = -30,
    EPriorityMuchLess  = -20,
    EPriorityLess      = -10,
    EPriorityNormal    = 0,
    EPriorityMore      = 10,
    EPriorityMuchMore  = 20,
    EPriorityRealTime  = 30
};

/* ----------------------------------------------------------- 16-bit descs */
class TDesC16 {
public:
    TDesC16() : iLength(0), iPtr(0) {}
    TDesC16(TInt aLength, const TUint16* aPtr) : iLength(aLength), iPtr(aPtr) {}
    TInt Length() const { return iLength; }
    const TUint16* Ptr() const { return iPtr; }
    TInt Size() const { return iLength * (TInt)sizeof(TUint16); }
protected:
    void Set(const TUint16* aPtr, TInt aLength) { iPtr = aPtr; iLength = aLength; }
    TInt iLength;             /* number of 16-bit units (genuine iTypeLength) */
    const TUint16* iPtr;      /* data (genuine: inline or TPtrC16::iPtr)      */
};

class TDes16 : public TDesC16 {
public:
    TDes16() : iMaxLength(0) {}
    TInt MaxLength() const { return iMaxLength; }
    TInt MaxSize() const { return iMaxLength * (TInt)sizeof(TUint16); }
    TUint16* Ptr() { return (TUint16*)iPtr; }
    void SetLength(TInt aLength)
        { iLength = aLength < 0 ? 0 : (aLength > iMaxLength ? iMaxLength : aLength); }
    void Zero()
        {
        for (TInt i = 0; i < iMaxLength; ++i) ((TUint16*)iPtr)[i] = 0;
        iLength = 0;
        }
protected:
    TInt iMaxLength;
};

typedef TDesC16 TDesC;
typedef TDes16  TDes;

class TPtrC16 : public TDesC16 {
public:
    TPtrC16() {}
    TPtrC16(const TUint16* aPtr, TInt aLength) { Set(aPtr, aLength); }
    TPtrC16(const TUint16* aString)
        {
        TInt n = 0;
        while (aString && aString[n]) ++n;
        Set(aString, n);
        }
    TPtrC16(const TDesC16& aDes) { Set(aDes.Ptr(), aDes.Length()); }
    void Set(const TUint16* aPtr, TInt aLength) { TDesC16::Set(aPtr, aLength); }
    void Set(const TDesC16& aDes) { TDesC16::Set(aDes.Ptr(), aDes.Length()); }
};

template <TInt KMaxLength>
class TBuf16 : public TDes16 {
public:
    TBuf16() { iPtr = iBuf; iLength = 0; iMaxLength = KMaxLength; }
    TBuf16(const TDesC16& aDes) { iPtr = iBuf; iMaxLength = KMaxLength; Copy(aDes); }
    TUint16* Ptr() { return iBuf; }
    const TUint16* Ptr() const { return iBuf; }
    void Copy(const TDesC16& aDes)
        {
        TInt n = aDes.Length();
        if (n > KMaxLength) n = KMaxLength;
        const TUint16* src = aDes.Ptr();
        for (TInt i = 0; i < n; ++i) iBuf[i] = src[i];
        iLength = n;
        }
private:
    TUint16 iBuf[KMaxLength];
};

/* ----------------------------------------------------------- 8-bit descs */
class TDesC8 {
public:
    TDesC8() : iLength(0), iPtr(0) {}
    TDesC8(TInt aLength, const TUint8* aPtr) : iLength(aLength), iPtr(aPtr) {}
    TInt Length() const { return iLength; }
    const TUint8* Ptr() const { return iPtr; }
    TInt Size() const { return iLength; }
protected:
    void Set(const TUint8* aPtr, TInt aLength) { iPtr = aPtr; iLength = aLength; }
    TInt iLength;
    const TUint8* iPtr;
};

class TDes8 : public TDesC8 {
public:
    TDes8() : iMaxLength(0) {}
    TInt MaxLength() const { return iMaxLength; }
    TInt MaxSize() const { return iMaxLength; }
    TUint8* Ptr() { return (TUint8*)iPtr; }
    void SetLength(TInt aLength)
        { iLength = aLength < 0 ? 0 : (aLength > iMaxLength ? iMaxLength : aLength); }
    void Zero()
        {
        for (TInt i = 0; i < iMaxLength; ++i) ((TUint8*)iPtr)[i] = 0;
        iLength = 0;
        }
protected:
    TInt iMaxLength;
};

class TPtrC8 : public TDesC8 {
public:
    TPtrC8() {}
    TPtrC8(const TUint8* aPtr, TInt aLength) { Set(aPtr, aLength); }
    TPtrC8(const TUint8* aString)
        {
        TInt n = 0;
        while (aString && aString[n]) ++n;
        Set(aString, n);
        }
    TPtrC8(const TDesC8& aDes) { Set(aDes.Ptr(), aDes.Length()); }
    void Set(const TUint8* aPtr, TInt aLength) { TDesC8::Set(aPtr, aLength); }
    void Set(const TDesC8& aDes) { TDesC8::Set(aDes.Ptr(), aDes.Length()); }
};

template <TInt KMaxLength>
class TBuf8 : public TDes8 {
public:
    TBuf8() { iPtr = iBuf; iLength = 0; iMaxLength = KMaxLength; }
    TUint8* Ptr() { return iBuf; }
    const TUint8* Ptr() const { return iBuf; }
private:
    TUint8 iBuf[KMaxLength];
};

/* In a Unicode (16-bit) build the unsuffixed spellings are the 16-bit ones. */
template <TInt KMaxLength>
class TBuf : public TBuf16<KMaxLength> {
public:
    TBuf() {}
    TBuf(const TDesC16& aDes) : TBuf16<KMaxLength>(aDes) {}
};

/* ------------------------------------------------------ package wrappers */
/* A package exposes the raw bytes of T as a descriptor.  TPckgC is the
 * read-only form; TPckg is the writable form used as an RMessage2::Read
 * target.  The default TPckg/TPckgC are 8-bit, as in genuine EUSER. */
template <class T>
class TPckgC8 : public TDesC8 {
public:
    TPckgC8(const T& aValue)
        { Set((const TUint8*)&aValue, (TInt)sizeof(T)); }
    const T& operator()() const { return *(const T*)iPtr; }
};

template <class T>
class TPckg8 : public TDes8 {
public:
    TPckg8(T& aValue)
        {
        iPtr = (const TUint8*)&aValue;
        iLength = (TInt)sizeof(T);
        iMaxLength = (TInt)sizeof(T);
        }
    TUint8* Ptr() { return (TUint8*)iPtr; }
    T& operator()() { return *(T*)iPtr; }
};

template <class T>
class TPckgC16 : public TDesC16 {
public:
    TPckgC16(const T& aValue)
        { Set((const TUint16*)&aValue, (TInt)((sizeof(T) + 1) / 2)); }
    const T& operator()() const { return *(const T*)iPtr; }
};

template <class T>
class TPckg16 : public TDes16 {
public:
    TPckg16(T& aValue)
        {
        iPtr = (const TUint16*)&aValue;
        iLength = (TInt)((sizeof(T) + 1) / 2);
        iMaxLength = iLength;
        }
    TUint16* Ptr() { return (TUint16*)iPtr; }
    T& operator()() { return *(T*)iPtr; }
};

template <class T> class TPckgC : public TPckgC8<T> {
public:
    TPckgC(const T& aValue) : TPckgC8<T>(aValue) {}
};
template <class T> class TPckg : public TPckg8<T> {
public:
    TPckg(T& aValue) : TPckg8<T>(aValue) {}
};

/* ------------------------------------------------------------- TVersion */
class TVersion {
public:
    TInt iMajor;
    TInt iMinor;
    TInt iBuild;
    TVersion() : iMajor(0), iMinor(0), iBuild(0) {}
    TVersion(TInt aMajor, TInt aMinor, TInt aBuild)
        : iMajor(aMajor), iMinor(aMinor), iBuild(aBuild) {}
    TBool operator==(const TVersion& aOther) const
        { return iMajor == aOther.iMajor && iMinor == aOther.iMinor && iBuild == aOther.iBuild; }
    TBool operator!=(const TVersion& aOther) const { return !(*this == aOther); }
};

/* -------------------------------------------------------- TSecureId/TUid */
class TUid {
public:
    TUint32 iId;
    TUid() : iId(0) {}
    explicit TUid(TUint32 aId) : iId(aId) {}
    TBool operator==(const TUid& aOther) const { return iId == aOther.iId; }
};

class TSecureId {
public:
    TUint32 iId;
    TSecureId() : iId(0) {}
    explicit TSecureId(TUint32 aId) : iId(aId) {}
    explicit TSecureId(TUid aUid) : iId(aUid.iId) {}
    TBool operator==(const TSecureId& aOther) const { return iId == aOther.iId; }
};

/* ------------------------------------------------------------------ _LIT
 * Genuine _LIT uses a TLitC aggregate; the stock hosted compiler cannot
 * seed a TUint16[] from a narrow string literal, so the units are generated
 * a character at a time.  The result is an aggregate with constant
 * initialisation (no global constructor) that converts to a TPtrC16 view.
 * Literals longer than 31 ASCII characters are truncated. */
template <TInt N>
class TLitC16 {
public:
    TInt iLength;
    TUint16 iBuf[N];
    TInt Length() const { return iLength; }
    const TUint16* Ptr() const { return iBuf; }
    operator TPtrC16() const { return TPtrC16(iBuf, iLength); }
};

#define _LIT_CH(s,i) ((TUint16)(((TInt)sizeof(s) > (TInt)(i)) ? (TUint8)((s)[i]) : (TUint8)0))
#define _LIT_BUF32(s) { _LIT_CH(s,0), _LIT_CH(s,1), _LIT_CH(s,2), _LIT_CH(s,3), _LIT_CH(s,4), _LIT_CH(s,5), _LIT_CH(s,6), _LIT_CH(s,7), _LIT_CH(s,8), _LIT_CH(s,9), _LIT_CH(s,10), _LIT_CH(s,11), _LIT_CH(s,12), _LIT_CH(s,13), _LIT_CH(s,14), _LIT_CH(s,15), _LIT_CH(s,16), _LIT_CH(s,17), _LIT_CH(s,18), _LIT_CH(s,19), _LIT_CH(s,20), _LIT_CH(s,21), _LIT_CH(s,22), _LIT_CH(s,23), _LIT_CH(s,24), _LIT_CH(s,25), _LIT_CH(s,26), _LIT_CH(s,27), _LIT_CH(s,28), _LIT_CH(s,29), _LIT_CH(s,30), _LIT_CH(s,31) }
#define _LIT(name,s) \
    static const TLitC16<32> name = { (TInt)(sizeof(s) - 1), _LIT_BUF32(s) }

#endif /* QPORT_USER_E32CMN_H */
