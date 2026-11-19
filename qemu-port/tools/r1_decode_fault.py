#!/usr/bin/env python3
"""r1_decode_fault.py -- post-mortem analyser for kernel serial-console fault dumps.

Reads a serial log, extracts Exc blocks and kernel-side fault events, and
produces an annotated report: exception name, registers, faulting instruction
disassembled from the relevant E32 code section, and stale-absolute-address
checks (the PE-without-relocations bug class).

Usage:
  python3 tools/r1_decode_fault.py LOGFILE [--image NAME=PATH[:CODEBASE[:CODEOFFSET]]]... [--json]
  LOGFILE may be '-' for stdin.
"""
import argparse
import json
import os
import re
import struct
import subprocess
import sys
import tempfile

SCRATCH = "/tmp/opencode"
EXC_NAMES = {
    0x00: "divide error",
    0x01: "debug",
    0x02: "NMI",
    0x03: "breakpoint",
    0x04: "overflow",
    0x05: "bound range exceeded",
    0x06: "invalid opcode",
    0x07: "device not available",
    0x08: "double fault",
    0x09: "coprocessor segment overrun",
    0x0a: "invalid TSS",
    0x0b: "segment not present",
    0x0c: "stack-segment fault",
    0x0d: "general protection",
    0x0e: "page fault",
    0x0f: "reserved",
    0x10: "x87 floating-point",
    0x11: "alignment check",
    0x12: "machine check",
    0x13: "SIMD floating-point",
}

REGS = ["EAX", "EBX", "ECX", "EDX", "ESP", "EBP", "ESI", "EDI"]


def parse_e32(path):
    with open(path, "rb") as f:
        b = f.read()
    if len(b) < 124:
        raise ValueError("file too small for E32 V header")
    code_size, data_size, heap_min, heap_max, stack_size, bss_size = \
        struct.unpack_from("<6i", b, 48)
    entry = struct.unpack_from("<I", b, 72)[0]
    code_base, data_base = struct.unpack_from("<2I", b, 76)
    dll_refs = struct.unpack_from("<i", b, 84)[0]
    code_off, data_off, imp_off, code_reloc_off, data_reloc_off = \
        struct.unpack_from("<5I", b, 100)
    return {
        "code_size": code_size,
        "data_size": data_size,
        "entry": entry,
        "code_base": code_base,
        "data_base": data_base,
        "dll_refs": dll_refs,
        "code_off": code_off,
        "data_off": data_off,
        "imp_off": imp_off,
    }


def parse_int_field(s, base):
    s = s.strip()
    if s.lower().startswith("0x"):
        return int(s, 16)
    return int(s, base)


def parse_image_spec(spec, auto_runs):
    if "=" not in spec:
        raise ValueError("image spec must be NAME=PATH[:CODEBASE[:CODEOFFSET]]")
    name, rest = spec.split("=", 1)
    name = name.strip()
    parts = rest.split(":")
    codebase = None
    codeoffset = None
    # peel trailing numeric fields
    if len(parts) >= 2:
        # try last two as ints
        try:
            cb = parse_int_field(parts[-2], 16)
            co = parse_int_field(parts[-1], 10)
            codebase = cb
            codeoffset = co
            parts = parts[:-2]
        except ValueError:
            try:
                cb = parse_int_field(parts[-1], 16)
                codebase = cb
                parts = parts[:-1]
            except ValueError:
                pass
    elif len(parts) == 1:
        pass
    path = ":".join(parts)
    info = parse_e32(path)
    if codebase is None:
        codebase = info["code_base"]
    if codeoffset is None:
        codeoffset = info["code_off"]
    codesize = info["code_size"]
    return {
        "name": name,
        "path": path,
        "codebase": codebase,
        "codeoffset": codeoffset,
        "codesize": codesize,
        "run_addresses": list(auto_runs),
    }


def read_log(path):
    if path == "-":
        return sys.stdin.read()
    with open(path, "r", errors="replace") as f:
        return f.read()


def detect_runs(text):
    runs = []
    for line in text.splitlines():
        if "ProcessCreate" in line or "Loaded r=" in line:
            m = re.search(r"run=([0-9a-fA-F]+)", line)
            if m:
                v = int(m.group(1), 16)
                if v not in runs:
                    runs.append(v)
    return runs


