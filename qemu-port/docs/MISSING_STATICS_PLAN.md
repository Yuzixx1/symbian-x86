# Missing statics: CActiveScheduler::iScheduler, User::iTlsIndex

Declared only: `user/e32base.h:76` (`static TInt iTlsIndex;`), `:221` (`static CActiveScheduler* iScheduler;`).
No TU defines either — `grep` for `CActiveScheduler* CActiveScheduler::` / `TInt User::iTlsIndex` is empty, and `user/ca_glue.cpp:21-23` states they are deliberately omitted ("no linked image references them anymore").

EFSRVMIN (`genuine/efsrv_min/build_server.sh:18-24`, 7 inputs + LIBGCC): `nm -C` shows exactly one open static, `U CleanupStack::iStack` in `r1_fs_server.o`. `iScheduler` and `iTlsIndex` appear in no object. **LATENT for EFSRVMIN** — neither blocks the link; do not add them for it.

`iTlsIndex` has no in-tree reader at all (declaration only) → **LATENT/dead**.

`iScheduler` is reached only through inline CActiveScheduler members, so it is **REQUIRED** the first time an image of TUs using Install/Add/Start/Current/Stop is linked: `user/ca_test.cpp:23,49,56`, `tests/r1a_fixture/r1a_fixture.cpp:71,89,101`, `genuine/ewsrv/patched/ws_main.cpp:53,2122`, `genuine/efile/patched/sf_main.cpp:184`, `sf_request.cpp:2337`. No current `.o` under `build/`, `genuine/`, `tests/` has them `U` (only unrelated `FsPluginManager::iScheduler`, `B` in `sf_plugin_man.o`), so all are LATENT today.

If one is ever needed, the definition belongs in the same port-glue TU that must define `CleanupStack::iStack` for EFSRVMIN: `genuine/efsrv_min/efsrv_min.cpp` (it is on the link line; `ca_glue.cpp` is not). One line each, added only after `nm` shows `U`:
`CActiveScheduler* CActiveScheduler::iScheduler = 0;` · `TInt User::iTlsIndex = -1;`
