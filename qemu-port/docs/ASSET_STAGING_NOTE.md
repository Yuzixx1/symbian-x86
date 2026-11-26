# Asset Staging Note (doom IWAD)

`/tmp/opencode/doom/freedoom-0.13.0/freedoom1.wad` is volatile — `/tmp` can be
cleared on reboot — so it is staged at `qemu-port/wads/freedoom1.wad`
(28,795,076 B, IWAD magic, sha256 `7323bcc1…f9e703d`, `cmp` identical).
That repo copy is git-ignored by `/qemu-port/wads/` in the root `.gitignore`:
user-supplied free-license asset, never committed (policy D2), so a fresh clone
has no IWAD and must be re-staged from the drop-off path.

P3 FreeDM stays SKIP — `freedm.wad` absent, pending user drop-off; no download,
gate reports SKIP with exit 0.
