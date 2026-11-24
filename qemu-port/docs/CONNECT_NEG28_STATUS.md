# Connect r=-28 (KErrBadName) — status: IN PROGRESS (root cause fixed in code, runtime not yet green)

- Evidence: user/guilib.c mtime 20:30:42, user/guihello.c 20:26:10, user/ws_e32_client.c 20:31:33 (all after 20:23; agent actively editing).
- Name literal now correct: ws_e32_client.c:146-150 builds "!E32WindowServer" as a 2-byte TPtrC16 (`unsigned short` array + EPtrC length), matching genuine/ewsrv/patched/co_cli.cpp:24 `CreateSession(_L("!E32WindowServer"),...)`; guilib.c:176-186 adds a TLitC16 variant for its own gl_connect().
- Latest boot log /tmp/opencode/chunkb_guihello.log (20:31): `GH: FAIL gui_init rc=-1` — the -28 is gone; connect now returns 0 but handle==0 (gl_connect's -1 path, guilib.c:188). dt4_desktop.log (20:30): `DT: Connect r=4294967295` (same -1).
- No log after 20:23 shows `Connect r=0` or a successful GH: path. final_verify after 20:30: slot269_retry (20:30) OVERALL FAIL (10 pass, 1 fail); selfcheck (20:31) individually PASSing.
- Root cause (known): hand-rolled descriptors lacked EPtrC/EBufC type bits in iLength, so genuine EWSRV name validation rejected the session name with KErrBadName; type bits now set, remaining -1 indicates connect succeeds but no session handle is returned — next debug step.
