// gcc_prefix.h — GCC replacement for MSVC-isms in genuine x86 EKA2 headers.
//
// Background: genuine x86 EKA2 was built with MSVC (`__VC32__`); the headers
// only define the macros below for `__VC32__` or (`__GCC32__` && `__MARM__`).
// This prefix supplies the GCC/x86 equivalents. It is BUILD GLUE (what MSVC's
// headers + bldmake provided implicitly), not kernel logic: no Symbian types,
// functions, or algorithms are defined or altered here.
//
// Force-included via `-include gcc_prefix.h` (like a compiler prefix file).
// Must come before ANY Symbian header. Keep it strictly to macros the
// genuine headers reference but never define on the __X86__+__GCC32__ path.

#ifndef __QEMU_GCC_PREFIX_H__
#define __QEMU_GCC_PREFIX_H__

// e32def.h: defined for __VC32__ and (__GCC32__ && __MARM__), missing for
// __GCC32__-on-x86. Empty: freestanding static kernel, no exceptions/DLLs.
#ifndef __NO_THROW
#define __NO_THROW
#endif
#ifndef __THROW
#define __THROW(t)
#endif
#ifndef IMPORT_C
#define IMPORT_C
#endif
#ifndef EXPORT_C
#define EXPORT_C
#endif
#ifndef IMPORT_D
#define IMPORT_D
#endif
#ifndef EXPORT_D
#define EXPORT_D
#endif
#ifndef NONSHARABLE_CLASS
#define NONSHARABLE_CLASS(x) class x
#endif
#ifndef NONSHARABLE_STRUCT
#define NONSHARABLE_STRUCT(x) struct x
#endif

// MSVC __declspec(...) used directly in a few headers on non-GCC paths.
// Empty: static linkage absorbs dllexport/naked/selectany here.
#ifndef __declspec
#define __declspec(x)
#endif

// MSVC _MSC_VER version gates in headers: pretend a new-enough MSVC so
// version-gated (non-compiler-specific) declarations stay visible, while
// every _MSC_VER-keyed CODE path still loses to the __GCC32__ branches.
#ifndef _MSC_VER
#define _MSC_VER 1300
#endif

// Integer types MSVC provides as keywords; e32def.h only typedefs these
// under __VC32__/__CW32__ branches.
#ifndef __int64
typedef long long __int64;
#endif
#ifndef __int32
typedef int __int32;
#endif
#ifndef __int16
typedef short __int16;
#endif
#ifndef __int8
typedef signed char __int8;
#endif

#endif // __QEMU_GCC_PREFIX_H__
