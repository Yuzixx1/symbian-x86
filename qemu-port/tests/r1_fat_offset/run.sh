#!/bin/bash
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out=${1:?provide a private output directory}
mkdir -p "$out"
c++ -std=c++98 -Wall -Wextra -Werror -Wno-unused-function -Wno-unused-but-set-variable -I"$here/include" "$here/test.cpp" -o "$out/test"
"$out/test"
for driver in ata ahci; do
    c++ -std=c++98 -O2 -Wall -Wextra -Wno-unused-function -fpermissive -I"$here/include" "$here/${driver}_flush.cpp" -o "$out/${driver}_flush"
    "$out/${driver}_flush"
done
