# R1-CONSOLE-RECIPE-CLOSURE v1 — pinned recipes and audited assets

2026-10-25. Card 24. **Source/recipe reconciliation and existing-byte audit
PASS; no HAL/EKDATA rebuild and no guest execution. Console closure is not
proven.**

The main HAL recipe now matches the surviving v2 recipe, the checker tests the
actually adopted DEFFILE recipes, stale build-status text is separated into
historical versus compiled-never-run states, and the accepted EKDATA v3 artifact
is pinned. The surviving HAL v2 image was independently parsed from its PE,
E32 and `.prep` bytes: six ordinals and the full 484-byte `InitialValue` match.
The accepted EKDATA v3 image has exactly three genuine exports and no writable
DLL data.

No HAL/keymap logic, genuine definition, shared build file, QEMU image or target
execution was changed. The upstream `kernelhwsrv` reference is at
`0c3208650587ac0230aed8a74e9bddb5288023eb` and clean.

## 1. Baseline and source reconciliation

The dirty main tree at `fa62323eea7bc96cefd666e40f5189567731bd44` plus current
work was the baseline. Pre-change hashes were recorded before editing; the
owned documents/sources are untracked and no old-HEAD overwrite was used.

| Owned path | Before | After |
| --- | --- | --- |
| `qemu-port/r1-console/hal/hal.mmp` | `939361600b5ba3eac0dcb1b472bc8d15e6f1dbbdc3ac24a8a6d56f4cf19535f1` | `c18c529203cf1e442e6f6e50af85205bedd0b988bd362a0d8c66dddc3efd190f` |
| `qemu-port/r1-console/hal/check_source.py` | `783a11868f9e693585d419737d89a8c5c82021319b238e045b97f7722bdbe522` | `1879adf5e9830d81923e438f7e9d6140bb734bc441db81edfac6d86896e9d872` |
| `qemu-port/r1-console/README.md` | `29a296b73ad76abbb87b165d49c5500b537e0401f232b948f8b23c662ef488c2` | `300594014927a1f00c8b9eb19d2ee198ceb030f1f441e810bf8e3a6983305465` |
| `qemu-port/r1-console/keymap/keymap.cpp` | `e459b089d9fb4645b0a5f5721f6d7a3e2fc59547b09756a545e3d21b9923da80` | `84157e7ba0e3d2cbaf8d8f48621c6f8be754f2a4dd36f52d8ba4ddf54ab33903` |
| `qemu-port/r1-console/hal/check_hal_evidence.py` | new | `38747f9d9d06780ddc801ef110eea32549d0e527ffdc40676f5287d781dd510c` |

No logic changed in `keymap.cpp`; only the stale `UNBUILT / UNRUN` comment became
"Compiled in EKDATA v3; never executed". `ekdata.mmp` and the local
`ekdatau.def` were already the accepted card-19 state and are unchanged here.

### HAL strict stem

Raptor's resolver appends `u` for strict Unicode definitions, and prepends the
platform default def directory only when the DEFFILE value has no slash. Main
previously named the genuine file literally:

```text
DEFFILE ../../../kernelhwsrv/halservices/hal/eabi/halu.def
```

That resolves as `haluu.def`, the v1 failure. The reconciled recipe, identical
to the surviving v2 HAL recipe, is:

```text
DEFFILE ../../../kernelhwsrv/halservices/hal/eabi/hal.def
```

The slash prevents platform-directory substitution; the appended `u` resolves
the genuine `eabi/halu.def`. The v2 build XML records
`Resolved def file: .../halu.def` and `prepdef.pl ... halu.def ... hal.prep
nodatasizes _E32Dll`.

### EKDATA local def

The accepted card-19 recipe is retained:

```text
DEFFILE ./ekdata.def
```

