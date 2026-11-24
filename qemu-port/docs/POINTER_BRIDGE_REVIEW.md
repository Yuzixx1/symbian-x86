# Genuine pointer-event bridge review

Verdict: **SAFE** (no kernel change, no boot-path defect found). git status shows kernelhwsrv/ untouched; all logic is port scaffolding calling genuine exports.

- Layout OK: `TPointerRawEvent : TRawEvent` (qemu_display.cpp:552-564) writes genuine protected fields iType/iTip/iPointerNumber/iDeviceNumber/iTicks/iU.pos.x/y — matches e32event.h:397-410 (bytes 0-3, iTicks@4, pos@8, 24B union => 32B event). No magic offsets, no `protected` hack; `iDeviceNumber=1` => `DeviceNumber()==0`.
- CS/preconditions OK: single path ThreadEnterCS→Kern::AddEvent→ThreadLeaveCS (qemu_display.cpp:622-624), mirroring ExecHandler::AddEvent (eventq.cpp:67-70). Boot DThread supplies thread context (qemu_variant_asic.cpp:53-85); `K::InactivityQ` is created in StartTickQueue before A::Init3 (sinit.cpp:470,484).
- Opt-in/race OK: read failure or empty file returns early (qemu_display.cpp:595-600); no marker means no AddEvent and no 300ms Sleep. Log: EWSRV already in scheduler (line 765) before inject (771), `evtThread=103e038`, `head==tail` right after AddEvent => synchronous TryDeliverEvent handoff to a ready RequestEvent.
- MEDIUM (test only) qemu_display.cpp:615,633-635 / r1_genuine_pointer_test.py:314-319: `consumed = (tail != tailBefore)` proves queue delivery, NOT that `CEvent::RunL()` called EUSER ord 1868 `TRawEvent::Pos()`; `tailBefore` is read outside the CS, so any other delivered event could set it. Verdict regex/group is otherwise correct (no spurious regex pass).
- LOW qemu_display.cpp:29,615-635: reads private `K::EventHeadPtr/TailPtr/Thread` from kern_priv.h for diagnostics only — shim couples to kernel internals; no logic depends on it, but a different kernel build could fail to link.
- LOW qemu_display.cpp:595-599: `FatUse('C')` return unchecked; when a drive is already active the file is read from that drive, not necessarily the boot volume (harmless with current staging).
- Corroboration: log has `add=0`, coords (80,40), ord 1868 bound (line 689), zero Exc/Panic/FAULT/#PF/Abort, client-window markers; no QEMU run performed here.

sret conclusion: delivery to `UserSvr::RequestEvent` is genuinely exercised; `Pos()` execution is strongly implied (ready request + no fault + bound ordinal) but **not directly asserted** — add an EWSRV post-Pos marker or injected-coordinate assertion before calling the sret path "proven". No blocking fixes required.
