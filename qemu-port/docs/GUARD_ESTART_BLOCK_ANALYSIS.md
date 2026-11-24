# ESTART block by the EFILE single-owner guard — diagnosis and minimal fix

Status: read-only diagnosis. No QEMU run; no `kernelhwsrv/` edits; no commit/push.
Evidence: `/tmp/opencode/r1_fullchain.log` (03:14), staged FS `/tmp/opencode/r1_fullchain_fs.img` (03:11),
`variant/e32load.cpp`, `genuine/pack_multi.py`, `genuine/estart_eshell/build_estart_eshell.py`.

## 1. Root cause (file:line)

The guard is **not** over-broad at runtime — it is correctly matching a **colliding UID3**:

* `genuine/pack_multi.py:113` stamps `UID2=0x1000008c, UID3=0x100000bf` (the genuine EFILE UID3) on
  **every** image it packs, including ESTART. `estart_.E32` and the staged `\ESTART.E32` (03:11, 25848 B)
  both genuinely contain `uids: 0x1000007a 0x1000008c 0x100000bf chk=0x55085fa7`.
* `genuine/estart_eshell/build_estart_eshell.py:277-296` patches only `etshell_`/`etshell_dbg` to a
  placeholder `0x10000111`; lines 282-283 explicitly keep ESTART on the EFILE UID. Neither uses the
  genuine UID3s (ESTART `0x10272C04`, ESHELL `0x101F7AE7`).
* `variant/e32load.cpp:62-66` `PortIsEfileImage()` = `IsEwsrvProcess(aName) ? EFalse :
  (info.iUid3 == KPortEfileUid3 || PortIsEfileName(aName))`.
* `variant/e32load.cpp:869-883`: `isEfile` → `if (sPortEfileOwner || efileBusy) return KErrAlreadyExists`
  (`e32err.h:186` = **-11**), else `sPortEfileOwner = ETrue`.

Trace from the log: `single-owner: EFILE.E32 uid3=100000bf claimed` (265) → `EFILE.E32 r=0 pid=9`
(1229) → `E32Validate uid3=100000bf` (1231) → `single-owner: ESTART.E32 uid3=100000bf ... launch
refused` (1309) → `GenuineBoot: ESTART.E32 r=-11` (1311). The boot list launches ESTART after EFILE
(`e32load.cpp:1490,1492`), so `sPortEfileOwner` is already set and `!FileServer` is published.

Ruled out:
* **Stale image buffer/global** — the staged ESTART file itself has `100000bf`; `E32Validate` fills a
  local `info`, and the 03:11 staged image matches the 1231/1309 printouts. Not a race with EFILE's
  threads.
* **Name substring** — `PortIsEfileName()` (42-58) tests `EFILE/EFILEX/EFILERFS/EFSRV` on the basename;
  `ESTART` matches none, and `IsEwsrvProcess()` (1399-1403) only excludes `EWSRV`. Refusal is UID3-only.
* **ProtServ** — the cap grant is at `e32load.cpp:924`, *after* the claim/refusal; irrelevant here.
* Artifacts were in flux (kernfull 02:55→03:06→03:11, ISO 02:56→03:06) but UID3 `100000bf` is stamped
  deterministically by `pack_multi.py:113`, so it is stable across those rebuilds.
* ESHELL is not refused in this log (`ESHELL r=0`, 2369); its UID3 `10000111` (line 1884 / staged
  `ESHELL.E32`) is a build-time workaround for the same guard, not a genuine UID3.

## 2. Minimal fix recipe

**A. Guard (`variant/e32load.cpp`, owned by another agent — recipe only):**
1. In `PortIsEfileImage()` (62-66) require **both** the actual image UID3 and an EFILE-family name;
   drop the name-only OR: `return aInfo.iUid3 == KPortEfileUid3 && PortIsEfileName(aName);`.
2. Add an explicit non-EFILE exclusion *before* the UID3 test, by basename in the style of
   `IsEwsrvProcess()` (1399-1403): never claim `ESTART`, `E32STRT`, `ESHELL`, `EWSRV`. This is the
   "never claim ESTART/ESHELL" requirement even while their staged UID3s still collide.
3. Keep `PortEfileProcessRunning()`/`PortFileServerPublished()` and `sPortEfileOwner` strictly inside
   `if (isEfile)` (796, 869) so a non-EFILE image can never be refused; keep claim rollback on failure
   (902, 913). No change to the claim's atomicity under NKern CS.

**B. Build (real fix at source):**
* `genuine/pack_multi.py:113`: stamp each image's genuine UID3 (EFILE `0x100000bf`, ESTART `0x10272C04`,
  ESHELL `0x101F7AE7`), recomputing UID checksum at +12 and header CRC at +20.
* `genuine/estart_eshell/build_estart_eshell.py:278-296`: replace the `0x10000111` placeholder with
  `0x101F7AE7` and remove the "ESTART keeps EFILE UID" path (282-283). After this the UID3 comparison
  alone is authoritative and the name fallback can be deleted.

## 3. No-regression checklist

* **EFILE dedup:** second EFILE/EFSRV alias still yields exactly 1 `claimed` + `launch refused`,
  IPC round trip x2, port-shim=0, no `#PF` (`r1_genuine_efsrv_test.py`, retire gate).
* **EFSRV slot:** `genuineFs` logic (1536-1539) still skips `EFSRV.E32` after `EFILE.E32 r=0`.
* **Shim fallback:** shim-only disk (no EFILE/EFSRV staged) must NOT print "file server up, svcmgr
  fallback skipped" and must run `SvcMgrBootLaunch()`/`\PORTFILESRV.E32`; guard never claims svcmgr.
* **EWSRV:** still `r=0` via `IsEwsrvProcess()` exclusion (64, 1399), VBE LFB + ProtServ at 924-936.
* **ESHELL:** reaches `C:/>`; the `Exc 00` (2366) is benign, not `#PF`/`FAULT`/`Panic`.
* `kernelhwsrv/` untouched; no `make clean`; no commit/push.

## 4. Discriminating test

Rebuild kernfull + ISO first (tree was in flux), then run the fullchain harness. Required markers:
`e32run: single-owner: EFILE.E32 uid3=100000bf claimed` **exactly 1**; `GenuineBoot: EFILE.E32 r=0`;
`E32Validate ... uid3=10272c04` for ESTART and `GenuineBoot: ESTART.E32 r=0` (**was -11**);
`E32Validate ... uid3=101f7ae7` for ESHELL and `GenuineBoot: ESHELL.E32 r=0`; `GenuineBoot: EWSRV.E32
r=0`; `launch refused` for non-EFILE **0**; `#PF`/`FAULT`/`Panic` and port-shim!=0 **0**.