Strict resolution produces `./ekdatau.def`; the local file is byte-identical to
`kernel/eka/eabi/ekdata.def` (`e2c06964...e24d`). A bare stem would have selected
the platform default directory, which is why the explicit `./` is part of the
pinned recipe. No `NOSTRICTDEF`, auto-export, ordinal invention or genuine-def
edit is used.

### Checker and stale status

`check_source.py` now asserts the adopted HAL strict stem, the adopted local
EKDATA `./` stem, local-def byte identity and the absence of `NOSTRICTDEF`. It
still uses the genuine generator and keymap checks. Its output now says
`HOST-ONLY` rather than implying the checker itself built or ran target code.

The README header distinguishes:

* HAL v2 — **compiled and byte-audited, never executed**; current recipe matches
  the v2 source but was not rebuilt after reconciliation.
* EKDATA v3 — **compiled and byte-audited, never executed**.
* v1 HAL/EKDATA output — **historical, rejected**; do not stage it as a console
  input.
* Console integration/runtime — **not proven**.

## 2. Independent HAL v2 byte audit

Existing artifacts only, no rebuild:

* `/home/yuzi/.cache/sybian-r1/r1-console-data-build-v2/epoc32/release/x86/urel/hal.dll`
* `.../hal.dll.sym`, `.../hal.prep`, `.../generated/{config.cpp,values.cpp}`
* `.../logs/build_hal.xml`

HAL image SHA-256 is
`4a95dbe01d201b8a0b347e0e5c4cc39f798a7d89d6e163d7f63d71d3a04cb150`
(10,960 bytes); staged `HAL.DLL` is the same hash. The E32 image has six export
slots, no writable data (`data_size=0`), no BSS (`bss=0`) and no data
relocations (`data_reloc=0`).

The independent parser compared the genuine def, COFF symbol table, PE export
address table and E32 export slots:

| Ordinal | Genuine symbol | PE RVA | E32 offset |
| --- | --- | --- | --- |
| 1 | `HAL::Get(HALData::TAttribute, int&)` | `0x16c0` | `0x6c0` |
| 2 | `HAL::Set(HALData::TAttribute, int)` | `0x1590` | `0x590` |
| 3 | `HAL::GetAll(int&, HAL::SEntry*&)` | `0x16f0` | `0x6f0` |
| 4 | `HAL::Get(int, HALData::TAttribute, int&)` | `0x1640` | `0x640` |
| 5 | `HAL::Set(int, HALData::TAttribute, int)` | `0x1520` | `0x520` |
| 6 | `HalInternal::InitialValue` | `0x4160` | `0x2440` |

The PE function table has exactly six entries at base 1; the E32 export count is
six; no name/ordinal aliases are present. All six required mangled symbols were
found in the actual COFF symbol table, in `.text` for 1–5 and `.rdata` for 6.

### 484-byte InitialValue and the 428 annotation

The genuine `halu.def` annotates ordinal 6 as `DATA 428`, but the actual array
has 121 `TInt` entries: **484 bytes**. The preserved `hal.prep` is the genuine
def with that numeric size removed and a trailing blank line:

```text
...HalInternal12InitialValueE @ 6 NONAME DATA
```

`prepdef.pl` was invoked with `nodatasizes`, so the 428 annotation did not limit
the emitted symbol. The independent checks are:

* Generated `values.cpp` has exactly 121 ordered attribute values.
* The 484 bytes at PE `.rdata` offset `0x160` (file `0x2960`) match generated
  values byte-for-byte.
* The 484 bytes at E32 code offset `0x2440` (file `0x24dc`) match the same
  generated values byte-for-byte.
* PE and E32 copies are identical to each other.
* Eight attributes are valid with zero implementation pointers:
  `ECPU`, `ECPUABI`, `ESystemTickPeriod`, `ENanoTickPeriod`, `EMemoryPageSize`,
  `EKeyboardIndex`, `EDisplayMode`, `EDisplayNumberOfScreens`.