def split_blocks(text):
    lines = text.splitlines()
    blocks = []  # list of list of lines
    kernel_events = []
    cur = None
    for line in lines:
        if re.match(r"^\s*Exc\s+[0-9a-fA-F]{2}\b", line):
            if cur is not None:
                blocks.append(cur)
            cur = [line]
            continue
        if re.search(r"FAULT:|HEAP CORRUPTED", line):
            if cur is not None:
                blocks.append(cur)
                cur = None
            kernel_events.append(line.strip())
            continue
        if cur is not None:
            if line.strip() == "":
                blocks.append(cur)
                cur = None
            else:
                cur.append(line)
    if cur is not None:
        blocks.append(cur)
    return blocks, kernel_events


def parse_fault(block):
    text = "\n".join(block)
    m = re.search(r"Exc\s+([0-9a-fA-F]{2})", block[0])
    code = int(m.group(1), 16)
    regs = {}
    for r in REGS + ["CS", "EIP", "DS", "SS", "ES", "FS", "GS"]:
        mm = re.search(r"\b" + r + r"=([0-9a-fA-F]+)", text)
        if mm:
            regs[r] = int(mm.group(1), 16)
    m = re.search(r"EFLAGS=([0-9a-fA-F]+)", text)
    eflags = int(m.group(1), 16) if m else None
    m = re.search(r"FAR=([0-9a-fA-F]+)", text)
    far = int(m.group(1), 16) if m else None
    m = re.search(r"ErrCode=([0-9a-fA-F]+)", text)
    errcode = int(m.group(1), 16) if m else None
    m = re.search(r"SS3=([0-9a-fA-F]+)", text)
    ss3 = int(m.group(1), 16) if m else None
    m = re.search(r"ESP3=([0-9a-fA-F]+)", text)
    esp3 = int(m.group(1), 16) if m else None
    m = re.search(r"Thread\s+(.*),\s*KernCSLocked=(.*),\s*IrqNest=(.*)", text)
    thread = m.group(1).strip() if m else None
    kerncs = m.group(2).strip() if m else None
    irqnest = m.group(3).strip() if m else None
    return {
        "code": code,
        "name": EXC_NAMES.get(code, "unknown (%02x)" % code),
        "regs": regs,
        "eflags": eflags,
        "far": far,
        "errcode": errcode,
        "ss3": ss3,
        "esp3": esp3,
        "thread": thread,
        "kerncs": kerncs,
        "irqnest": irqnest,
        "raw_block": block,
    }


def attribute_addr(addr, images, runs):
    candidates = []
    for img in images:
        bases = [img["codebase"]] + list(img.get("run_addresses", [])) + list(runs)
        for base in bases:
            if base <= addr < base + img["codesize"]:
                return img, addr - base, base
    return None, None, None


def disassemble_image(img):
    # returns list of dict {addr, text, bytes}
    path = img["path"]
    off = img["codeoffset"]
    size = img["codesize"]
    with open(path, "rb") as f:
        data = f.read()
    code = data[off:off + size]
    tmpdir = os.path.join(SCRATCH, "w4_decode")
    os.makedirs(tmpdir, exist_ok=True)
    fd, binpath = tempfile.mkstemp(prefix="img_", suffix=".bin", dir=tmpdir)
    with os.fdopen(fd, "wb") as f:
        f.write(code)
    try:
        out = subprocess.run(
            ["objdump", "-D", "-b", "binary", "-mi386", "-Mintel", binpath],
            capture_output=True, text=True,
        ).stdout
    finally:
        try:
            os.remove(binpath)
        except OSError:
            pass
    instrs = []
    for line in out.splitlines():
        m = re.match(r"^\s*([0-9a-f]+):\s*((?:[0-9a-f]{2} )+)\s*(.*)$", line)
        if m:
            addr = int(m.group(1), 16)
            text = m.group(3).strip()
            instrs.append({"addr": addr, "text": text, "bytes": m.group(2).strip()})
    return instrs


def context_around(instrs, fault_off, n=3):
    idx = None
    for i, ins in enumerate(instrs):
        if ins["addr"] == fault_off:
            idx = i
            break
    if idx is None:
        return None, None
    start = max(0, idx - n)
    end = min(len(instrs), idx + n + 1)
    return instrs[start:end], idx - start


def check_abs(text, img):
    # returns (kind, value, inside) or None
    span_lo = img["codebase"]
    span_hi = img["codebase"] + img["codesize"]
    # jmp/call dword ptr [ABS]
    m = re.match(r"^(jmp|call)\b", text)
    if m and "DWORD PTR" in text.upper():
        mm = re.search(r"DWORD PTR\s*(?:ds:)?\[?(0x[0-9a-fA-F]+)\]?", text, re.IGNORECASE)
        if mm:
            val = int(mm.group(1), 16)
            return m.group(1), val, (span_lo <= val < span_hi)
    # mov reg, IMM
    m = re.match(r"^mov\s+([a-z]{2,3})\s*,\s*(0x[0-9a-fA-F]+)\s*$", text, re.IGNORECASE)
    if m:
        val = int(m.group(2), 16)
        return "mov", val, (span_lo <= val < span_hi)
    return None


