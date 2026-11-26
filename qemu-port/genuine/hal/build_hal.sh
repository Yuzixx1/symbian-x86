#!/bin/sh
# build_hal.sh -- build the genuine HAL user library (HALU.DLL) + exe smoke twin.
# Run from qemu-port/. kernelhwsrv/ is read-only; outputs land in genuine/build/
# with the hal_ prefix. See genuine/HAL_DM_NOTES.md.
set -e
unset LD_LIBRARY_PATH
cd "$(dirname "$0")/../.."
KH=../kernelhwsrv

python3 genuine/hal/gen_def.py >/dev/null

# HALU.DLL: genuine hal_main.cpp + userhal.cpp + halcfg.pl-generated tables,
# export directory from bx86/halu.def (aliased, see gen_def.py).
python3 tools/genuine_build.py \
  $KH/halservices/hal/src/hal_main.cpp \
  $KH/halservices/hal/src/userhal.cpp \
  genuine/hal/gen/hal_values.cpp \
  genuine/hal/gen/hal_config.cpp \
  genuine/hal/hal_glue.cpp \
  --name hal_halu --type dll \
  -I$KH/halservices/hal/inc -Igenuine/hal/shim \
  --def genuine/hal/hal_halu.def \
  --export-map hal_ord1=_ZN3HAL3GetEN7HALData10TAttributeERi \
  --export-map hal_ord2=_ZN3HAL6GetAllERiRPNS_6SEntryE \
  --export-map hal_ord3=_ZN3HAL3SetEN7HALData10TAttributeEi \
  --export-map hal_ord4=_ZN3HAL3GetEiN7HALData10TAttributeERi \
  --export-map hal_ord5=_ZN3HAL3SetEiN7HALData10TAttributeEi \
  --export-map hal_ord6=_ZN11HalInternal12InitialValueE \
  --iat-sym hal_printf_slot --euser-import 915

# hal_smoke.E32: same genuine sources as an EXE (UID1=0x1000007a) so the
# kernel's E32Run can start it as a process in QEMU for the smoke run.
python3 tools/genuine_build.py \
  $KH/halservices/hal/src/hal_main.cpp \
  $KH/halservices/hal/src/userhal.cpp \
  genuine/hal/gen/hal_values.cpp \
  genuine/hal/gen/hal_config.cpp \
  genuine/hal/hal_glue.cpp \
  --name hal_smoke --type exe -DHAL_AS_EXE_SMOKE -DHAL_PROBE_SCREENINFO \
  -I$KH/halservices/hal/inc -Igenuine/hal/shim \
  --iat-sym hal_printf_slot --euser-import 915

python3 tools/e32read.py genuine/build/hal_halu.E32
python3 tools/e32read.py genuine/build/hal_smoke.E32
