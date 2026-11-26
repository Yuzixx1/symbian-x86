# PROJECT GOAL — Sybian REAL

## The Goal

Build a **usable PC OS** using the genuine Symbian EKA2 kernel as the base.

This means a computer a person can actually sit down and use:
- **GUI / desktop environment** — windows, mouse, desktop environment
- **Networking** — TCP/IP stack, NIC drivers, web browser
- **Audio subsystem**
- **Graphics / framebuffer drivers**
- **File manager with GUI** — file browser, not just a command-line filesystem server
- **Hardware drivers** beyond QEMU emulation
- **End-user applications** — text editor, etc.

The kernel is genuine EKA2 (x86), kept read-only under `kernelhwsrv/`. Everything else — userland, services, drivers, applications — is built on top.

---

## What We've Built (Infrastructure)

These are the foundation pieces, not the end goal:

| Component | Status |
|-----------|--------|
| EUSER.DLL (2503 exports) | Loads resident, PE2 import resolution |
| CActiveScheduler / CActive | vtable dispatch, Start()/Stop() round trip |
| portfilesrv | Genuine user process, file service host |
| User heap | Chunk-backed, 64K cap |
| Wait/exit API | User::WaitForExit() shim |
| Service manager | Ordered start, restart backoff, shutdown |
| Smoke tests | 15/15 PASS |

These are **infrastructure**, not the end goal. They make a PC OS possible, but they are not a PC OS.

---

## What Remains (The Actual PC OS)

| Component | Status |
|-----------|--------|
| GUI / desktop environment | Not started |
| Networking stack (TCP/IP) | Not started |
| NIC drivers | Not started |
| Web browser | Not started |
| Audio subsystem | Not started |
| Graphics / framebuffer drivers | Not started |
| File manager with GUI | Not started |
| Hardware drivers beyond QEMU | Not started |
| End-user applications | Not started |

---

## Build Order (from full_os_checklist.md)

1. **M-03** MMU + user isolation — design only, kernel-side
2. **P-06/P-08** Wait/exit-status API — implemented
3. **S-04 → I-08 → I-07** EUSER/CBase → CServer2 → active scheduler — implemented
4. **S-02** Service manager — SVCMGR.E32 builds
5. **S-02 + F-06** → retire PortFile → next
6. **H-02** Console server with input — next after services
7. **D-06 → N-01..N-04** NIC → stack → sockets → applications
8. **W-01** GUI — last

---

## Rule

If it doesn't help a person sit down and use the computer, it's infrastructure, not the goal. Infrastructure is necessary but not sufficient.

> **2026-11-26 — correction (supersedes the 11/11 and FINAL26 entries):** the old `Smoke tests | 11/11 PASS` row above was a stale single-harness count and has been updated to **15/15 PASS**. The authoritative gate is `tools/r1_final_verify.py`, **15 rows** (`--list` prints 15 rows and exits 0; `DEFAULT_TESTS` at `:134-150`), and the live authoritative run is **`FINAL27_RC=0`** — `/tmp/opencode/final27_gate.log` ends `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, i.e. **15/15 PASS**, at HEAD `ee8490e`, no commits. `FINAL20_RC=0`, `FINAL26_RC=0` and the 11/11 count are superseded history only.
