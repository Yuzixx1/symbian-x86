#!/usr/bin/env python3
"""fix_mchunk_adjust.py — build-glue repair for the Direct memory model.

Upstream gap (SymbianSource, untested path — x86/Direct never shipped):
DMemModelChunk::Adjust (memmodel/epoc/direct/mchunk.cpp) only VALIDATES the
new size but never records it, leaving iSize at the fully-committed maximum
from DoCreate. The model-independent RMessageK pool protocol (sipc.cpp
ExpandMessagePool) grows the pool by Adjust(curr+page) and threads messages
over [oldSize, Size()); with iSize stuck at max the first Adjust asks
max+page and fails, so the supervisor thread can never be created and no
Direct-model kernel boots. (Moving/flexible Adjust commits pages AND tracks
iSize; Direct has no MMU work but MUST still track iSize.)

Repair: in the preprocessed translation unit, prepend `iSize = aNewSize;` to
the final `return KErrNone;` of DMemModelChunk::Adjust. Everything else in
the function (validation, KBOOT/KMEMTRACE traces) is preserved verbatim.

Usage: fix_mchunk_adjust.py <preprocessed.i>   (rewrites in place)
Fails loudly unless exactly one site is replaced.
"""

import sys
import re

SIG = "TInt DMemModelChunk::Adjust(TInt aNewSize)"


def main(path):
    with open(path) as f:
        src = f.read()
    start = src.find(SIG)
    if start < 0:
        print("fix_mchunk_adjust: signature not found in %s" % path)
        return 1
    # Span of the function body: brace-count from the opening brace.
    brace = src.find("{", start)
    depth = 0
    end = -1
    for i in range(brace, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                end = i
                break
    if end < 0:
        print("fix_mchunk_adjust: body end not found in %s" % path)
        return 1
    body = src[start:end]
    cands = [m.start() for m in re.finditer(r"return KErrNone;", body)]
    if len(cands) != 1:
        print("fix_mchunk_adjust: expected 1 return, found %d in %s"
              % (len(cands), path))
        return 1
    pos = start + cands[0]
    fixed = src[:pos] + "iSize = aNewSize;\n" + src[pos:]
    with open(path, "w") as f:
        f.write(fixed)
    print("fix_mchunk_adjust: iSize tracking added in %s" % path)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
