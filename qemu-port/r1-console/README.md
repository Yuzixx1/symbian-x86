# R1-CONSOLE-DATA-SOURCE — fixed US keymap and genuine HAL data

**Recipe closure status (card 24): HAL source recipe reconciled to the surviving
v2 source; the HAL v2 artifact was compiled and is byte-audited in the closure
report but has never been executed. EKDATA v3 was compiled and byte-audited with exactly three
genuine exports, but has never been executed. Console integration and acceptance
remain unproven.** This is not a console server, translator, input adapter,
display handler or launcher. Genuine HAL/EKTRAN/EDISP/EWSRV/ECONS and genuine
ESHELL remain the selected route. No shared integration edits here.

## Provenance and ownership

Original delivery worktree: `/tmp/opencode/r1-console-data-source`, detached at
port `fa62323eea7bc96cefd666e40f5189567731bd44`. The live dirty main workspace at
`/home/yuzi/Documents/Sybian REAL` is the current source of truth and was read
explicitly for coordinator docs, port constants and the surviving v2 build
inputs. Main was not reset to old HEAD. Card 24 owns the recipe/README/checker
reconciliation and the new closure report; HAL/keymap logic and genuine
definitions were not changed. No commit/push.

Upstream read-only reference: `kernelhwsrv`
`0c3208650587ac0230aed8a74e9bddb5288023eb`. Inputs:

* `kernel/eka/include/k32keys.h`, `e32keys.h`, `ewsrv/ky_tran.cpp`: genuine
  scan/conversion/function table layout, first-match behavior, modifier handling.
* `bsptemplate/asspandvariant/template_variant/specific/keymap.cpp`: limited
  adaptation of table/export and modifier/default rules, not the wholesale
  unrelated accent/control-digit template. Copyright/EPL notices preserved in
  `keymap/keymap.cpp`; the original remains unchanged. No distribution clearance
  is asserted. Review upstream license obligations before any redistribution.
* `kernel/eka/eabi/ekdata.def`: original three fixed GCC-mangled export names;
  no `bx86gcc/ekdata.def` exists in this checkout. The EKDATA recipe uses
  `DEFFILE ./ekdata.def`; the local `keymap/ekdatau.def` is byte-identical to
  this genuine EABI definition, avoiding an unverified architecture fallback.
* `halservices/hal/{halcfg.pl,inc/hal_data.h,src/hal_main.cpp,src/userhal.cpp}`
  and template `hal/hal.mmp`: generator grammar, immutable Get branch, real HAL
  implementation and build format. No upstream HAL source was copied/modified.
* Adopted `docs/r1_console_design_candidate_v1.md` §3 and coordinator console
  decisions in `r1_research_reconciliation_v0.md` (2026-10-23).

## HAL profile and exact interfaces

`hal/config.hcf` deliberately lists only eight attributes, all `=0` with no
`: set`. In the **genuine generator grammar**, this means valid immutable
InitialValue lookup, not a fake function that returns success. Attributes
omitted from the config have Properties=0 and genuine HAL returns
KErrNotSupported. Generated zero InitialValue entries for omitted attributes
do **not** make those attributes supported.

| Attribute | Initial value | Source / qualification |
| --- | --- | --- |
| ECPU | x86 (enum 2) | Live port Makefile `__X86__`, selected genuine x86 sources |
| ECPUABI | x86 (enum 6) | Selected x86 GCC SDK ABI; not ARM4/MSVC |
| EMemoryPageSize | 4096 | Genuine direct/x86/mmboot.h KPageShift=12; page allocation geometry, not a paging/isolation claim |
| ESystemTickPeriod | 15625 µs | Genuine linked kernel/stimer.cpp TTickQ::Init, 64-Hz nominal system ticks |
| ENanoTickPeriod | 1000 µs | Live QemuVariantAsic::MsTickPeriod, 1000-Hz PIT profile |
| EKeyboardIndex | 0 | Adopted immutable layout policy; not dynamic/P&S layout acceptance |
| EDisplayMode | 0 | Adopted immutable upstream VGA-text selection |
| EDisplayNumberOfScreens | 1 | Selected single-screen profile |

