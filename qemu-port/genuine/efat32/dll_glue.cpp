// Port glue for the genuine F32 DLL/FSY E32 images (build_fsy_dll.py).
// No kernelhwsrv/ source is modified: this only supplies the freestanding
// C++/libc helpers the genuine objects expect, plus a documented export
// fallback target.  Genuine logic is entirely in kernelhwsrv/ objects.

#include <stddef.h>

// A tiny bump allocator for operator new/delete.  The F32 client objects
// allocate descriptors/arrays; a static heap keeps the DLL self-contained
// (no EUSER import needed for the allocator itself).
static char sHeap[64 * 1024];
static size_t sUsed = 0;

static void* qalloc(size_t n)
    {
    n = (n + 7u) & ~size_t(7u);
    if (!n || sUsed + n > sizeof(sHeap)) return 0;
    void* p = &sHeap[sUsed];
    sUsed += n;
    return p;
    }

void* operator new(unsigned int n) { return qalloc(n); }
void* operator new[](unsigned int n) { return qalloc(n); }
void operator delete(void*) {}
void operator delete[](void*) {}
void operator delete(void*, unsigned int) {}
void operator delete[](void*, unsigned int) {}

// Freestanding libc helpers used by the genuine sources / gcc.
extern "C" void* memcpy(void* d, const void* s, size_t n)
    {
    unsigned char* a = (unsigned char*)d;
    const unsigned char* b = (const unsigned char*)s;
    while (n--) *a++ = *b++;
    return d;
    }
extern "C" void* memmove(void* d, const void* s, size_t n)
    {
    unsigned char* a = (unsigned char*)d;
    const unsigned char* b = (const unsigned char*)s;
    if (a < b) { while (n--) *a++ = *b++; }
    else { a += n; b += n; while (n--) *--a = *--b; }
    return d;
    }
extern "C" void* memset(void* d, int c, size_t n)
    {
    unsigned char* a = (unsigned char*)d;
    while (n--) *a++ = (unsigned char)c;
    return d;
    }
extern "C" int memcmp(const void* a, const void* b, size_t n)
    {
    const unsigned char* x = (const unsigned char*)a;
    const unsigned char* y = (const unsigned char*)b;
    while (n--) { if (*x != *y) return *x - *y; ++x; ++y; }
    return 0;
    }
extern "C" void memclr(void* d, size_t n) { memset(d, 0, n); }
extern "C" int memcompare(const void* a, const void* b, size_t n) { return memcmp(a, b, n); }

// No C++ runtime in the port: pure-virtual calls and static destructors never
// run in these library images, but the ABI symbols must still be present.
extern "C" void __cxa_pure_virtual() {}
extern "C" int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }
extern "C" void __cxa_finalize(void*) {}

// Documented fallback target for .def export ordinals whose symbol is not
// emitted by this -fno-rtti GCC build (RTTI _ZTI/_ZTV slots in efsrvu.def).
// Keeps the ordinal layout intact; real exported symbols use their own code.
extern "C" void efat32_export_stub() {}
