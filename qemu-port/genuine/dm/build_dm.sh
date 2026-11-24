#!/bin/sh
# build_dm.sh -- build the genuine domain manager components as E32 images:
#   dm_domainsrv.E32    domainsrv.cpp + domaincfg.cpp  (server, runnable)
#   dm_domainpolicy.E32 domainpolicy.cpp               (DLL, 3 exports)
#   dm_domaincli.E32    domaincli.cpp                  (DLL, 51 exports)
# Run from qemu-port/. kernelhwsrv/ is read-only; outputs land in
# genuine/build/ with the dm_ prefix. See genuine/HAL_DM_NOTES.md.
set -e
cd "$(dirname "$0")/../.."
KH=../kernelhwsrv
DM=$KH/userlibandfileserver/domainmgr
FS=$KH/userlibandfileserver/fileserver

# 1. compile objects once (for stub/def generation + inspection)
python3 genuine/dm/compile_objs.py

# 2. export defs + maps from the genuine bx86 .def files
python3 genuine/dm/gen_dm_def.py $DM/bx86/domainpolicyu.def \
    genuine/dm/obj/domainpolicy.o genuine/dm/dm_domainpolicy.def
python3 genuine/dm/gen_dm_def.py $DM/bx86/domaincliu.def \
    genuine/dm/obj/domaincli.o genuine/dm/dm_domaincli.def

# 3. link-time stubs for EUSER/EFSRV symbols (single-import pipeline)
python3 genuine/dm/gen_stubs.py genuine/dm/obj/domainsrv.o genuine/dm/obj/domaincfg.o \
    genuine/dm/obj/dm_glue.o genuine/dm/dm_stubs.S dm_printf_slot
python3 genuine/dm/gen_stubs.py genuine/dm/obj/domaincli.o genuine/dm/obj/dm_dll_glue.o \
    genuine/dm/dm_cli_stubs.S dm_printf_slot
python3 genuine/dm/gen_stubs.py genuine/dm/obj/domainpolicy.o genuine/dm/obj/dm_dll_glue.o \
    genuine/dm/dm_policy_stubs.S dm_printf_slot

INC="-I$DM/inc -I$DM/src -I$FS/inc"

# 4. the domain manager server (E32Run-loadable, smoke-tested in QEMU)
python3 tools/genuine_build.py \
  $DM/src/domainsrv.cpp $DM/src/domaincfg.cpp \
  genuine/dm/dm_glue.cpp genuine/dm/dm_stubs.S \
  --name dm_domainsrv --type server $INC \
  --iat-sym dm_printf_slot --euser-import 915

# 5. domainpolicy.dll (DLL + 3 exports from domainpolicyu.def)
python3 tools/genuine_build.py \
  $DM/src/domainpolicy.cpp genuine/dm/dm_dll_glue.cpp genuine/dm/dm_policy_stubs.S \
  --name dm_domainpolicy --type dll $INC -DDM_DLL_LABEL=DMPOLICY -DDM_DLL_IS_POLICY \
  --def genuine/dm/dm_domainpolicy.def $(cat genuine/dm/dm_domainpolicy.def.maps | tr '\n' ' ') \
  --iat-sym dm_printf_slot --euser-import 915

# 6. domaincli.dll (DLL + 51 exports from domaincliu.def)
python3 tools/genuine_build.py \
  $DM/src/domaincli.cpp genuine/dm/dm_dll_glue.cpp genuine/dm/dm_cli_stubs.S \
  --name dm_domaincli --type dll $INC -DDM_DLL_LABEL=DMCLI \
  --def genuine/dm/dm_domaincli.def $(cat genuine/dm/dm_domaincli.def.maps | tr '\n' ' ') \
  --iat-sym dm_printf_slot --euser-import 915

for f in dm_domainsrv dm_domainpolicy dm_domaincli; do
  python3 tools/e32read.py genuine/build/$f.E32
done