No guessed CPU revision/frequency, manufacturer, fast counter, bitmap pixel
geometry, twips, colors, display memory stride, battery, contrast, backlight or
power functions. In particular **ECPUArch is unsupported**, not an invented
revision number. No unresolved required constant was filled with a placeholder.
CPU frequency reporting would need a separately verified provider.

The one 80x25 **text-cell** screen still requires the separate kernel display
provider: fully initialized ScreenInfo with base 0xa0000, effective VGA address
0xb8000 after genuine EDISP adds 0x18000, real descriptor copying, and honest
mode/state behavior. 80x25 is not a bitmap pixel resolution. This card exposes
no ProcessDisplay* provider or private display implementation. Unsupported HAL
queries must be reviewed against the eventual real dependency closure; extend
through separately verified providers rather than arbitrary template values.

## Keymap/input boundary

`keymap/keymap.cpp` exports only:

1. KeyDataConvTable(SConvTable&, TUint&, TUint&, SScanCodeBlockList&, SKeyCodeList&)
2. KeyDataFuncTable(SFuncTables&)
3. KeyDataSettings(TRadix&, TCtrlDigitsTermination&, TInt&, TInt&)

These must occupy genuine fixed ordinals **1/2/3**, respectively. Structures
come from genuine k32keys.h, not local substitutes. There are 13 general-state
slots (all empty), a bounded default function table with final catch-all,
modifier functions, and no keypad block. Missing scans convert to EKeyNull.
All mapped scans lie in exported inclusive bounds Backspace(1)..Equals(0x83).

US layout: A..Z physical scans map to lowercase normally, uppercase with
Caps XOR Shift, lowercase with both; Ctrl+A..Z maps to 1..26. Shift changes
digits to `)!@#$%^&*(` in 0..9 order and punctuation to ordinary US symbols.
Alt is a modifier only, not AltGr/composition. Releases update side-specific
modifier state but emit no character. Releasing one Shift/Ctrl/Alt while the
other side remains held must retain the aggregate modifier (genuine EKTRAN
recomputes this). Caps toggles on keydown only; the input owner must suppress
repeated modifier make events. Caps and six side modifiers are non-autorepeat.

The input card must convert **physical Set-1** make/break into Symbian scans
before genuine Kern::AddEvent. Do not forward hardware bytes or already shifted
characters as the scan. Controlled PS/2 mapping contract:

| Set-1 make (hex) | Symbian scan |
| --- | --- |
| 1e.. row A/S/D/F/G/H/J/K/L = 1e,1f,20,21,22,23,24,25,26 | ASCII uppercase A,S,D,F,G,H,J,K,L |
| Q/W/E/R/T/Y/U/I/O/P = 10..19 | ASCII uppercase corresponding letter |
| Z/X/C/V/B/N/M = 2c..32 | ASCII uppercase corresponding letter |
| 02..0b | ASCII '1'..'9','0' |
| 01 / 0e / 0f / 1c / 39 | EStdKeyEscape / Backspace / Tab / Enter / Space |
| 2a / 36 / 1d / E0 1d / 38 / E0 38 | LeftShift / RightShift / LeftCtrl / RightCtrl / LeftAlt / RightAlt |
| 3a | EStdKeyCapsLock |
| E0 4b / E0 4d / E0 48 / E0 50 | LeftArrow / RightArrow / UpArrow / DownArrow |
| E0 47 / E0 4f / E0 49 / E0 51 / E0 52 / E0 53 | Home / End / PageUp / PageDown / Insert / Delete |
| 29 / 33 / 34 / 35 / 2b | XXX(backtick) / Comma / FullStop / ForwardSlash / BackSlash |
| 27 / 28 / 1a / 1b / 0c / 0d | SemiColon / SingleQuote / SquareBracketLeft / SquareBracketRight / Minus / Equals |

