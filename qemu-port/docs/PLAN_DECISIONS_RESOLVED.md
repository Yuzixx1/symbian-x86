# PLAN DECISIONS RESOLVED — user's FINAL answers

Date: 2026-11-25. The user answered every open item in
[PLAN_CANONICAL_DECISIONS.md](PLAN_CANONICAL_DECISIONS.md); each answer
chose the recommended default. "Silence = default" is therefore resolved:
no open decisions remain. `kernelhwsrv/` read-only; no commit/push; no
`make clean`; no QEMU run.

## Final decisions

- **D1 GUI retired.** P2 (GUI on genuine EWSRV) will NOT run; commit `94ce9c9` stands. Console shell + DOOM/FreeDM is the UI.
- **D2 Second game.** FreeDM (BSD/CC0 PWAD) on the existing `DOOM.E32`. No shareware, no Wolf3D.
- **D3 Blit path.** exec-270 fbblit only. Moot while D1 keeps P2 off.
- **D4 No commits/push overnight.** Working tree only; the WATCH archive move stays blocked (it needs a landed commit).
- **D5 Per-test WAD swap.** Do not grow `FS_MB` for freedoom2.wad.
- **D6 Netstack.** The authored netstack is the permanent socket ABI; "genuine ESOCK" is closed as a goal; external EPL import deferred.

## Consequences for P0-P5

- P2 removed from scope entirely.
- P3 = FreeDM SKIP-clean scaffold only, until the asset drop lands.
- P0, P1, P4-CI and P5-static proceed overnight.
- All commits require an explicit user request.