* `ECPU=2`, `ECPUABI=6`, `ESystemTickPeriod=15625`, `ENanoTickPeriod=1000`,
  `EMemoryPageSize=4096`; all omitted attributes are zero/unsupported.

The 428/484 difference is therefore explained by stripped size metadata and a
fully checked emitted payload, not by editing the genuine def. A future recipe
that fails to invoke `nodatasizes` would be a separate build failure; the pinned
def must not be changed merely to match the annotation.

The existing generated tables are reproducible from the current
`config.hcf`/`values.hda` with the genuine `halcfg.pl`; only the output-path
comment on the first generated line differs. This is a host generator check,
not a HAL rebuild.

## 3. Accepted EKDATA v3, kept separate

EKDATA v3 is a separate card-19 result:

* Image SHA-256:
  `2e11e7287325a7829a28b7cb510c5605390e57b1953a0b16704b8210c4aced0f`
  (2,780 bytes).
* Exactly three ordinal slots: `KeyDataConvTable` 1, `KeyDataFuncTable` 2,
  `KeyDataSettings` 3, matched to actual linked symbols and E32/PE slots.
* No fourth/ignore export; imports are genuine EUSER `User::Free` 644 and
  `User::Alloc` 646.
* `data_size=0`, `bss=0`, `data_reloc=0`; no writable DLL data.
* `staged/sys/bin/EKDATA.DLL` and `EKDATA.00` are byte-identical to the same
  image, not symlinks.

The current EKDATA recipe and `keymap.cpp` differ from the built v3 source only
in comments (the `./` explanation and compiled-never-run status). Compiled logic
and the exported def are unchanged. HAL v2 audit does not repair or validate the
separate EKDATA history; EKDATA v3 does not validate the HAL runtime.

## 4. Canonical pinned manifest

### Repository recipe inputs

| Path | SHA-256 |
| --- | --- |
| `qemu-port/r1-console/hal/hal.mmp` | `c18c529203cf1e442e6f6e50af85205bedd0b988bd362a0d8c66dddc3efd190f` |
| `qemu-port/r1-console/hal/bld.inf` | `5cce7d0a7150aafff2c1942723c7df14819a0446389fea415ab214def4ca2422` |
| `qemu-port/r1-console/hal/config.hcf` | `b50dda28a271a26b3567253a9b06fd9778471482737521fbab894375c648ddc0` |
| `qemu-port/r1-console/hal/values.hda` | `f51a7323b69f0f1c1fa7cfada37fda7b99f58ff5d043a1c084c7ec4b2e93df94` |
| `qemu-port/r1-console/hal/check_source.py` | `1879adf5e9830d81923e438f7e9d6140bb734bc441db81edfac6d86896e9d872` |
| `qemu-port/r1-console/hal/check_hal_evidence.py` | `38747f9d9d06780ddc801ef110eea32549d0e527ffdc40676f5287d781dd510c` |
| `qemu-port/r1-console/keymap/ekdata.mmp` | `eb857159f775298e97c0e959d92ae91f898056ced021d5bd65edd3b02e0abae0` |
| `qemu-port/r1-console/keymap/ekdatau.def` | `e2c069642d2ec349826f2d94cb9ccf3c3b5fe7db1f6013772b0ad1dd4cd0344f` |
| `qemu-port/r1-console/keymap/keymap.cpp` | `84157e7ba0e3d2cbaf8d8f48621c6f8be754f2a4dd36f52d8ba4ddf54ab33903` |
| `qemu-port/r1-console/keymap/bld.inf` | `8c3a36201e4ee7314216629748b4b600271f6b27cb8a93ebab8dcaeec549e24d` |

### Frozen genuine definitions

| Path | SHA-256 |
| --- | --- |
| `kernelhwsrv/halservices/hal/eabi/halu.def` | `ee25994480fb665de219b626a233301173724368431609fa061eaca190508028` |
| `kernelhwsrv/kernel/eka/eabi/ekdata.def` | `e2c069642d2ec349826f2d94cb9ccf3c3b5fe7db1f6013772b0ad1dd4cd0344f` |

