# EWSRV_INPUT_INJECTION — PS/2 ISRs → genuine EWSRV raw event queue

**Status:** implemented + verified 2026-11-24. Closes the input half of the
`docs/WINSRV_MIGRATION_PLAN.md` §5 blocker ("input injection into genuine EWSRV
is the biggest open item"). `kernelhwsrv/` untouched; no commit/push; no
`make clean`.

## Design

Genuine EWSRV receives input through the EKA2 kernel event queue, exactly like
on real hardware: a keyboard/pointer driver builds a `TRawEvent` and calls the
public `Kern::AddEvent` API; `K::TryDeliverEvent` (eventq.cpp:134) hands it to
the pending `K::EventRequest` of the event-hook owner (`K::EventThread` —
genuine EWSRV, captured via `ExecHandler::CaptureEventHook`); EWSRV's
`CEvent::RunL` (ws_main.cpp:601) then dispatches it — `EKeyDown/EKeyUp` →
`iEvent.Event().ScanCode()` → `KeyTranslator->TranslateKey` →
`CWsWindow::KeyPress` (ws_main.cpp:654-670); `EPointerMove` → `Pos()` →
`CWsWindow::MouseMove` (ws_main.cpp:643).

`Kern::AddEvent` has hard preconditions (eventq.cpp:98-103: thread context,
CS held, kernel unlocked, interrupts enabled, no fast mutex) — **never
satisfiable from ISR context** (IRQ entry runs with IF=0 on the IRQ stack).
The genuine interrupt-driven keyboard driver solves this the same way we do
(bsptemplate `keyboard_interrupt.cpp`): **ISR queues a DFC; the DFC calls
`Kern::AddEvent`**. The port bridge mirrors that:

1. **ISR (IRQ1, `variant/qemu_variant_int.cpp` `QemuKbdIsr`, line ~579-586):**
   each PS/2 set-1 scancode (make/break, incl. `0xE0`-extended keys) is
   converted by `KbdStdScancode()` (line 363) to a genuine `TStdScanCode`
   (`<e32keys.h>` `EStdKey*` / ASCII for letters+digits, same convention as the
   genuine bsptemplate `convertCode` tables — no magic numbers) and pushed as
   `TRawEvent(EKeyDown/EKeyUp, stdScanCode)` into a 16-entry SPSC ring, then
   `TDfc::Add()` queues the drain DFC. Ring full → drop + counter (never block
   in ISR).
2. **ISR (IRQ12, `variant/qemu_mouse.cpp` `MouseByte`, lines 157-190):** each
   decoded packet becomes `EPointerMove(x,y)` plus `EButton1/2/3Down/Up` for
   click-bit transitions (PS/2 bit 1/2/4 → EButton1/2/3).
3. **DFC (`KbdInjDfcFn`, qemu_variant_int.cpp:314) on `Kern::DfcQue0()`'s
   thread:** drains the ring and, per event, wraps the public API exactly like
   the genuine `ExecHandler::AddEvent` (eventq.cpp:67-69):
   `NKern::ThreadEnterCS(); Kern::AddEvent(ev); NKern::ThreadLeaveCS();`
   (lines 325-327) and logs `KBINJ: <type> ... add=%d consumed=%d
   evtThread=%x head=%x tail=%x`. `consumed=1` means `TryDeliverEvent`
   completed EWSRV's pending request — i.e. the event reached genuine EWSRV.
4. **Event construction** uses a local `TPortRawEvent : public TRawEvent`
   subclass that fills the protected fields through the genuine `<e32event.h>`
   layout (iType/iTip/iPointerNumber/iDeviceNumber/iTicks/iU) — the same trick
   as the proven pointer bridge `TPointerRawEvent` in `qemu_display.cpp`; no
   magic offsets, no `#define protected public`, no kernel patch.

**Opt-in gate:** `\KBINJ.TXT` on the boot FAT volume, resolved once by
`QemuKbdInjArm()` (qemu_variant_int.cpp:500) from `QemuKbdArm()` — first at
`Asic::Init3` (before `FatMount`, gate stays unresolved) then at shell time.
Content selects the source mask: `"ptr"` → pointer only (1), `"key"` → keyboard
only (2), anything else/presence → both (3). Absent file → `gate OFF mask=0`
and every boot is byte-for-byte on the old ws-ring path (verified: ewsrv /
smoke / pointer logs show `gate OFF mask=0` and still PASS). This mirrors the
`\PTRINJ.TXT` one-shot boot pointer bridge; the two coexist (PTRINJ = boot-time
one-shot, KBINJ = live ISR path).

## Files

| file | change |
|---|---|
| `variant/qemu_variant.h:253-280` | bridge API decls (input-side): `QemuKbdInjArm`, `QemuInjGateActive`, `QemuInjGateMask`, `QemuEwsrvPushKey/PointerMove/Button`, `QemuInjStats` |
| `variant/qemu_variant_int.cpp:232-530` | ring + `TPortRawEvent` + `KbdInjDfcFn` + `KbdStdScancode` tables + push/arm/gate |
| `variant/qemu_variant_int.cpp:579-586` | `QemuKbdIsr`: scancode → `QemuEwsrvPushKey(make/break, std)` (E0 keys included) |
| `variant/qemu_variant_int.cpp:686` | `QemuKbdArm()` calls `QemuKbdInjArm()` |
| `variant/qemu_mouse.cpp:157-190` | `MouseByte`: pointer/button events when gate bit0 set |
| `tools/r1_genuine_input_test.py` | new test (two boots, see below). **OFF-ROSTER**: not in `tools/r1_final_verify.py` `DEFAULT_TESTS`, and it still stages the retired portfilesrv shim from `~/.cache/sybian-r1/r1-portfilesrv-flat-v1/output/portfilesrv` (constant `:59`, staged `:114`, asserted in the FAT `:135`). It is evidence-only and is not a gate row. |
| `docs/EWSRV_INPUT_INJECTION.md` | this report |

## Evidence (QEMU, `python3 tools/r1_genuine_input_test.py` → **PASS**; off-roster, not a gate row)

Boot A (`\KBINJ.TXT` = `ptr`), log `/tmp/opencode/genuine_input.log`:
```
KBINJ: genuine EWSRV input bridge gate ON mask=1 (\KBINJ.TXT r=0 got=4; 1=ptr 2=key)
KBINJ: EPointerMove aux=34210186 add=0 consumed=1 evtThread=1031080 head=1018480 tail=1018480
MOUSE TEST: injected 3 AUX bytes, packets 0 -> 1 x=522 y=394 isr=3 OK
```
+ all 6 EWSRV boot markers, all 9 EWCLIENT window markers (launched from the
shell while the gate was ON), **no Exc/PanicKernExec/FAULT/MMU #PF/Abort**,
screendump renders the client window (>1 colour). So the real 8042 → IRQ12
ISR → `TRawEvent(EPointerMove)` → `Kern::AddEvent` → genuine EWSRV path works
end-to-end with zero faults and a healthy window server + client.

Boot B (`\KBINJ.TXT` = `key`), log `/tmp/opencode/genuine_input.keyboot.log`:
```
KBINJ: genuine EWSRV input bridge gate ON mask=2 ...
KBINJ: EKeyDown std=0x41 add=0 consumed=1 evtThread=1031080 head=1018480 tail=1018480
PD: #PF pid=9 cr2=0xf0000010 err=0x05 user=1 inUserRange=0 cr3=0x45e000
FAULT: KERN 0x00000008 (8)
```
QMP/HMP `sendkey a` (PS/2 make 0x1E / break 0x9E) → IRQ1 ISR →
`TRawEvent(EKeyDown, 'A'=0x41)` → `Kern::AddEvent` add=0 → **consumed=1 by
genuine EWSRV** (`evtThread=0x1031080`, its DThread) — the kernel event queue
handed the key to EWSRV's pending `UserSvr::RequestEvent`, which is exactly
"genuine EWSRV receives the input event".

The two lines `PD: #PF ... cr2=0xf0000010` / `FAULT: KERN 0x00000008` in that
log are the **pre-fix** (19:51) EWSRV-side keymap fault, fixed the same day —
see the keymap section below. The post-fix re-run
(`/tmp/opencode/ewsrv_kytran_input_test.log` → `RESULT: PASS`; post-fix
`genuine_input.keyboot.log`) shows the same `EKeyDown std=0x41 consumed=1` with
**0 occurrences of `KERN 8` / `#PF` / `cr2=0xf0000010`** and a clean shutdown.

Regressions (all PASS after the change): `tools/r1_qemu_smoke.py`,
`tools/r1_genuine_ewsrv_test.py --no-build`, `tools/r1_genuine_pointer_test.py
--no-build` (PTRBRIDGE `consumed=1` intact), plus the new input test. All
non-KBINJ boots log `gate OFF mask=0`.

## Keymap (RESOLVED — was a blocker, no longer one)

Historical: in the first keyboard boot the `EKeyDown` was consumed and genuine
EWSRV then faulted **inside its own key translator** — `CKeyTranslatorX` was
created by `CKeyTranslator::New()` whose port patch then returned `KErrNone`
from `Initialise()` *without loading EKDATA.DLL and without initializing*
`iConvTable` / `iFuncTables` (genuine `k32keys.h:449-473` POD members, heap
garbage — observed `KeyTranslator=02202d18`, fault at `cr2=0xf0000010`).
`TranslateKey` → `TConvTable::Convert` / `TFuncTable::getDefault` walked those
garbage pointers (`pentries[0]` at NULL if zeroed) → user-mode #PF in pid 9
(EWSRV) → `K::PanicKernExec` (pd.cpp:281-289, user faults are machine-fatal in
this port). Pointer events never touched the translator (`EPointerMove` →
`Pos()` → `MouseMove`), which is why boot A was fault-free even then.

**Fixed and verified 2026-11-24 — the blocker no longer stands.**
`genuine/ewsrv/patched/ky_tran.cpp:85` (the `CKeyTranslatorX` constructor)
installs a built-in keymap: identity scan-code table over 0x00-0xff,
edit/modifier scan codes mapped to genuine `EKey*`, `TConvTable` /
`TFuncTable` / `TCtrlDigits` zero-initialised with valid `pentries`
(`gPortDefaultEntry`, `gPortGenFuncTables[13]`), `iIsdefaultKeyData=ETrue`; and
`Initialise()` returns `KErrNone` (`ky_tran.cpp:100-109`) without EKDATA.DLL.
The genuine table-walking code is unchanged. Post-fix boot B shows
`EKeyDown std=0x41 consumed=1` with zero `KERN 8` / `#PF` / `cr2=0xf0000010`
occurrences and a clean shutdown. Full evidence: `docs/KEYMAP_FIX_STATUS.md:1`.

So both gate settings are usable: `"key"`/`"all"` injection no longer kills
EWSRV, and `"ptr"` remains the production-safe default. Remaining
(non-blocking) gap: the keymap is a minimal identity map — no EKDATA layouts,
no IME or locale layouts. Full `TWsEvent` delivery to a focus window still
benefits from a real keymap (EKDATA) or a raw-event-mode window
(`CWsWindow::QueueRawEvent`, ws_win.cpp:2135, sets `iKeyCode=ScanCode()`
without the translator).

## Blockers / next steps

- ~~EWSRV keymap tables~~ — **resolved** (above): the first key no longer
  kills EWSRV. The lingering non-blocking limitation is the minimal identity
  keymap.
- An event-reading EWCLIENT (genuine/ewsrv owner) would close the loop to
  `RWsSession::GetEvent`/`TWsEvent` in a client; the kernel→EWSRV hop is done.
- After winsrv retirement, `wsrv_publish_*` calls in the ISRs become dead and
  can be deleted with the shim (migration plan §4 deletion set).

**2026-11-26 — correction (supersedes this file's live/pending winsrv-or-GUI premise):** winsrv/GUI desktop was RETIRED in commit `94ce9c9` (7 files deleted: `winsrv.c`, `guishost.h`, `wsclient.c`, `wsclient.h`, `qemu_windowsrv.cpp`, `qemu_windowsrv.h`, `qemu_gui_exec.cpp`); the winsrv slow execs 249/250/251 and the WINSRV launch path are gone. The console shell plus DOOM/Freedoom are the UI; genuine EWSRV is the target whenever a GUI returns, per `AGENTS.md`. Everything above that describes the ws-ring path as live or the winsrv retirement as pending is historical only — the KBINJ pointer path (`\KBINJ.TXT` = `ptr`) remains the production-safe evidence.

**2026-11-26 — gate count and keymap status corrected (authoritative):** the authoritative gate is **15/15 PASS at HEAD `ee8490e`, `FINAL27_RC=0`** (`/tmp/opencode/final27_gate.log` → `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`), not the earlier 13-row 13/13 figure. The EWSRV keymap blocker previously recorded in this file is **resolved**: `CKeyTranslatorX` installs its built-in keymap at `genuine/ewsrv/patched/ky_tran.cpp:85` and `Initialise()` returns `KErrNone` (`:100-109`), so the old "key tables are heap garbage / first key kills EWSRV" claim no longer holds — see `docs/KEYMAP_FIX_STATUS.md:1`. Explicitly: `tools/r1_genuine_input_test.py` is **OFF-ROSTER** — it is not in `tools/r1_final_verify.py` `DEFAULT_TESTS`, no gate row runs it, and it still stages the retired portfilesrv cache (`:59` constant, `:114` staging, `:135` FAT assertion). Its PASS is supporting evidence only, never a gate result.
