#!/usr/bin/env python3
"""fix_cia_immediates.py — build-glue rewrite for cexec.cia (x86gcc quirk).

Genuine kernel/x86/cexec.cia calls through file-scope address constants::

    const TLinAddr K_PanicCurrentThread = (TLinAddr)&K::PanicCurrentThread;
    ...
    asm("call %a0" : : "i"(K_PanicCurrentThread));

At -O1+ our GCC rejects the indirect form ("impossible constraint in 'asm'")
while the direct form assembles to a correct PC32 call (verified via relocs).
-O0 is NOT an alternative: it forces frame-pointer prologues that corrupt
the hand-written frame math in every .cia (observed as a stack wipe through
TPriListBase -> memclr). So: preprocess first, then rewrite the two indirect
sites to direct function addresses, then compile the result.

Usage: fix_cia_immediates.py <preprocessed.i>   (rewrites in place)
Fails loudly unless exactly the known sites are replaced.
"""

import sys

OLD_NOSPACE = ':"i"(K_PanicCurrentThread)'
OLD_SPACE = ':"i" (K_PanicCurrentThread)'
NEW = ':"i"(&K::PanicCurrentThread)'
EXPECTED = 2


def main(path):
    with open(path) as f:
        src = f.read()
    n = src.count(OLD_NOSPACE) + src.count(OLD_SPACE)
    if n != EXPECTED:
        print("fix_cia_immediates: expected %d sites, found %d in %s"
              % (EXPECTED, n, path))
        return 1
    src = src.replace(OLD_NOSPACE, NEW).replace(OLD_SPACE, NEW)
    with open(path, "w") as f:
        f.write(src)
    print("fix_cia_immediates: rewrote %d sites in %s" % (n, path))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
