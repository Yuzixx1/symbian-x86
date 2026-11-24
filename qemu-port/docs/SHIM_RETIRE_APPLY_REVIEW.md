# r1_shim_retire_apply.py safety review (read-only)

VERDICT: NEEDS FIXES — guards are mostly real, but two HIGH issues must be fixed before any apply.

HIGH tools/r1_shim_retire_apply.py:357-364 — post-apply gate call is outside try/except; a TimeoutExpired/OSError/crash after writes leaves edits applied with NO rollback. Wrap post gate + rollback in try/finally and verify rollback rc before returning.
HIGH :83-87,129-139 — edits comment out the GENUINE EFILE/EFSRV/ESTART/ESHELL/EWSRV boot entries plus SvcMgrBootLaunch/ExecInstall (e32load.cpp:1285-1290,1343; qemu_variant_asic.cpp:194-196). The same gate requires those components to boot, so post gate necessarily FAILs → apply always rolls back; if the gate were weakened, the disk would have no file server. Reconcile with docs/SHIM_RETIREMENT_PLAN.md.
MED :271-294,397-398 — --rollback trusts arbitrary manifest "path" entries (only containment via safe_target is checked) and needs no token; a crafted dir can overwrite any qemu-port/ file. Restrict to planned edit paths and require --confirm.
MED :266,287-290,361 — backup records sha256(src) but never verifies the copy; do_rollback is non-atomic and its return code is ignored, so a corrupt snapshot can leave broken files and still print "gate after rollback: PASS". Verify dst hash at backup, make rollback all-or-nothing, check its rc.
MED :315 — only kernelhwsrv/ cleanliness is checked; all three edit targets are already dirty in this workspace. Add a clean check / explicit per-file confirmation for edited qemu-port files.
LOW :335,355,364 — "refusals exit 2" is inaccurate: pre-gate FAIL=3, write error=4, post FAIL=5. Document or normalize.
OK: gate really re-runs with --only and no --dry-run (:168-184); verdict parsed from fresh stdout, not stale logs; --from-file ignored under --apply (:428); every edit string is unique in current source; stale unindented ewsrv comment and estart pseudo-code correctly excluded (:77-87); no deletes/commits/pushes/make clean; writes confined to qemu-port/ and /tmp/opencode.
DRY-RUN: py_compile OK; no-args preview 4/4 BLOCKED (SKIP); --shim winsrv preview BLOCKED; --shim winsrv --apply without token REFUSE rc=2. No apply, no QEMU; kernelhwsrv/ clean.
