# USERLAND_BUILD_STATUS — 2026-11-24 19:58 (read-only diagnosis)

Scope: `make userland` failure analysis for whoever owns `user/user.mk` next.
No user.mk / user/ sources edited; no commit/push; `kernelhwsrv/` untouched.

## 1. Freshness check (pre-build)

| file | mtime | git status |
| --- | --- | --- |
| `user/user.mk` | 2026-11-24 19:53:55 | ` M` |
| `user/wsdl.c` | 2026-11-24 19:52:43 | ` M` |
| `user/ws_e32_client.c` | 2026-11-24 19:53:38 | `??` |
| `user/shot_e32.c` | 2026-11-24 19:48:03 | `??` |

user.mk last edit was >3 min before the build (19:58), so `make -n userland`
and a real `make userland` were run. Build log:
`/tmp/opencode/userland_build_*.log` (EXIT=2).

## 2. Observed build result

- 14 targets compiled/linked fine before the stop, **including the guilib
  chunk** (`guilib.o`, `guihello.o` → GUIHELLO.E32, DTEDIT.E32, DTCALC.E32).
  **No guilib fix needed.**
- `build/user/WSE32TEST.E32` and `build/user/WSE32DRAW.E32` are **already
  built and up to date** — the ws_e32_iat defsym + pack_multi rules at
  user.mk:1393-1422 are correct as-is. **No change needed there.**
- **The only `make userland` failure** (make stops at first error):
  ```
  /usr/bin/ld.bfd: build/user/wsdl.o: in function `marker':
  wsdl.c:(.text+0x44e): undefined reference to `ws_e32_iat_printf'
  make: *** [user/user.mk:1375: build/user/wsdl.elf] Error 1
  ```
  Cause: `user/wsdl.c` was migrated to the EWCLIENT pattern (line 23:
  `extern unsigned ws_e32_iat_printf;` at 0x40800c), but the WSDL rules at
  user.mk:1374-1378 still link with only `--defsym=wsdl_printf_slot=0x408000`
  and still pack with `tools/elf2e32.py --euser-import 915`.
- **`user/shot_e32.c` has NO rule in user.mk at all** (no SHOTE32 target), and
  **`tools/build_ews_app.py` does NOT exist** (checked `tools/`). The nearest
  existing builder is `genuine/ewsrv/build_client.py` (builds EWCLIENT.E32
  from patched `co_cli.cpp` + `co_twin.cpp` + `co_utl.cpp` + `ewsrv_shims.cpp`
  + `stubs.S` with g++, then pack_multi).

## 3. Exact user.mk patch list

### Fix A — WSDL.E32: replace lines 1374-1378 with the 4-defsym + pack_multi pattern

Replace:
```
build/user/wsdl.elf: build/user/start.o build/user/runtime.o build/user/wsclient.o build/user/wsdl.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=wsdl_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/WSDL.E32: build/user/wsdl.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet
```
with:
```
build/user/wsdl.elf: build/user/start.o build/user/runtime.o build/user/wsclient.o build/user/wsdl.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld --defsym=ws_e32_iat_connect=0x408000 --defsym=ws_e32_iat_send=0x408004 --defsym=ws_e32_iat_close=0x408008 --defsym=ws_e32_iat_printf=0x40800c -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/WSDL.E32: build/user/wsdl.elf genuine/pack_multi.py tools/mke32.py tools/e32read.py
	python3 genuine/pack_multi.py $< $@ --iat-start 0x8000 --ordinals 211,1547,120,915
	python3 tools/e32read.py $@ --quiet
```
(Keep the `wsdl.o` compile rule at 1372-1373 and the `userland:` dep at 1369;
optionally update the stale comment at 1367-1368 which still says
`wsdl_printf_slot`.)

### Fix B — SHOTE32.E32: add a new chunk (no rule exists today)

`tools/build_ews_app.py` does not exist yet. Either:
- (preferred, consistent with EWCLIENT) create `tools/build_ews_app.py`
  modeled on `genuine/ewsrv/build_client.py`, compiling
  `genuine/ewsrv/patched/co_cli.cpp`, `kernelhwsrv/.../ewsrv/co_twin.cpp`,
  `co_utl.cpp`, `genuine/ewsrv/ewsrv_shims.cpp`, `genuine/ewsrv/stubs.S` plus
  `user/shot_e32.c` (g++ `-x c++`), linking with the four
  `ws_e32_iat_*` defsyms (0x408000/04/08/0c) and packing with
  `genuine/pack_multi.py --iat-start 0x8000 --ordinals 211,1547,120,915`,
  output `build/user/SHOTE32.E32`; then add to user.mk:
```
userland: build/user/SHOTE32.E32
build/user/SHOTE32.E32: user/shot_e32.c tools/build_ews_app.py genuine/pack_multi.py | build/user
	python3 tools/build_ews_app.py shot_e32 build/user/SHOTE32.E32
```
- or inline the g++ compile/link lines in user.mk following the WSE32TEST
  pattern (lines 1393-1402) plus the genuine co_cli/co_twin/co_utl/shims
  objects.

### Fix C — none needed

- guilib chunk (user.mk:1209-1224, 1319-1342): builds clean.
- WSE32TEST / WSE32DRAW rules (1393-1422): already correct, images built.

## 4. Boundaries

Read-only diagnosis: `make -n userland` + one `make userland` (failed at
wsdl.elf as above) + `make build/user/WSE32TEST.E32 WSE32DRAW.E32` (both up
to date). No source/object/ISO modified beyond normal build outputs in
`build/user/`, no commit/push, no `make clean`, `kernelhwsrv/` untouched.
