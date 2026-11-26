# WAVE2_ASSET_CHECK - IWAD durability gate (read-only, 2026-11-25)

- PASS recovered IWAD: `/tmp/opencode/doom/freedoom-0.13.0/freedoom1.wad` = 28,795,076 B, magic `IWAD`, numlumps 3163, dir offset 0x01B69B14 (inside file).
- PASS staged copy: `qemu-port/wads/freedoom1.wad` = 28,795,076 B; `cmp` identical; sha256 `7323bcc168c5a45ff10749b339960e98314740a734c30d4b9f3337001f9e703d` on both.
- PASS commit-proof: root `.gitignore:23` = `/qemu-port/wads/`; `git check-ignore -v` matches that rule; `git ls-files wads/` empty and `git status` clean for `wads/`. Caveat: that rule is in the working tree only - `git show HEAD:.gitignore` lacks it (`.gitignore` is modified, uncommitted), so ignore protection becomes durable only once `.gitignore` is committed.
- PASS P3 FreeDM stays SKIP: no `freedm.wad` / `freedoom2.wad` anywhere under `/tmp/opencode`; `freedm_recovered.img` (64 MiB) exists but carries only entry `FDOOM1  WAD` (0 `FREEDM  WAD` entries, sole `freedm.wad` hit is an IWAD name list at 0x2B7F3B) - no FreeDM IWAD.
- NOTE durability: `/tmp` is tmpfs (RAM-backed, 6.6 G free), so the recovered copy is volatile; the durable artifact is `qemu-port/wads/freedoom1.wad` on disk. Losing `/tmp` does not block wave-2.
- VERDICT: 4/4 PASS - wave-2 build may proceed against `qemu-port/wads/freedoom1.wad`; nothing else written.
