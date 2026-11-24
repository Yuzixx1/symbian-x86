/*
 * euser_local.cpp -- minimal LOCAL implementations of the handful of EUSER
 * exports referenced by the genuine F32 translation units we link into the
 * probe (genuine/obj/F32/filesystem_utils.o and sl_bpb32.o).
 *
 * These are not replacements for the genuine EUSER.DLL: they only implement
 * the descriptor/memory primitives needed by two small F32 files so that the
 * genuine F32 BYTE CODE can execute in our freestanding PE2 E32 process.  The
 * descriptor layouts and class names are taken from the genuine SDK headers
 * (epoc32/include/e32des{8,16}.h) and kernel/eka/bx86gcc/euseru.def ordinals:
 *   1032 TPtrC8::TPtrC8(const TUint8*)
 *   1033 TPtrC8::TPtrC8(const TUint8*,TInt)
 *   1718 TDesC8::At() const
 *   1719 TDesC8::Ptr() const
 *   1951 memclr, 1954 memmove, 756 TDes8::FillZ, 782 TDes8::SetLength
 * Wide variants for filesystem_utils.cpp compiled with -D_UNICODE:
 *   TPtrC16::TPtrC16(const TUint16*,TInt), TDesC16::At/Ptr.
 */
#include <stdint.h>

struct GDesC8 { uint32_t iTypeLength; const uint8_t* iPtr; };
struct GDes8  { uint32_t iTypeLength; uint32_t iMaxLength; uint8_t* iPtr; };
struct GDesC16 { uint32_t iTypeLength; const uint16_t* iPtr; };

extern "C" {

void* memclr(void* dst, unsigned n) {
    uint8_t* p = (uint8_t*)dst;
    while (n--) *p++ = 0;
    return dst;
}
void* memmove(void* dst, const void* src, unsigned n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) { while (n--) *d++ = *s++; }
    else { d += n; s += n; while (n--) *--d = *--s; }
    return dst;
}
void* memcpy(void* dst, const void* src, unsigned n) {
    return memmove(dst, src, n);
}
void* memset(void* dst, int c, unsigned n) {
    uint8_t* p = (uint8_t*)dst;
    while (n--) *p++ = (uint8_t)c;
    return dst;
}

/* Genuine iTypeLength: low 28 bits are the length (KMaskDesLength8=0x0fffffff),
   high nibble holds the 8/16-bit type flags -- NOT a shifted length. */
void _ZN6TPtrC8C1EPKh(GDesC8* self, const uint8_t* s) {
    unsigned n = 0;
    while (s[n]) ++n;
    self->iTypeLength = n;
    self->iPtr = s;
}
void _ZN6TPtrC8C1EPKhi(GDesC8* self, const uint8_t* s, int n) {
    self->iTypeLength = (unsigned)n;
    self->iPtr = s;
}
/* Genuine TDesC8::AtC returns const TUint8& (pointer in EAX), not a value. */
const uint8_t* _ZNK6TDesC83AtCEi(const GDesC8* self, int i) {
    return &self->iPtr[i >= 0 ? i : 0];
}
const uint8_t* _ZNK6TDesC83PtrEv(const GDesC8* self) { return self->iPtr; }

void _ZN7TPtrC16C1EPKti(GDesC16* self, const uint16_t* s, int n) {
    self->iTypeLength = (unsigned)n | 0x80000000u;  /* 16-bit type flag */
    self->iPtr = s;
}
const uint16_t* _ZNK7TDesC163AtCEi(const GDesC16* self, int i) {
    return &self->iPtr[i >= 0 ? i : 0];
}
const uint16_t* _ZNK7TDesC163PtrEv(const GDesC16* self) { return self->iPtr; }

void _ZN5TDes85FillZEv(GDes8* self) {
    unsigned len = self->iTypeLength & 0x0fffffffu;
    for (unsigned i = 0; i < len; ++i) self->iPtr[i] = 0;
    self->iTypeLength &= ~0x0fffffffu;
}
void _ZN5TDes89SetLengthEi(GDes8* self, int n) {
    self->iTypeLength = (self->iTypeLength & ~0x0fffffffu) | ((unsigned)n & 0x0fffffffu);
}

}
