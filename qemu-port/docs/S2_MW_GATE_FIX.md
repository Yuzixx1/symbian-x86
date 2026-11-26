# S2 MW Gate Fix — split the make-wiring assertion

Evidence: `Makefile:342` sets `R1_FS_CHANNEL ?= 0`; the ifeq guard at
`Makefile:344` encloses both the `R1_FS_CHANNEL_OBJ := build/r1_fs_channel.o`
assignment and its compile rule (`:344-350`), appended to `KERNFULL_OBJS` at
`:351`. Measured: `make -n kernfull` names `r1_fs_channel` 0 times by default,
2 times with `R1_FS_CHANNEL=1`. So the pre-gate MW assertion in
`tools/r1_wave2_s2.py` (`run_ladder(..., wired=True)` → `gate_make_wiring`,
`:296`/`:215`) demands wiring the tree never had — the channel is opt-in.
Two static probes (`make -n` only, never clean) replace it:
- **MW-a**: default `make -n kernfull` MUST NOT name `r1_fs_channel.o` (0).
- **MW-b**: `R1_FS_CHANNEL=1 make -n kernfull` MUST name it (>= 1).
Both hold today, so the pair is the invariant: opt-in wiring intact, default link clean. **Waiver**: if the in-flight S2 run aborted on the old one-sided
MW assertion, MW is waived for it — gate defect, not tree regression; no tree
edit needed, a tooling follow-up owns the two-part gate.