def find_suspicious(images):
    results = []
    for img in images:
        instrs = disassemble_image(img)
        span_lo = img["codebase"]
        span_hi = img["codebase"] + img["codesize"]
        for ins in instrs:
            info = check_abs(ins["text"], img)
            if info is None:
                continue
            kind, val, inside = info
            # only report jmp/call stale absolutes (outside span)
            if kind in ("jmp", "call") and not inside:
                results.append({
                    "image": img["name"],
                    "offset": ins["addr"],
                    "instruction": ins["text"],
                    "kind": kind,
                    "value": val,
                    "inside": inside,
                    "span": (span_lo, span_hi),
                })
            # also report mov immediates that look like code-span-adjacent? skip to avoid noise
    return results


def fmt_reg_line(fault):
    r = fault["regs"]
    parts = []
    for name in REGS:
        if name in r:
            parts.append("%s=%08x" % (name, r[name]))
    # group
    line1 = "  " + " ".join(parts[:4])
    line2 = "  " + " ".join(parts[4:8])
    return line1, line2


def main():
    ap = argparse.ArgumentParser(description="Decode kernel fault dumps")
    ap.add_argument("logfile", help="log file or '-' for stdin")
    ap.add_argument("--image", action="append", default=[], metavar="NAME=PATH[:CODEBASE[:CODEOFFSET]]")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    try:
        text = read_log(args.logfile)
    except Exception as e:
        print("r1_decode_fault: cannot read log: %s" % e, file=sys.stderr)
        return 2

    runs = detect_runs(text)
    images = []
    for spec in args.image:
        try:
            img = parse_image_spec(spec, runs)
            images.append(img)
        except Exception as e:
            print("r1_decode_fault: bad --image %r: %s" % (spec, e), file=sys.stderr)
            return 2

    blocks, kernel_events = split_blocks(text)
    faults = [parse_fault(b) for b in blocks]

    if not faults:
        if not args.json:
            print("r1_decode_fault: no fault block found", file=sys.stderr)
            return 2
        # json mode: still maybe print kernel events
        for ke in kernel_events:
            print(json.dumps({"kind": "kernel_event", "text": ke}))
        return 0

    # Pre-disassemble images
    img_instrs = {img["name"]: disassemble_image(img) for img in images}

    suspicious = find_suspicious(images)

    # Attribute + extract faulting instruction per fault
    for f in faults:
        eip = f["regs"].get("EIP")
        img, off, base = (None, None, None)
        if eip is not None:
            img, off, base = attribute_addr(eip, images, runs)
        f["attr"] = (img, off, base)
        if img is not None and off is not None:
            instrs = img_instrs[img["name"]]
            ctx, rel = context_around(instrs, off, 3)
            f["ctx"] = ctx
            f["ctx_rel"] = rel
            # faulting instruction text
            fault_ins = None
            for ins in instrs:
                if ins["addr"] == off:
                    fault_ins = ins["text"]
                    break
            f["fault_insn"] = fault_ins
            f["abs_check"] = check_abs(fault_ins, img) if fault_ins else None
        else:
            f["ctx"] = None
            f["ctx_rel"] = None
            f["fault_insn"] = None
            f["abs_check"] = None

    if args.json:
        for f in faults:
            img, off, base = f["attr"]
            obj = {
                "exception": "%02x" % f["code"],
                "exception_name": f["name"],
                "thread": f["thread"],
                "mode": "umode" if (f["regs"].get("CS", 0) & 3) == 3 else "kmode",
                "registers": {k: "0x%08x" % v for k, v in f["regs"].items()},
                "eflags": ("0x%08x" % f["eflags"]) if f["eflags"] is not None else None,
                "far": ("0x%08x" % f["far"]) if f["far"] is not None else None,
                "errcode": ("0x%08x" % f["errcode"]) if f["errcode"] is not None else None,
                "eip_raw": ("0x%08x" % f["regs"].get("EIP")) if "EIP" in f["regs"] else None,
                "eip_image": img["name"] if img else None,
                "eip_offset": ("0x%x" % off) if off is not None else None,
                "eip_base": ("0x%x" % base) if base is not None else None,
                "faulting_instruction": f["fault_insn"],
                "context": [i["text"] for i in (f["ctx"] or [])],
                "absolute_check": (
                    {"kind": f["abs_check"][0], "value": "0x%x" % f["abs_check"][1],
                     "inside_span": f["abs_check"][2]} if f["abs_check"] else None
                ),
                "kernel_events": kernel_events,
                "suspicious_refs": suspicious,
            }
            print(json.dumps(obj))
        # also kernel events as separate? already included
        return 0

    # Human-readable report
    for i, f in enumerate(faults, 1):
        cs = f["regs"].get("CS", 0)
        mode = "umode" if (cs & 3) == 3 else "kmode"
        print("=== Fault %d: Exc %02x (%s) ===" % (i, f["code"], f["name"]))
        if f["thread"] is not None:
            print("Thread: %s, KernCSLocked=%s, IrqNest=%s" % (f["thread"], f["kerncs"], f["irqnest"]))
        print("Mode: %s (CS=0x%08x)" % (mode, cs))
        l1, l2 = fmt_reg_line(f)
        print("Registers:")
        print(l1)
        print(l2)
        # extra regs
        extras = []
        for name in ["CS", "DS", "SS", "ES", "FS", "GS"]:
            if name in f["regs"]:
                extras.append("%s=0x%08x" % (name, f["regs"][name]))
        if extras:
            print("  " + "  ".join(extras))
        if f["eflags"] is not None:
            print("  EFLAGS=0x%08x FAR=0x%08x ErrCode=0x%08x" % (
                f["eflags"], f["far"] if f["far"] is not None else 0,
                f["errcode"] if f["errcode"] is not None else 0))
        if f["ss3"] is not None or f["esp3"] is not None:
            print("  SS3=0x%08x ESP3=0x%08x" % (
                f["ss3"] if f["ss3"] is not None else 0,
                f["esp3"] if f["esp3"] is not None else 0))
        # EIP attribution
        eip = f["regs"].get("EIP")
        img, off, base = f["attr"]
        if eip is not None:
            if img is not None:
                print("EIP=0x%08x (raw) -> %s+0x%x (base 0x%x)" % (eip, img["name"], off, base))
            else:
                print("EIP=0x%08x (raw) -> not attributable to any image" % eip)
        # faulting instruction
        if img is not None and f["ctx"]:
            print("Faulting instruction (%s+0x%x):" % (img["name"], off))
            rel = f["ctx_rel"]
            for j, ins in enumerate(f["ctx"]):
                marker = "-->" if j == rel else "   "
                print("  %s 0x%04x: %s" % (marker, ins["addr"], ins["text"]))
            if f["abs_check"]:
                kind, val, inside = f["abs_check"]
                span_lo = img["codebase"]
                span_hi = img["codebase"] + img["codesize"]
                print("  Absolute %s: 0x%x -> %s span [0x%x,0x%x)" % (
                    kind.upper(), val,
                    "inside" if inside else "outside",
                    span_lo, span_hi))
        elif eip is not None and img is None:
            print("No faulting instruction extractable (EIP not in any image).")
        print()

    if kernel_events:
        print("Kernel-side events:")
        for ke in kernel_events:
            # quote source location for HEAP CORRUPTED
            m = re.search(r"HEAP CORRUPTED\s+(.+)$", ke)
            if m:
                loc = m.group(1).strip()
                print('  %s  (source location: "%s")' % (ke, loc))
            else:
                print("  " + ke)
        print()

    if suspicious:
        print("Stale absolute references (jmp/call dword ptr [ABS] outside declared span):")
        for s in suspicious:
            print("  %s @+0x%04x: jmp dword ptr [0x%x] -> target 0x%x %s span [0x%x,0x%x)" % (
                s["image"], s["offset"], s["value"], s["value"],
                "inside" if s["inside"] else "outside", s["span"][0], s["span"][1]))
        print()

    # Summary
    print("Summary:")
    from collections import Counter
    c = Counter(f["name"] for f in faults)
    for name, n in c.items():
        print("  %s: %d" % (name, n))
    print("Per fault:")
    for f in faults:
        cs = f["regs"].get("CS", 0)
        mode = "umode" if (cs & 3) == 3 else "kmode"
        eip = f["regs"].get("EIP")
        img, off, base = f["attr"]
        if img is not None:
            loc = "%s+0x%x" % (img["name"], off)
        else:
            loc = "unattributed"
        mnem = f["fault_insn"].split()[0] if f["fault_insn"] else "n/a"
        print("  EIP=0x%08x %s %s %s" % (eip if eip is not None else 0, mode, loc, mnem))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
