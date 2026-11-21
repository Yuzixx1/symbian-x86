# Z-machine port for Sybian REAL

`zmachine.c` is a genuine, from-scratch Z-machine interpreter written against
the **Z-Machine Standards Document v1.0/v1.1** (fetched from the IF Archive,
`if-archive/infocom/z-machine/ZSpec10.zip` and `ZSpec11.txt`). It is not a
text-adventure imitation: it decodes real Z-code. Frotz 2.44 (dumb interface)
was used as a native reference oracle while debugging; no Frotz source is
vendored.

## What is implemented (Z-machine v1–v3)

- Header validation, byte/packed address conversions, dynamic memory snapshot
- ZSCII text decoding: A0/A1/A2 alphabets, abbreviations, 10-bit escapes,
  newlines, object short names, `print`/`print_ret`/`print_addr`/`print_paddr`
- Object tree and 31 property defaults; attributes, parent/sibling/child,
  `get_prop*`, `get_next_prop`, `put_prop`, `insert_obj`, `remove_obj`
- Dictionary (v3 entries) and lexical analysis: input encoding, binary-safe
  linear lookup, word separators, v1–v3 parse buffer layout
- Full v1–v3 opcode set: 2OP/1OP/0OP/VAR forms, all operand type encodings,
  store/branch fields, calls with locals and 16-bit stack frames, arithmetic,
  comparison, `sread`, `random`, `verify`, `show_status`, status line
- Save/restore are skipped (they fail, as permitted), and v4+ screen/sound
  opcodes are out of scope. Restart from the pristine dynamic image works.

The interpreter uses a static 128 KiB story image (the v3 story-file limit),
a 1024-word stack and 64 call frames. It calls **no malloc()**, so it stays
inside the process's 64 KiB heap budget with room to spare.

## Game file

`minizork.z3` is **Mini-Zork I**, a real 1988 Infocom Z-code v3 demo
(release 34, serial 871124), downloaded from the IF Archive:
`https://www.ifarchive.org/if-archive/infocom/demos/minizork.z3`.
It is copyrighted by Infocom/Activision and is **not public domain**; it was
freely distributed by Infocom as a promotional demo and is hosted by the IF
Archive for historical use. It is NOT redistributed as part of the E32 —
the test stages it on a scratch FAT image only.

To run a full commercial story (e.g. Zork I/II/III .z3), obtain a legally
licensed copy and copy it to the FAT volume as `ZORK.Z3`. The app tries
`ZORK.Z3` first and falls back to `MINIZORK.Z3`. Any v3 story up to 128 KiB
should work; the interpreter deliberately rejects other versions.

## Verification

- Native harness: `zmachine.c` driven with a scripted input produces the same
  Mini-Zork transcript as Frotz 2.44 (`look`, `north`, `get lamp`, `quit`).
- QEMU: `python3 tools/r1_zmachine_test.py` stages `ZMACHINE.E32`,
  `EUSER.DLL` and `MINIZORK.Z3`, boots the R1 kernel, types the commands and
  asserts the `ZM:` markers, real game text and a fault-free log.
- `python3 tools/e32read.py build/user/ZMACHINE.E32` validates the PE2 image
  and its single EUSER.DLL ordinal-915 import.
