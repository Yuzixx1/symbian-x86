# R1-EKDATA-LOCAL-DEF — single EKDATA-only v3 build

> **Later status (card 24):** main HAL MMP/checker and status documentation were
> reconciled in `r1_console_recipe_closure_v1.md`; the card-19 statement below
> that they were left untouched describes the pre-closure state. No HAL/EKDATA
> rebuild occurred in card 24.

2026-10-23. **Build and exact three-ordinal audit PASS; never executed.**
Coordinator recovery waiver read in `r1_console_data_v2_recovery.md`. No further
missing-report search, recovered-prose claim, HAL rebuild, target retry, guest
execution, QEMU, shared build edits, commit or push.

## Source recovery and resolver evidence

Verified all ten surviving source files against v2 `logs/source_hashes.sha256`
before adopting them. Also verified all six recovery-note hashes (strict audit,
generated table audit, audit.json and staged HAL/EKDATA aliases). Source remains
at `/home/yuzi/.cache/sybian-r1/r1-console-data-build-v2/workspace/qemu-port/r1-console`.
Current main keymap.cpp matches recovered SHA-256
`e459b089d9fb4645b0a5f5721f6d7a3e2fc59547b09756a545e3d21b9923da80`.
Recovered EKDATA MMP before fix:
`057f3501ea14204ed32a0a056b0997bc129056e150bc574dd48fda942f82bf44`.
Local genuine ekdatau.def:
`e2c069642d2ec349826f2d94cb9ccf3c3b5fe7db1f6013772b0ad1dd4cd0344f`,
byte-identical to upstream `kernelhwsrv/kernel/eka/eabi/ekdata.def`.

Actual copied Raptor `python/raptor_meta.py:2491-2502` appends `u`, but also
prepends platform-default directory if DEFFILE contains no slash. Bare v2
`ekdata.def` therefore did not select its local `ekdatau.def`. Authorized
**`DEFFILE ./ekdata.def`** preserves strict Unicode resolution and bypasses that
directory fallback; no NOSTRICTDEF, invented ordinals or auto-export tolerance.

Private build used verified surviving source, changing only that DEFFILE line.
Main source changes: corrected EKDATA MMP/comment and added byte-identical local
`keymap/ekdatau.def`. Main had the older upstream-relative MMP and no local def;
these were reconciled explicitly, not overwritten wholesale with v2 source.
Main HAL MMP/checker differ from surviving v2: **left untouched**, no HAL source
or logic adoption implied. README earlier UNBUILT is historical; this report is
later EKDATA build evidence. Original v2 prose remains unavailable and waived.

## Isolation and one authorized attempt

New unused root **`/home/yuzi/.cache/sybian-r1/r1-ekdata-build-v3`** (`R`) checked
absent before provisioning; home free 271,449,190,400 bytes. Quota showed tmpfs
176/6297 MiB and 283/6297 MiB, no reported home quota. Private Wine prefix/tmp,
SDK/tools/Raptor/source copies. Fresh wineboot rc0, M: maps private tools/mingw.
No prefix sharing or v1/v2 writes. No generated target objects copied.

Verified 7,293 reused regular files from v2, manifest `logs/reused_inputs.json`.
Copied old EKDATA outputs moved to private `preserved_release/` before build;
expected new target absent, preventing stale success. HAL files copied as inert
inputs only, never rebuilt. Wrappers/env absolute roots substituted to v3;
absolute Python/make links repointed, no tool source/byte patches. Upstream
read-only reference in private workspace preserves relative layout.

Variant `port-x86-rfile64-v1`, actual target `x86_urel`; reviewed pins verified:

| Input | SHA-256 |
|---|---|
| EUSER | `4851fec4b6439bf5263e6f10f7f3010828f84df55df050c710f57077f2dd017b` |
| petran.exe | `b200bf0878679e6c7d6c20e5a78cb2bb93f16d5b21790a2b66c7dd04a53ec155` |
| platform_paths.hrh | `8a8ebdc3f40804d630def1af4c0c3a68568a79d13eb08d0661d9ba252dd96f31` |
| variant header | `500889884b3063c1b0712c000d2c62f55e70774cbfda41b51b82f343be277106` |
| variant.cfg | `2c505b4bb43e4b74e362589ce78f866053e4e9d8283688e47cca321c58e08cd0` |

Source `$R/env.sh`, exact target invocation:

```sh
bash "$SBS_HOME/bin/sbs" --toolcheck=off -d -j 1 \
  -f "$R/logs/build_ekdata.xml" \
  -b "$R/workspace/qemu-port/r1-console/keymap/bld.inf" \
  -c x86_urel -p ekdata.mmp
```