Break adds bit 7 to the final Set-1 byte, preserving E0; deliver the same
Symbian scan with key-up event type. This table is an **interface requirement**,
not an implemented hardware decoder. E1/Pause, PrintScreen multi-byte sequences,
Fn, F1..F24, Num/Scroll Lock, numeric keypad, ISO extra Hash key, Windows/Menu,
media/device/application/power keys and non-US layouts are absent. Unknown or
excluded sequences must not become characters or power/case events. Modifier
state recovery on ring overflow belongs to the input card. COM1 bytes are not
PS/2; USB requires a separately reviewed raw adapter. Do not call AddEvent from
the ISR/arbitrary DFC. DOMAINCLI must still be staged, but no power-domain
events/service success is synthesized by these tables.

## Canonical recipes and historical build status

The current recipes are reconciled and host-checked, but **card 24 performed no
HAL/EKDATA rebuild**. Historical isolated runs did produce the audited images:

* HAL v2: rc0 in `/home/yuzi/.cache/sybian-r1/r1-console-data-build-v2`; the
  surviving HAL bytes are audited, never executed. Current HAL recipe source
  matches that run, but the image was not rebuilt after this reconciliation.
* EKDATA v3: rc0 in `/home/yuzi/.cache/sybian-r1/r1-ekdata-build-v3`; exactly
  three genuine exports, never executed. Its `./ekdata.def` recipe is current.

A separately authorized isolated SDK using reviewed `port-x86-rfile64-v1` must
provide genuine headers, EUSER, HAL import library, compiler startup and Raptor
environment. Retain effective security defaults. Provision a read-only sibling
`kernelhwsrv` at workspace root (or adjust the owned MMP paths in a reviewed
staging copy); do not patch upstream/scratch.

From `qemu-port/r1-console/hal`, after setting `UPSTREAM` to the read-only source
root and creating an empty owned `generated/` directory in that authorized copy:

```sh
perl "$UPSTREAM/halservices/hal/halcfg.pl" -x \
  "$UPSTREAM/halservices/hal/inc/hal_data.h" config.hcf generated/config.cpp
perl "$UPSTREAM/halservices/hal/halcfg.pl" \
  "$UPSTREAM/halservices/hal/inc/hal_data.h" values.hda generated/values.cpp
sbs --toolcheck=off -d -b bld.inf -c x86_urel -p hal.mmp
```

From `qemu-port/r1-console/keymap` in the same authorized copy:

```sh
sbs --toolcheck=off -d -b bld.inf -c x86_urel -p ekdata.mmp
```

Both MMPs link euser.lib. HAL compiles unchanged `hal_main.cpp`/`userhal.cpp`
plus the two genuine generated tables. `hal.mmp` uses the strict stem
`eabi/hal.def`, which resolves the genuine `eabi/halu.def`; a literal
`halu.def` is wrong because Raptor would append a second `u`. EKDATA uses
`DEFFILE ./ekdata.def` so strict `u` resolution selects the local, byte-identical
`ekdatau.def`. Existing SDK `hal.lib` is the matching import-library route (HAL
image MMP has `NOEXPORTLIBRARY`). Verify SDK internal `kernel/hal_int.h`
exports/include paths. Earlier v1 output is historical and rejected; do not use
it as a console input. Canonical hashes and the exact recipe manifest are in
`docs/r1_console_recipe_closure_v1.md`.

Expected image metadata (must audit built images, not infer from this text):

| Image | UID1 | UID2 | UID3 | Security |
| --- | --- | --- | --- | --- |
| HAL.DLL | 10000079 | 1000008d | 100039e8 | genuine template capability All, vendor 70000001 |
| EKDATA.DLL and alias EKDATA.00 | 10000079 | 1000008d | 100039e0 | capability All, vendor 70000001 |

HAL/EKDATA capability All does not grant capabilities to the ordinary proof
application. Verify generated immutable tables, fixed non-ABSENT exports,
imports, DLL data/BSS/relocations and genuine startup with the loader owner.
Do not generate a replacement def, renumber ordinals or enable NOSTRICTDEF to
hide a mismatch. No source-side ABI freeze is claimed.

