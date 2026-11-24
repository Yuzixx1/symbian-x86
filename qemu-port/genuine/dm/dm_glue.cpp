/*
 * dm_glue.cpp -- port glue + entry for building the GENUINE domain manager
 * server (domainsrv.cpp + domaincfg.cpp) as an E32 server image on the
 * ported x86 kernel.
 *
 * kernelhwsrv/ is untouched; this file lives in qemu-port/genuine/dm/.
 *
 * The genuine server links euser.lib + efsrv.lib. Our elf2e32.py pipeline
 * supports exactly ONE EUSER.DLL import (ordinal 915, RDebug::Printf, bound
 * through the PE2 IAT slot `dm_printf_slot`); every other EUSER/EFSRV symbol
 * referenced by domainsrv.cpp is satisfied by the generated dm_stubs.S
 * (one `ret`-style stub per symbol, see gen_stubs.py). Those stubs are only
 * link-time fillers for this smoke image -- the full server bring-up path
 * (CServer2::Start, CActiveScheduler::Start, RMessagePtr2, RFs, ...) is what
 * the parallel efsrv/ewsrv multi-import pipeline (pack_multi.py thunks)
 * will bind to the real EUSER.DLL/EFSRV.DLL exports.
 *
 * app_main() (called by the port's user/start.S) prints load markers through
 * the EUSER IAT slot and exercises genuine domaincfg.cpp code
 * (TTransitionConfig::CheckValues) before exiting cleanly.
 */
#include <e32std.h>
#include <u32std.h>
#include <e32debug.h>
#include <domainpolicy.h>
#include <domaindefs.h>
#include "domaincfg.h"

/* PE2 IAT slot at 0x400000+iat_off (ld --defsym in genuine_build.py). */
extern void (*dm_printf_slot)(const char*, ...);

#define dmarker(...) do { \
    unsigned fn_ = *(unsigned*)(void*)&dm_printf_slot; \
    if (fn_ && fn_ != 915u) \
        ((void (*)(const char*, ...))fn_)(__VA_ARGS__); \
} while (0)

extern "C" int app_main(void)
{
    dmarker("DM: genuine domainsrv.cpp+domaincfg.cpp loaded (DOMAINSRV server image, x86 port)\r\n");

    /* Exercise genuine domaincfg.cpp: TTransitionConfig from a state spec. */
    SDmStateSpecV1 spec;
    spec.iState = 1;
    spec.iTimeoutMs = 200;
    spec.iDeferralLimit = 3;
    spec.iFailurePolicy = 0;
    TTransitionConfig good(spec);
    TInt r = good.CheckValues();
    dmarker("DM: TTransitionConfig(spec).CheckValues -> r=%d (KErrNone=%d)\r\n", (int)r, (int)KErrNone);

    SDmStateSpecV1 bad = spec;
    bad.iTimeoutMs = -1;                    /* negative value -> KDmErrBadDomainSpec */
    TTransitionConfig badCfg(bad);
    r = badCfg.CheckValues();
    dmarker("DM: TTransitionConfig(bad).CheckValues -> r=%d (KDmErrBadDomainSpec=%d)\r\n",
           (int)r, (int)KDmErrBadDomainSpec);

    TTransitionConfig byState(2);           /* key-lookup constructor */
    dmarker("DM: TTransitionConfig(TDmDomainState=2) constructed, iState=%d\r\n", (int)byState.iState);

    dmarker("DM SMOKE: done, exiting cleanly\r\n");
    return 0;
}