**One attempt, rc0**, one compile/link, 2 seconds, 0 errors / 1 warning: known
missing checklib. No DEFFILE warning or compiler diagnostics, stderr empty.
Private `harness/build.py` fails immediately on nonzero, refuses existing log/
target. Exact argv/status: `logs/build_ekdata.cmd.json`; expanded commands and
attempt-1 status: XML. XML:430,436 resolves **local ekdatau.def**; :515 invokes
genuine prepdef with nodatasizes, :551 links generated `ekdata.lib.exp`, :552
Petran genuine E32. This is a real frozen export link, not inferred from count.

## Emitted audit

Image SHA-256 **`2e11e7287325a7829a28b7cb510c5605390e57b1953a0b16704b8210c4aced0f`**,
2,780 bytes (`0xadc`), path `$R/epoc32/release/x86/urel/ekdata.dll`.

| Ordinal | Genuine linked symbol | PE RVA | E32 code-relative offset |
|---|---|---|---|
| 1 | KeyDataConvTable(SConvTable&, unsigned&, unsigned&, SScanCodeBlockList&, SKeyCodeList&) | `0x13e0` | `0x3e0` |
| 2 | KeyDataFuncTable(SFuncTables&) | `0x13a0` | `0x3a0` |
| 3 | KeyDataSettings(TRadix&, TCtrlDigitsTermination&, int&, int&) | `0x1370` | `0x370` |

Independent byte parser compares PE export address table and COFF linked symbol
section/offset to E32 slots for every genuine def symbol. **Exactly three slots,
no fourth/ignore export**. Initial host audit lookup corrected to account for
COFF leading underscore; no target/image changes or rebuild.

X86 E32 V-header/PE2, version 10.0, uncompressed, flags `22000101`, unpaged code,
default data paging, not SMP-safe/debuggable. UID1/2/3
`10000079 / 1000008d / 100039e0`; SID `100039e0`, VID `70000001`.
Capabilities raw low/high `[000fffff,00000000]` (Petran displays high-first).
Initialized data/BSS **0/0**, no data relocation stream. No writable-DLL-data
extension required by this image. Not a privilege grant to callers.

Code offset `0x9c`, size `0x9a8`, entry `0x264`, base `0x10000000`; export table
`0xa38`, import `0xa44`, code relocation `0xa70`. 46 unique in-code relocations,
all type `0x3000`, block/count/extent check ends exactly at EOF. UID checksum
`ea05f972` and header CRC `5d1663e2` independently checked (upstream polynomial,
zero initial CRC and c90fdaa2 header CRC initializer). Bounded structural audit,
not full loader validation or translation test.

Only imports **euser{000a0000}[100039e5].dll**, ordinals **644 User::Free** and
**646 User::Alloc**, actual IAT and final terminator checked. Provider hash pinned
above; prior v1/v2 provider ordinal evidence establishes these present non-ABSENT
exports. No fresh EUSER rebuild or official-kit equivalence claim.

Petran -v and objdump -p/-t are inspection only, rc0, stdout/err/argv archived.
`harness/audit.py`, `logs/v3_audit.json` retain exact mangled names and byte checks.
`staged/sys/bin/EKDATA.DLL` and **EKDATA.00** are real byte-identical copies, both
same image hash above, not symlinks and never loaded.

## HAL separation, preservation and coordinator handoff

Historical HAL v2 evidence hash-verified only, not independently rerun/rebuilt:
strict_ordinal_audit.json records six correct slots, generated_table_audit.json
records full 484-byte InitialValue; recovery note documents prepdef nodatasizes
removes size metadata. Preserved HAL hash
`4a95dbe01d201b8a0b347e0e5c4cc39f798a7d89d6e163d7f63d71d3a04cb150`.
**EKDATA v3 success does not validate or repair main HAL recipe/checker differences**.

310 recorded v1/v2 source/log/staged/release files rehashed unchanged, inventory
`logs/prior_hashes.json`; no prior prefix access/build, no cleanup. Final manifest
`logs/final_hashes.sha256` covers v3 source/helpers/logs/images/aliases.

- Changed repo paths: keymap/ekdata.mmp, new keymap/ekdatau.def, this new report.
- Result: one EKDATA-only build rc0; exact three genuine ordinal mappings PASS.
- Blockers: none for this build; known checklib warning remains. HAL source
  reconciliation, complete console dependency/startup/runtime validation separate.
- Next coordinator: review source diff and pinned artifact, then separately stage
  it with reviewed HAL/EKTRAN/EDISP/EWSRV/ESHELL. No runtime slot or ABI freeze.
- Manual/stable/QEMU: none run or changed; translation, input/display, security
  and console acceptance remain pending. No guest execution or power/domain claim.
