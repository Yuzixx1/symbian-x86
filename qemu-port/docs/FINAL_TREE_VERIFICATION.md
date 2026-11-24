# Final Tree Verification (2026-11-24)

Read-only verification of the pushed state and expected leftovers. No QEMU run,
no edits outside this report. `kernelhwsrv/` was not touched; `make clean` was
not run; nothing was committed or pushed.

## Summary table

| Check | Result |
| --- | --- |
| Local HEAD | `d26fade5db13a140845a30e0faabe4550fb3940d` |
| Remote HEAD (`refs/heads/main`) | `d26fade5db13a140845a30e0faabe4550fb3940d` |
| Local == remote | PASS |
| `git diff --check` | PASS (no whitespace errors) |
| `archive/handwritten-r25` (branch) | `56fbaffbcf41cc4f97d60a6deeef6912376a0e44` |
| `archive/handwritten-r25` (annotated tag -> commit) | `56fbaffbcf41cc4f97d60a6deeef6912376a0e44` |
| `kernelhwsrv/` status | clean |
| `qemu-port/{variant,fs,tools,genuine}/` status | clean (no uncommitted source edits) |

## Commit chain (`git log --oneline -3`)

```
d26fade Retire portfilesrv/r1_fs shims: genuine EFSRV is sole file server
7f76900 Genuine Symbian userspace: EFSRV/EWSRV/estart/etshell/EUSER running on EKA2 x86
56fbaff R25: multi-key hold support + kernel typematic bitmap; DOOM input proven clean
```

Both authorized commits are present and pushed: `7f76900` (pivot) and `d26fade`
(retirement). Remote `main` points at `d26fade5db13a140845a30e0faabe4550fb3940d`.

## Expected working-tree leftovers (`git status --short`)

| Status | Path | Classification |
| --- | --- | --- |
| `M` | `qemu-port/.sybian/coordinator/state.json` | expected runtime/coordinator state leftover |
| `M` | `qemu-port/docs/DOCS_INDEX.md` | concurrent doc edit (expected) |
| `M` | `AGENTS.md` | concurrent doc edit (expected; see caveat) |
| `??` | `qemu-port/-D__cdecl=__attribute__((cdecl))` | expected stray-file artifact |
| `??` | `qemu-port/.sybian/qemu-slot.lock` | expected lock-file artifact |
| `??` | `symbian-x86-archive-56fbaff.tar.gz` | expected preservation tarball |

No other modifications or untracked files were reported. Nothing under
`kernelhwsrv/` or the `variant/`, `fs/`, `tools/`, `genuine/` source trees is
modified or untracked.

## Caveats

- The root `AGENTS.md` "Current state (2026-11-24)" block still labels shim
  retirement as *pending / in progress* ("treat it as uncommitted until
  confirmed"). That is stale: retirement is now committed and pushed as
  `d26fade`. It is a concurrent doc edit only — no source change.
- `refs/tags/archive/handwritten-r25` is an annotated tag (`4f08597...`), so its
  raw object hash differs from the commit; it resolves to
  `56fbaffbcf41cc4f97d60a6deeef6912376a0e44`, matching the preservation point.
- `git ls-remote archive/handwritten-r25` produced an ambiguous-refname warning
  because both a branch and a tag share the name; both were checked explicitly.
- The three `??` files and the two coordinator/doc `M` files are known leftovers
  and were intentionally left in place — they are not part of either commit.
- Standing rules remain: never accept `SessionPath r=-15` as IPC success; never
  re-add the EFILE trampoline; no `git add -A`; no commit/push without explicit
  request.