### Accepted target artifacts and evidence

| Asset / evidence | SHA-256 |
| --- | --- |
| HAL.DLL (accepted v2) | `4a95dbe01d201b8a0b347e0e5c4cc39f798a7d89d6e163d7f63d71d3a04cb150` |
| EKDATA.DLL = EKDATA.00 (accepted v3) | `2e11e7287325a7829a28b7cb510c5605390e57b1953a0b16704b8210c4aced0f` |
| HAL v2 `hal.prep` | `c6fd6c1aefbc775753c2382204bf984b66348164ffaaa97f45be6916e716a1d1` |
| HAL v2 `generated/values.cpp` | `f20af7c5f2ae93330e23826b608e82bd2fb74de6d8bb4c257ce32876a398adab` |
| HAL v2 `generated/config.cpp` | `90316b6312a0ebedb13e8125452b302d604dd4e1b623adf8ff7a3f4abe2ce181` |
| HAL v2 `logs/build_hal.xml` | `fbbd61656e5e284627dc0790a547d1f87c7ca0827ddaebc516d4e369f3194117` |
| EKDATA v3 `logs/build_ekdata.xml` | `429fdf7cdefdcbb2d74cd8eee537c6c7d37065e06ecb5375ab1e8326aae087d6` |
| EKDATA v3 `logs/v3_audit.json` | `393a42a5998d5b24121b272e8a2cfea3024b344ccb011d1e4537815f60d29071` |
| Existing HAL strict audit (historical input) | `4a760453be1186040ac99429317ccdad37ab2c07434d7796231bbef297db8907` |
| Existing HAL table audit (historical input) | `ac548d65d74f8277329f2edb0bb973222048bb31f13acf530051a7cb1c94dff0` |
| Existing combined audit (historical input) | `18c95f7a848473dbd9e7af2af09786da3b1a298cbcd8eae3c30c198fc64a1997` |

The three "historical input" files are evidence records, not trusted results:
`check_hal_evidence.py` re-derived the six ordinals and 484 bytes from image
bytes.

### Toolchain and variant pins

| Input | SHA-256 |
| --- | --- |
| EUSER | `4851fec4b6439bf5263e6f10f7f3010828f84df55df050c710f57077f2dd017b` |
| EFSRV (copied input, not linked by these two recipes) | `332d97274a151bb17a18112e728684b98c8462fe8723bfadb1de0d471e372b6d` |
| petran.exe | `b200bf0878679e6c7d6c20e5a78cb2bb93f16d5b21790a2b66c7dd04a53ec155` |
| platform_paths.hrh | `8a8ebdc3f40804d630def1af4c0c3a68568a79d13eb08d0661d9ba252dd96f31` |
| port_x86_rfile64_v1.hrh | `500889884b3063c1b0712c000d2c62f55e70774cbfda41b51b82f343be277106` |
| variant.cfg | `2c505b4bb43e4b74e362589ce78f866053e4e9d8283688e47cca321c58e08cd0` |

Configuration: `port-x86-rfile64-v1`, actual target `x86_urel`.

### Canonical build/stage recipe

From the authorized isolated copy, with `UPSTREAM` pointing at the read-only
genuine source root:

```sh
# HAL
cd qemu-port/r1-console/hal
perl "$UPSTREAM/halservices/hal/halcfg.pl" -x \
  "$UPSTREAM/halservices/hal/inc/hal_data.h" config.hcf generated/config.cpp
perl "$UPSTREAM/halservices/hal/halcfg.pl" \
  "$UPSTREAM/halservices/hal/inc/hal_data.h" values.hda generated/values.cpp
bash "$SBS_HOME/bin/sbs" --toolcheck=off -d -j 1 \
  -b bld.inf -c x86_urel -p hal.mmp

# EKDATA
cd ../keymap
bash "$SBS_HOME/bin/sbs" --toolcheck=off -d -j 1 \
  -b bld.inf -c x86_urel -p ekdata.mmp
```

