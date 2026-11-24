# USERMK_CONSOLIDATION — consolidated, conflict-checked patch list for `user/user.mk`

**Status:** READ-ONLY consolidation report, 2026-11-24 ~20:12. No edits except this
file. `user/user.mk` was **not** touched (mtime 2026-11-24 20:01:14, 1671 lines —
possibly owned by another agent right now; this report gives *current* line numbers).
No commit/push, no `make clean`, `kernelhwsrv/` untouched.

**Why:** several migration chunks migrated the `user/*.c` sources to the EWSRV
PE2-IAT pattern but reported their `user.mk` rule edits without applying them.
The 19:58 report (`docs/USERLAND_BUILD_STATUS.md`) line numbers have since drifted
because `user.mk` was edited at 20:01 (WSE32DRAW chunk etc. added ~+9 lines above
WSDL, ~+29 above GCBLITTEST). Current line numbers below are re-verified by grep.

## 0. Freshness / prerequisites

| item | state |
|---|---|
| `user/user.mk` mtime | 2026-11-24 20:01:14 (1671 lines) — read-only here |
| `tools/build_ews_app.py` | **DOES NOT EXIST** (checked `tools/`). Nearest builder: `genuine/ewsrv/build_client.py` (EWCLIENT pattern: genuine `co_cli.cpp` + `co_twin/co_utl` + `ewsrv_shims.cpp` + `stubs.S`, g++ `-x c++`, one thunk per EUSER import, pack via `genuine/pack_multi.py`) |
| `genuine/pack_multi.py` | exists (2026-11-24 03:18) |
| WSE32TEST / WSE32DRAW rules | **already exist and are correct** — user.mk:1389-1411 and 1413-1431 (`ws_e32_iat_*` defsyms 0x408000/04/08/0c + `pack_multi --iat-start 0x8000 --ordinals 211,1547,120,915`); images built and up to date. **No change needed.** |
| Reference IAT slot map | 0x408000 ord 211 CreateSession, 0x408004 ord 1547 DoSendReceive(sync), 0x408008 ord 120 Close, 0x40800c ord 915 RDebug::Printf |

Migrated sources and the IAT symbols they now reference (all via `--defsym`, so
no object defines them):
- `user/wsdl.c:23` — `ws_e32_iat_printf`; also calls `ws_fill/ws_text/ws_window_geom` → **must link `build/user/ws_e32_client.o`** (the E32-path impls live in `ws_e32_client.c`, see `wsclient.c:314` comment).
- `user/saver.c:50-53` — `saver_iat_connect/send/close/printf` (self-contained, no `ws_*` calls).
- `user/gcblittest.c:43-46` — `gcblittest_iat_connect/send/close/printf` (self-contained).
- `user/breakout.c:54` — `breakout_iat_async` **at 0x408010, ord 1548** (async DoSendReceive) on top of the `ws_e32_iat_*` map; calls `ws_open/ws_server_ready/ws_screen_size/ws_window_create/ws_draw_fill_rect` → must link `build/user/ws_e32_client.o`.
- `user/shot_e32.c` — full EWCLIENT pattern (genuine `e32twin.h`/`co_cli.cpp`, ~20 EUSER imports); previously built as `SHOTE32.E32` 53888 B, 20 imports, e32read PASS (see `docs/CLEANUP_LOG.md`), but has **no rule in user.mk today**.

## 1. Per-target patch list (old block → new block), current line numbers

### T1 — WSDL.E32 swap (reported as ~1372-1378 at 19:58 → now **1383-1387**)

Old (current 1383-1387; keep dep 1378, `wsclient.o` 1379-1380, `wsdl.o` 1381-1382;
optionally refresh stale comment 1372-1377 which still says `wsdl_printf_slot`):
```
build/user/wsdl.elf: build/user/start.o build/user/runtime.o build/user/wsclient.o build/user/wsdl.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=wsdl_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/WSDL.E32: build/user/wsdl.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet
```
New (WSE32TEST pattern + `ws_e32_client.o` — required, `wsdl.c` calls the
`ws_*` API that `ws_e32_client.c` implements):
```
build/user/wsdl.elf: build/user/start.o build/user/runtime.o build/user/ws_e32_client.o build/user/wsdl.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld --defsym=ws_e32_iat_connect=0x408000 --defsym=ws_e32_iat_send=0x408004 --defsym=ws_e32_iat_close=0x408008 --defsym=ws_e32_iat_printf=0x40800c -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/WSDL.E32: build/user/wsdl.elf genuine/pack_multi.py tools/mke32.py tools/e32read.py
	python3 genuine/pack_multi.py $< $@ --iat-start 0x8000 --ordinals 211,1547,120,915
	python3 tools/e32read.py $@ --quiet
```
(Drop `wsclient.o` from the link only if nothing references its symbols —
`wsdl.c` uses the `ws_*` API now served by `ws_e32_client.o`; verify with
`make build/user/WSDL.E32`.) **This is the current `make userland` breaker**
(`undefined reference to ws_e32_iat_printf`, was user.mk:1375 at 19:58).

