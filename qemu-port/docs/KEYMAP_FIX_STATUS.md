# KEYMAP FIX STATUS — CKeyTranslatorX built-in keymap (2026-11-24)

**STATUS: LANDED and VERIFIED.**

## Evidence
- `genuine/ewsrv/patched/ky_tran.cpp` modified 2026-11-24 20:01 (+157 lines): port block installs a built-in minimal keymap — identity scan-code table 0x00–0xff, editing/modifier scan codes mapped to genuine `EKey*` codes, `TConvTable`/`TFuncTable`/`TCtrlDigits` constructors zero-init + install `gPortDefaultEntry`/`gPortGenFuncTables[13]` with valid `pentries` (no more garbage deref in `TranslateKey`), `iIsdefaultKeyData=ETrue`, `Initialise()` returns `KErrNone` without EKDATA.DLL. Genuine table-walking code unchanged.
- `genuine/ewsrv/build_ewsrv.py` also modified; `genuine/build/EWSRV.E32` rebuilt 20:03 (56820 bytes, 212 EUSER imports).
- Post-fix tests 20:03: `/tmp/opencode/ewsrv_kytran_input_test.log` → `RESULT: PASS`; bootB `sendkey a: EKeyDown std=0x41 consumed=1`, explicit `EWSRV-side #PF after EKeyDown: absent`. `/tmp/opencode/genuine_input.keyboot.log` (20:03): 0 occurrences of `KERN 8` / `#PF` / `cr2=0xf0000010`; EKeyDown/EKeyUp delivered, clean shutdown.

## Remaining gap
None for the KERN 8 fault. Open (non-blocking) follow-ups: keymap is minimal identity (no EKDATA layouts — arrow/edit keys work via EKey* mapping, but no real layout/IME); `genuine/ewsrv/` has uncommitted changes (ky_tran.cpp, build_ewsrv.py, ewsrv_shims.cpp + untracked edisp files) awaiting the owner's commit.