Privileged staging owner must place the audited HAL.DLL, **EKDATA.DLL and
EKDATA.00 as byte-identical copies of the same audited keymap image**, with
matching hashes, in real system search paths consistent with EnforceSysBin.
Do not rebuild the alias with another UID, invent an extension-only loader
shortcut, or rely on ignored ChangeKeyData errors. Genuine EWSRV first loads
EKDATA.DLL then tries EKDATA.00 for index 0; both real-name lookups must succeed.
Loader/FS owner must confirm suffix/decorated-name search and UID3 checks for
the .00 alias before EWSRV becomes system-permanent. Stage audited genuine
ECONS/EWSRV/EDISP/EKTRAN/DOMAINCLI/ESHELL and transitive dependencies separately.

## Host checks and future acceptance

Executed successfully from the live main checkout (card 24):

```sh
python3 qemu-port/r1-console/hal/check_source.py \
  --upstream '/home/yuzi/Documents/Sybian REAL/kernelhwsrv' \
  --port '/home/yuzi/Documents/Sybian REAL/qemu-port'
python3 qemu-port/r1-console/hal/check_hal_evidence.py \
  --v2-root /home/yuzi/.cache/sybian-r1/r1-console-data-build-v2 \
  --ekdata-root /home/yuzi/.cache/sybian-r1/r1-ekdata-build-v3 \
  --upstream '/home/yuzi/Documents/Sybian REAL/kernelhwsrv' \
  --port '/home/yuzi/Documents/Sybian REAL/qemu-port'
```

`check_source.py` validates exact HAL profile/constants, immutable/unsupported
properties and generated InitialValue values using the **real upstream Perl
generator**; generated text is temporary in the owned HAL directory, not an SDK
mutation. It checks scan bounds, duplicate singleton blocks, letter/digit
mappings, required keys, 13 state slots, three export declarations, genuine def
ordinals, the adopted `DEFFILE` recipes and UID metadata.

`check_hal_evidence.py` reads existing bytes only. It independently parses the
HAL PE export/symbol tables, E32 export slots, preserved `.prep`, generated
tables and both 484-byte InitialValue copies, and separately verifies the accepted
EKDATA v3 hash/exports/data/aliases. Neither host check compiles, links or
executes target images, and neither proves console behavior.

Required future serialized QEMU assertions (not run):

1. Audit image hashes/UIDs/capabilities/imports/exports and generated tables;
   query the eight HAL values and verify immutable Set rejection. Probe omitted
   CPU-speed/contrast/backlight/bitmap attributes for KErrNotSupported; immutable
   Get must not depend on property initialization. Do not infer global P&S closure.
2. Privileged preflight: load both keymap names, validate all three ordinal
   addresses and UID3, call exports and inspect table extents; prove alias load
   instead of merely observing default-layout fallback.
3. Exercise genuine EKTRAN (no substitute translator): all letters/digits,
   every punctuation pair, Caps/Shift XOR, Ctrl letters, arrows/edit keys,
   both-side hold/release, modifier autorepeat suppression and no key-up output.
   Confirm excluded scans yield no character and never trigger domain activity.
4. Integrate genuine display/event/launcher paths; observe visible EDISP VGA,
   pending-read idle while timers/output work, paired input completion/cancel,
   ESHELL healthy idle and unchanged t_cactw. Rendezvous alone is insufficient.
5. One integration owner adds stable-suite coverage, preserving existing legacy
   shell/PortCon/PortFile/pilot tests and exclusive VGA/input ownership. No shared
   suite or build wiring was edited by this card.

Draft owner manual step, only after coordinator approval: on confirmed disposable
media boot the integrated genuine console, record HAL values and alias-load
diagnostics, type `aA1![]{}\\|` plus arrows/edit keys, hold/release both Shift
sides, verify Ctrl/key-up behavior, visible output and shell recovery. Confirm
actual PS/2 versus USB route. No hardware run occurred; serial diagnostics alone
are not visible-console or R1-B proof.

Remaining integration constraints: live kernel/SDK configuration consistency;
layout alias search; genuine loader mutable-data/init/dependency lifecycle;
actual HAL API dependencies beyond this minimal immutable profile; display
descriptor ABI/direct VGA access; input recovery; genuine service closure and
system-permanent failure policy. Refer conflicts to the coordinator, never
weaken security or manufacture successful unsupported calls.
