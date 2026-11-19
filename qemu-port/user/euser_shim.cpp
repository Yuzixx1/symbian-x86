/*
 * euser_shim.cpp — static member definitions for EUSER/CBase shim.
 * C++98, -ffreestanding -fno-exceptions -fno-rtti.
 */
#include "e32base.h"
#include "e32cmn.h"
#include "ipc.h"

/* Static member definitions */
CActiveScheduler* CActiveScheduler::iScheduler = nullptr;
CActive* CActiveScheduler::iActiveQ = nullptr;
CBase* CleanupStack::iStack = nullptr;
TInt User::iTlsIndex = 0;

/* RTTI typeinfo objects needed for -frtti with virtual classes.
   The compiler references these for classes with virtual functions. */
void* __ZTVN10__cxxabiv117__class_type_infoE[] = {
    0, 0, 0, 0, 0, 0
};
void* __ZTVN10__cxxabiv120__si_class_type_infoE[] = {
    0, 0, 0, 0, 0, 0
};
char __ZTI10__cxxabiv117__class_type_info[] = { 0 };
char __ZTI10__cxxabiv120__si_class_type_info[] = { 0 };

// Typeinfo objects for user classes
char __ZTI7CBase[] = { 0 };
char __ZTI7CActive[] = { 0 };
char __ZTI14CActiveScheduler[] = { 0 };
char __ZTI9CServer2[] = { 0 };
char __ZTI9CSession2[] = { 0 };
char __ZTI9RMessage2[] = { 0 };

// Vtables for user classes
void* __ZTV7CBase[] = { 0, 0, 0, 0, 0, 0 };
void* __ZTV7CActive[] = { 0, 0, 0, 0, 0, 0 };
void* __ZTV14CActiveScheduler[] = { 0, 0, 0, 0, 0, 0 };
void* __ZTV9CServer2[] = { 0, 0, 0, 0, 0, 0 };
void* __ZTV9CSession2[] = { 0, 0, 0, 0, 0, 0 };
void* __ZTV9RMessage2[] = { 0, 0, 0, 0, 0, 0 };