### T2 — SHOTE32.E32 new rule (no rule exists anywhere in user.mk today)

Prerequisite: `tools/build_ews_app.py` must be created first (modeled on
`genuine/ewsrv/build_client.py`; compiles genuine `co_cli.cpp` +
`co_twin/co_utl` + `ewsrv_shims.cpp` + `stubs.S` + `user/shot_e32.c` with
g++ `-x c++`, links per-import defsyms, packs with `genuine/pack_multi.py`),
then append (suggest placing right after the WSE32DRAW chunk, ~line 1432):
```
userland: build/user/SHOTE32.E32
build/user/SHOTE32.E32: user/shot_e32.c tools/build_ews_app.py genuine/pack_multi.py | build/user
	python3 tools/build_ews_app.py shot_e32 build/user/SHOTE32.E32
```
Alternative: inline the g++ compile/link lines in user.mk following the
WSE32TEST pattern (1407-1411) plus the genuine co_cli/co_twin/co_utl/shims
objects. SHOTE32 was already proven once (53888 B, e32read PASS).

### T3 — SAVER.E32 swap (reported 555-565 — **still exact**: block = **555-565**)

Old (561-565; keep dep 558, `saver.o` 559-560):
```
build/user/saver.elf: build/user/start.o build/user/runtime.o build/user/saver.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=saver_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/SAVER.E32: build/user/saver.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet
```
New (saver.c is self-contained with `saver_iat_*`; no `ws_e32_client.o` needed):
```
build/user/saver.elf: build/user/start.o build/user/runtime.o build/user/saver.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld --defsym=saver_iat_connect=0x408000 --defsym=saver_iat_send=0x408004 --defsym=saver_iat_close=0x408008 --defsym=saver_iat_printf=0x40800c -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/SAVER.E32: build/user/saver.elf genuine/pack_multi.py tools/mke32.py tools/e32read.py
	python3 genuine/pack_multi.py $< $@ --iat-start 0x8000 --ordinals 211,1547,120,915
	python3 tools/e32read.py $@ --quiet
```
(Also refresh comment 555-557, which still describes execs 249/250/251.)

### T4 — GCBLITTEST.E32 swap (reported ~1492-1499 → now **1521-1528**; comment 1516-1520)

Old (1524-1528; keep dep 1521, `gcblittest.o` 1522-1523, keep `malloc.o` in the
elf deps — gcblittest still uses it):
```
build/user/gcblittest.elf: build/user/start.o build/user/runtime.o build/user/malloc.o build/user/gcblittest.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=gcblittest_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/GCBLITTEST.E32: build/user/gcblittest.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet
```
New:
```
build/user/gcblittest.elf: build/user/start.o build/user/runtime.o build/user/malloc.o build/user/gcblittest.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld --defsym=gcblittest_iat_connect=0x408000 --defsym=gcblittest_iat_send=0x408004 --defsym=gcblittest_iat_close=0x408008 --defsym=gcblittest_iat_printf=0x40800c -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/GCBLITTEST.E32: build/user/gcblittest.elf genuine/pack_multi.py tools/mke32.py tools/e32read.py
	python3 genuine/pack_multi.py $< $@ --iat-start 0x8000 --ordinals 211,1547,120,915
	python3 tools/e32read.py $@ --quiet
```
(The `GCBLIT.E32` alias at 1592-1593 is unaffected — it just `cp`s GCBLITTEST.E32.)

### T5 — BREAKOUT.E32 swap (reported 1249-1262 — **still exact**: block = **1249-1262**)