Do not run both against a shared output directory concurrently. Stage only
audited images: `HAL.DLL`, `EKDATA.DLL` and `EKDATA.00`, with both EKDATA names
byte-identical. A rebuild must be rehashed and re-audited; byte identity with
the pinned images is not assumed across toolchain or output paths.

### Console integration component status

| Component | Concrete source/recipe | Pinned artifact | Status |
| --- | --- | --- | --- |
| HAL | `qemu-port/r1-console/hal/hal.mmp` + genuine `halservices/hal` | `4a95dbe0...` | accepted evidence; never executed |
| EKDATA | `qemu-port/r1-console/keymap/ekdata.mmp` + genuine def | `2e11e728...` (both names) | accepted evidence; never executed |
| EKTRAN | `kernelhwsrv/kernel/eka/ewsrv/ektran.mmp` | none | build/audit pending |
| EWSRV | `kernelhwsrv/kernel/eka/ewsrv/ewsrv.mmp` | none | build/audit pending |
| ECONS | `kernelhwsrv/kernel/eka/ewsrv/econs.mmp` | none | build/audit pending |
| EDISP | `kernelhwsrv/kernel/eka/drivers/edisp/edisp.mmp` | none | build/audit pending |
| ESHELL | `kernelhwsrv/userlibandfileserver/fileserver/etshell/eshell.mmp` | none | build/stage pending |

`DOMAINCLI` and other transitive dependencies remain separate integration
requirements; this table is not a new console survey or an ABI freeze.

## 5. Concrete remaining operations

1. **HAL:** use the accepted v2 artifact for the next integration review, or
   authorize a fresh HAL-only build from the now-reconciled recipe and audit its
   hash/exports/InitialValue before staging. This card did neither rebuild.
2. **Console components:** build and audit the five concrete MMPs above in the
   same isolated `port-x86-rfile64-v1` environment. No artifact/hash is claimed
   yet for those components.
3. **Loader/FS:** confirm decorated/suffix name search and UID3 checks for the
   `EKDATA.00` alias, plus system-bin path and immutable-data/startup handling,
   before making EWSRV permanent.
4. **Runtime:** disposable-image QEMU/serial checks, two-boot persistence,
   ordinary `TestBootSequence=false`, display/input ownership and owner desktop
   observation remain separately authorized. No guest was run here.
5. **Acceptance:** do not treat accepted HAL/EKDATA source or export audits as
   visible-console, translation, input, security or hardware acceptance.

## 6. Host validation actually run

After the changes:

```sh
python3 qemu-port/r1-console/hal/check_source.py \
  --upstream "$PWD/kernelhwsrv" --port "$PWD/qemu-port"

python3 qemu-port/r1-console/hal/check_hal_evidence.py \
  --v2-root /home/yuzi/.cache/sybian-r1/r1-console-data-build-v2 \
  --ekdata-root /home/yuzi/.cache/sybian-r1/r1-ekdata-build-v3 \
  --upstream "$PWD/kernelhwsrv" --port "$PWD/qemu-port"
```

Both PASS. The evidence checker reports HAL six-ordinal/PE/E32/484-byte matches,
HAL six exports and zero writable data, EKDATA v3 hash/three exports/zero data,
byte-identical EKDATA aliases, `.prep` `nodatasizes` behavior and the current
DEFFILE recipes. Raw JSON, command logs and baseline provenance are under
`/home/yuzi/.cache/sybian-r1/r1-console-recipe-closure-v1/logs/`.

No C++ target compilation, target link, HAL/EKDATA rebuild, QEMU, device write,
hardware run, sanitizer claim, commit or push occurred. Host parsing and
generator checks cannot establish loader behavior or console acceptance.