Old (1258-1262; keep dep 1255, `breakout.o` 1256-1257):
```
build/user/breakout.elf: build/user/start.o build/user/runtime.o build/user/breakout.o user/link.ld
	ld -m elf_i386 --emit-relocs --defsym=breakout_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/BREAKOUT.E32: build/user/breakout.elf tools/elf2e32.py tools/mke32.py
	python3 tools/elf2e32.py $< $@ --euser-import 915
	python3 tools/e32read.py $@ --quiet
```
New (**5 slots** — adds `breakout_iat_async=0x408010` ord 1548; links
`ws_e32_client.o` because breakout.c calls `ws_open/ws_window_create/...`):
```
build/user/breakout.elf: build/user/start.o build/user/runtime.o build/user/ws_e32_client.o build/user/breakout.o user/link.ld
	ld -m elf_i386 --emit-relocs -T user/link.ld --defsym=ws_e32_iat_connect=0x408000 --defsym=ws_e32_iat_send=0x408004 --defsym=ws_e32_iat_close=0x408008 --defsym=ws_e32_iat_printf=0x40800c --defsym=breakout_iat_async=0x408010 -o $@ $(filter %.o,$^) $(LIBGCC)
build/user/BREAKOUT.E32: build/user/breakout.elf genuine/pack_multi.py tools/mke32.py tools/e32read.py
	python3 genuine/pack_multi.py $< $@ --iat-start 0x8000 --ordinals 211,1547,120,915,1548
	python3 tools/e32read.py $@ --quiet
```
**Gate:** BREAKOUT is chunk D3 and is blocked on the EWSRV input-injection /
`ws_get_event` hard gate (`docs/GUISHELL_MIGRATION_MAP.md` §2 item 2) — apply
the rule only when the D1 input bridge lands.

## 2. Conflict check

| chunk | lines touched (current user.mk) | overlaps another chunk? |
|---|---|---|
| T1 WSDL | 1383-1387 (comment 1372-1377 optional) | no |
| T2 SHOTE32 | append-only, new chunk after ~1431 | no |
| T3 SAVER | 561-565 (comment 555-557 optional) | no |
| T4 GCBLITTEST | 1524-1528 (comment 1516-1520 optional) | no |
| T5 BREAKOUT | 1258-1262 (comment 1249-1254 optional) | no |

**No two chunks touch the same lines** — all five ranges are disjoint and each
adds its own single `userland:` dep line at a distinct spot. The only shared
resource is conceptual, not textual: all targets reuse the same IAT slot map
0x408000-0x40800c (T5 adds 0x408010), which is fine because each target links
its own objects and uses per-app defsym names (`ws_e32_iat_*`, `saver_iat_*`,
`gcblittest_iat_*`, `breakout_iat_async`). T1 and T5 additionally share
`build/user/ws_e32_client.o` as a link input — no rule-text conflict.
Stale-number warning: reports written before 20:01 cite WSDL at 1374-1378 and
GCBLITTEST at ~1492-1499; apply at the current numbers above, not the old ones.

## 3. Suggested ordering for whoever owns user.mk next

1. **T1 WSDL** first — it is the live `make userland` breaker
   (`ws_e32_iat_printf` undefined at the wsdl.elf link); pattern already proven
   by WSE32TEST/WSE32DRAW at 1402-1431.
2. **T2 SHOTE32** — create `tools/build_ews_app.py` (clone of
   `genuine/ewsrv/build_client.py` generalized over the app name), then append
   the rule; SHOTE32 already built+validated once via this pattern.
3. **T3 SAVER** (chunk C, no input dependency; self-contained defsyms).
4. **T4 GCBLITTEST** (chunk C; keep `malloc.o` in the elf deps).
5. **T5 BREAKOUT** last — gated on the D1 input bridge / `ws_get_event`
   (`GUISHELL_MIGRATION_MAP.md` §2); needs the 5th slot/ordinal 1548.

After each: `make build/user/<TARGET>.E32` then `python3 tools/e32read.py` (the
rules already do) and finally a full `make userland`; then re-run
`python3 tools/r1_os_selfcheck.py` before any retire-gate flips.

## 4. Boundaries

Read-only: only this doc written. `user/user.mk` and all `user/*.c` read only
(mtime checked; not modified here). No build run, no commit/push, no
`make clean`, `kernelhwsrv/` untouched.
