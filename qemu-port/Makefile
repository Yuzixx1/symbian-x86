# Makefile — minimal standalone build for the genuine EKA2 x86 slice on QEMU.
# Bypasses bldmake/ActivePerl/rombuild entirely. Phase 2 builds and boots ONLY
# the Multiboot bridge; genuine kernel sources are listed, verified, and given
# pattern rules, but full kernel linkage waits for Phase 3 ports.
#
# NOTE: all paths are RELATIVE (no $(CURDIR)) because the workspace root
# contains a space ("Sybian REAL") which GNU Make cannot handle in targets.
# Always invoke make from this directory (qemu-port/).
#
# Usage:
#   make check-env     # verify Linux toolchain (gcc/as/ld/qemu)
#   make               # build/build/multiboot.elf
#   make check-slice   # verify every genuine Phase-3 input file exists
#   make qemu          # boot the bridge in QEMU, serial banner on stdio (5s)
#   make headers       # show multiboot header bytes + section map
#   make clean

QPORT := .
EKA   := ../kernelhwsrv/kernel/eka
BUILD := build

include toolchain.mk

# Default goal: kernels + both hybrid ISOs (test artifacts never go stale).
.DEFAULT_GOAL := all
include user/user.mk

# Phase 8 Step 2: libgcc for __udivdi3 (nklib.o SRatio::Reciprocal, freestanding).
LIBGCC := $(shell $(CC) -m32 -print-libgcc-file-name)
LDFLAGS += $(LIBGCC)

# ---------------------------------------------------------------- Phase 2+3 image
BRIDGE_S := boot/multiboot.S
LINK_T   := boot/link.ld
BRIDGE_ELF := build/multiboot.elf
PORTED_S := boot/ported/bootcpu_gas.S boot/ported/bootmain_gas.S
VARIANT_CXX := variant/qemu_assp.cpp variant/qemu_kernel.cpp
PORTED_O := build/bootcpu_gas.o build/bootmain_gas.o
VARIANT_O := build/qemu_assp.o build/qemu_kernel.o build/irq.o build/kernel_main.o
# Phase 8: genuine object lists live HERE (before first use): make expands
# explicit-rule prerequisites immediately at parse time, so these must
# precede the $(BRIDGE_ELF) rule below. Per-file build rules stay in the
# GEN section further down (recipes expand lazily and are unaffected).
GEN_BUILD := build/genuine
GEN_OBJS := $(GEN_BUILD)/nkern.o $(GEN_BUILD)/sched.o $(GEN_BUILD)/nk_timer.o $(GEN_BUILD)/dfcs.o $(GEN_BUILD)/nklib.o $(GEN_BUILD)/ncglob.o $(GEN_BUILD)/atomics.o $(GEN_BUILD)/ncsched.o $(GEN_BUILD)/ncthrd.o $(GEN_BUILD)/ncutils.o $(GEN_BUILD)/vectors.o $(GEN_BUILD)/nk_entry.o $(GEN_BUILD)/nkerns.o $(GEN_BUILD)/ncsched_cpp.o $(GEN_BUILD)/ncthrd_cpp.o $(GEN_BUILD)/ncutils_cpp.o $(GEN_BUILD)/vectors_cpp.o $(GEN_BUILD)/cmem.o
# Bootloader + genuine nanokernel + variant glue = the primary image.
# libgcc trails the objects: archives satisfy only previously-seen undefined refs.
# Phase 9: retired IRQ0 stub (variant/irq_gas.S rule below) re-linked: pure asm,
# C handler now genuine tick.
GEN_LINK_OBJS := build/multiboot.o build/bootcpu_gas.o build/bootmain_gas.o build/mmu_gas.o build/qemu_assp.o build/irq_gas.o build/genuine_glue.o build/genuine_console.o $(GEN_OBJS)

# ---------------------------------------------------------------- Phase 11: genuine kernel objects (Kern personality)
# Same recipe as GEN but WITHOUT __STANDALONE_NANOKERNEL__ (kernel files need
# the assp.h/plat_priv.h branch of x86.h) and WITH mmp macro __IN_KERNEL__.
# plat_priv.h is our variant export (inc/); assp.h stays genuine.
KERN_FLAGS := -m32 -march=i486 -std=gnu++98 -ffreestanding -nostdlib -fno-builtin -fno-pic -fno-pie -fno-stack-protector -mno-red-zone -fno-exceptions -fno-rtti -fno-threadsafe-statics -O2 -masm=intel
# (Genuine X86 kernel asms are Intel-syntax throughout: cpudefs.h __crash
# "int 0xff", x86.h MOV_CR4_*/ASM_DEBUG*, .cia bodies. Matches KERN_CIA_DEFS.)
KERN_DEFS := -D__GCC32__ -D__X86__ -D__EPOC32__ -D_UNICODE -D_DEBUG -D__IN_KERNEL__ -D__KERNEL_MODE__ -DIMPORT_C= -DEXPORT_C= -DIMPORT_D= -DEXPORT_D= -D'__NO_THROW=throw()' -D'TEMPLATE_SPECIALIZATION=template<>' -D'__cdecl=__attribute__((cdecl))' -D'__fastcall=__attribute__((fastcall))' -D'__stdcall=__attribute__((stdcall))' -D'NONSHARABLE_CLASS(x)=class x' -D'NONSHARABLE_STRUCT(x)=struct x' -D'_FOFF(c,f)=((TInt)__builtin_offsetof(c,f))'
KERN_CIA_DEFS := $(KERN_DEFS) -D__CIA__ -masm=intel
# Kernel .cia are hand-written asm dispatchers whose frame math assumes the
# compiler emits NO prologue (bare entry: [esp]=ret). That holds at -O2
# (frame-pointer omission, as proven by the Phase-9 GEN slice) but NOT at -O0
# (observed stack wipe via TPriListBase -> memclr). Never use -O0 here.
KERN_CIA_FLAGS := -m32 -march=i486 -std=gnu++98 -ffreestanding -nostdlib -fno-builtin -fno-pic -fno-pie -fno-stack-protector -mno-red-zone -fno-exceptions -fno-rtti -fno-threadsafe-statics -O2
KERN_INCLUDES := -I$(EKA)/include -I$(EKA)/include/nkern -I$(EKA)/include/kernel -I$(EKA)/include/kernel/x86 -I$(EKA)/include/nkern/x86 -I$(EKA)/common -I$(EKA)/include/memmodel/epoc -I$(EKA)/include/memmodel/epoc/direct -I$(EKA)/include/memmodel/epoc/direct/x86 -I$(QPORT)/inc -I$(QPORT)/build/export
KERN_BUILD := build/kern
KERN_OBJS_1 := $(KERN_BUILD)/ncutilf.o $(KERN_BUILD)/ncutilf_cpp.o $(KERN_BUILD)/nctimer.o
KERN_OBJS_2 := $(KERN_BUILD)/cglobals.o $(KERN_BUILD)/cexec.o $(KERN_BUILD)/cinit.o $(KERN_BUILD)/ckernel.o $(KERN_BUILD)/cipc.o $(KERN_BUILD)/csched.o $(KERN_BUILD)/cutils.o $(KERN_BUILD)/abort.o $(KERN_BUILD)/cache.o $(KERN_BUILD)/cexec_cia.o $(KERN_BUILD)/ckernel_cia.o $(KERN_BUILD)/csched_cia.o $(KERN_BUILD)/cutils_cia.o $(KERN_BUILD)/cumem.o $(KERN_BUILD)/cumem_cia.o
# Phase 11 Step 2: Direct memory model (mmp MM_DIRECT+X86 order) + EPOC common.
KERN_MEMMODEL := mchunk mglobals minit mkernel mcodeseg mprocess mutils mdefrag xglobals xinit xipc xkernel xsched xutils putils pcodeseg pprocess pglobals nvram
KERN_OBJS_3 := $(addprefix $(KERN_BUILD)/, $(addsuffix .o, $(KERN_MEMMODEL)))
# Phase 11 Step 2: kernel core (ekern.mmp order) + klib + common.
# (__EXPORT_MEMCPY__ from ekern.mmp is def-tool-only; no source references it.)
KERN_CORE := sglobals device eventq sexec sinit skernel scodeseg sipc notifier object power server sha256 securerng stimer sutils random sprocess sthread smqueue sproperty ssharedio ssecure emi objectix sbtrace stest sshbuf smap srandombuff kerncorestats byte_pair
KERN_LIB := dbase kdes8 kheap bma alloc array des8 mem heap_hybrid debugfunction secure
KERN_OBJS_4 := $(addprefix $(KERN_BUILD)/, $(addsuffix .o, $(KERN_CORE) $(KERN_LIB)))

genuine-kern-scratch: build/export/exec_enum.h $(KERN_OBJS_1) $(KERN_OBJS_2) $(KERN_OBJS_3) $(KERN_OBJS_4)

# Generated exec tables (genuine genexec.pl + execs.txt): build artifacts that
# `make clean` removes, so they need a real rule (KERN objects include them).
build/export/exec_enum.h: $(EKA)/kernel/execs.txt $(EKA)/kernel/genexec.pl
	mkdir -p build/export/kernel
	perl $(EKA)/kernel/genexec.pl -i $(EKA)/kernel/execs.txt -e build/export/exec_enum.h -u build/export/exec_user.h -k build/export/kernel/exec_kernel.h

build/export/exec_user.h build/export/kernel/exec_kernel.h: build/export/exec_enum.h
	@test -f $@

$(KERN_BUILD):
	mkdir -p $(KERN_BUILD)

$(KERN_BUILD)/ncutilf.o: $(EKA)/nkern/x86/ncutilf.cia | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_CIA_DEFS) $(KERN_INCLUDES) -x c++ -c $(EKA)/nkern/x86/ncutilf.cia -o $@

$(KERN_BUILD)/ncutilf_cpp.o: $(EKA)/nkern/x86/ncutilf.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $(EKA)/nkern/x86/ncutilf.cpp -o $@

$(KERN_BUILD)/nctimer.o: $(EKA)/nkern/x86/nctimer.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $(EKA)/nkern/x86/nctimer.cpp -o $@

# Phase 11 batch 2: kernel/x86 companions (mmp order) + klib/x86 utils.
$(KERN_BUILD)/cglobals.o: $(EKA)/kernel/x86/cglobals.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $(EKA)/kernel/x86/cglobals.cpp -o $@

$(KERN_BUILD)/cexec.o: $(EKA)/kernel/x86/cexec.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $(EKA)/kernel/x86/cexec.cpp -o $@

$(KERN_BUILD)/cinit.o: $(EKA)/kernel/x86/cinit.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $(EKA)/kernel/x86/cinit.cpp -o $@

$(KERN_BUILD)/ckernel.o: $(EKA)/kernel/x86/ckernel.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $(EKA)/kernel/x86/ckernel.cpp -o $@

$(KERN_BUILD)/cipc.o: $(EKA)/kernel/x86/cipc.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $(EKA)/kernel/x86/cipc.cpp -o $@

$(KERN_BUILD)/csched.o: $(EKA)/kernel/x86/csched.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $(EKA)/kernel/x86/csched.cpp -o $@

$(KERN_BUILD)/cutils.o: $(EKA)/kernel/x86/cutils.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $(EKA)/kernel/x86/cutils.cpp -o $@

$(KERN_BUILD)/abort.o: $(EKA)/kernel/x86/abort.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $(EKA)/kernel/x86/abort.cpp -o $@

$(KERN_BUILD)/cache.o: $(EKA)/kernel/x86/cache.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $(EKA)/kernel/x86/cache.cpp -o $@

$(KERN_BUILD)/cexec_cia.o: $(EKA)/kernel/x86/cexec.cia tools/fix_cia_immediates.py | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_CIA_DEFS) $(KERN_INCLUDES) -E -x c++ -c $(EKA)/kernel/x86/cexec.cia -o $(KERN_BUILD)/cexec.i
	python3 tools/fix_cia_immediates.py $(KERN_BUILD)/cexec.i
	$(CXX) $(KERN_CIA_FLAGS) $(KERN_CIA_DEFS) $(KERN_INCLUDES) -x c++ -c $(KERN_BUILD)/cexec.i -o $@

$(KERN_BUILD)/ckernel_cia.o: $(EKA)/kernel/x86/ckernel.cia | $(KERN_BUILD)
	$(CXX) $(KERN_CIA_FLAGS) $(KERN_CIA_DEFS) $(KERN_INCLUDES) -x c++ -c $(EKA)/kernel/x86/ckernel.cia -o $@

$(KERN_BUILD)/csched_cia.o: $(EKA)/kernel/x86/csched.cia | $(KERN_BUILD)
	$(CXX) $(KERN_CIA_FLAGS) $(KERN_CIA_DEFS) $(KERN_INCLUDES) -x c++ -c $(EKA)/kernel/x86/csched.cia -o $@

$(KERN_BUILD)/cutils_cia.o: $(EKA)/kernel/x86/cutils.cia | $(KERN_BUILD)
	$(CXX) $(KERN_CIA_FLAGS) $(KERN_CIA_DEFS) $(KERN_INCLUDES) -x c++ -c $(EKA)/kernel/x86/cutils.cia -o $@

$(KERN_BUILD)/cumem.o: $(EKA)/klib/x86/cumem.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $(EKA)/klib/x86/cumem.cpp -o $@

$(KERN_BUILD)/cumem_cia.o: $(EKA)/klib/x86/cumem.cia | $(KERN_BUILD)
	$(CXX) $(KERN_CIA_FLAGS) $(KERN_CIA_DEFS) $(KERN_INCLUDES) -x c++ -c $(EKA)/klib/x86/cumem.cia -o $@

# Phase 11 Step 2 pattern rules: basenames are unique across these dirs, so
# make picks the one whose prerequisite exists. Compile-only (not linked yet).
$(KERN_BUILD)/%.o: $(EKA)/memmodel/epoc/direct/%.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

# Direct-model Adjust size-tracking repair (tools/fix_mchunk_adjust.py):
# explicit rule shadows the pattern above for mchunk only.
$(KERN_BUILD)/mchunk.o: $(EKA)/memmodel/epoc/direct/mchunk.cpp tools/fix_mchunk_adjust.py | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -E -x c++ -c $(EKA)/memmodel/epoc/direct/mchunk.cpp -o $(KERN_BUILD)/mchunk.i
	python3 tools/fix_mchunk_adjust.py $(KERN_BUILD)/mchunk.i
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -x c++ -c $(KERN_BUILD)/mchunk.i -o $@

$(KERN_BUILD)/%.o: $(EKA)/memmodel/epoc/direct/x86/%.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

$(KERN_BUILD)/%.o: $(EKA)/memmodel/epoc/%.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

$(KERN_BUILD)/%.o: $(EKA)/kernel/%.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

$(KERN_BUILD)/%.o: $(EKA)/klib/%.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

$(KERN_BUILD)/%.o: $(EKA)/common/%.cpp | $(KERN_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

# ---------------------------------------------------------------- Phase 11 Step 3: kern-flagged nanokernel (full-kernel image)
# Same sources as the GEN slice (plus ncmonitor.cia, which the Kern needs),
# compiled WITHOUT __STANDALONE_NANOKERNEL__ into build/knk. The working
# nanokernel-only image keeps using build/genuine; nothing here touches it.
KNK_BUILD := build/knk
KNK_CPPS := nkern sched nk_timer dfcs nklib ncglob nk_entry nkerns
KNK_CPPX := ncsched_cpp ncthrd_cpp ncutils_cpp vectors_cpp
KNK_CIAS := atomics ncsched ncthrd ncutils vectors cmem
KNK_OBJS := $(addprefix $(KNK_BUILD)/, $(addsuffix .o, $(KNK_CPPS) $(KNK_CPPX) $(KNK_CIAS)))

$(KNK_BUILD):
	mkdir -p $(KNK_BUILD)

$(KNK_BUILD)/%.o: $(EKA)/nkern/%.cpp | $(KNK_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

# NB: ncsched/ncthrd/ncutils/vectors exist as BOTH .cia and .cpp; the plain
# .o always comes from the .cia (like GEN), the _cpp.o from the .cpp.
$(KNK_BUILD)/ncglob.o: $(EKA)/nkern/x86/ncglob.cpp | $(KNK_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

$(KNK_BUILD)/nk_entry.o: $(EKA)/nkern/x86/nk_entry.cpp | $(KNK_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

$(KNK_BUILD)/ncsched_cpp.o: $(EKA)/nkern/x86/ncsched.cpp | $(KNK_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

$(KNK_BUILD)/ncthrd_cpp.o: $(EKA)/nkern/x86/ncthrd.cpp | $(KNK_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

$(KNK_BUILD)/ncutils_cpp.o: $(EKA)/nkern/x86/ncutils.cpp | $(KNK_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

$(KNK_BUILD)/vectors_cpp.o: $(EKA)/nkern/x86/vectors.cpp | $(KNK_BUILD)
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

$(KNK_BUILD)/%.o: $(EKA)/nkern/x86/%.cia | $(KNK_BUILD)
	$(CXX) $(KERN_CIA_FLAGS) $(KERN_CIA_DEFS) $(KERN_INCLUDES) -x c++ -c $< -o $@

$(KNK_BUILD)/%.o: $(EKA)/common/x86/%.cia | $(KNK_BUILD)
	$(CXX) $(KERN_CIA_FLAGS) $(KERN_CIA_DEFS) $(KERN_INCLUDES) -x c++ -c $< -o $@

# Full-kernel variant objects (our code; KERN flags so mangling matches).
build/kernfull_glue.o: variant/kernfull_glue.cpp variant/qemu_variant.h | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/kernfull_glue.cpp -o $@

build/qemu_pci.o: variant/qemu_pci.cpp | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/qemu_pci.cpp -o $@

build/qemu_ata.o: variant/qemu_ata.cpp variant/qemu_variant.h | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/qemu_ata.cpp -o $@

build/qemu_ahci.o: variant/qemu_ahci.cpp variant/qemu_variant.h | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/qemu_ahci.cpp -o $@

build/qemu_xhci.o: variant/qemu_xhci.cpp variant/qemu_variant.h | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/qemu_xhci.cpp -o $@

build/fat32.o: variant/fat32.cpp variant/qemu_variant.h | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/fat32.cpp -o $@

build/qemu_usb.o: variant/qemu_usb.cpp variant/qemu_variant.h | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/qemu_usb.cpp -o $@

build/qemu_usb_ohci.o: variant/qemu_usb_ohci.cpp variant/qemu_variant.h | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/qemu_usb_ohci.cpp -o $@

build/qemu_variant_rom.o: variant/qemu_variant_rom.cpp variant/qemu_variant.h | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/qemu_variant_rom.cpp -o $@

build/qemu_variant_asic.o: variant/qemu_variant_asic.cpp variant/qemu_variant.h | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/qemu_variant_asic.cpp -o $@

build/qemu_variant_int.o: variant/qemu_variant_int.cpp variant/qemu_variant.h | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/qemu_variant_int.cpp -o $@

build/kern_shell.o: variant/kern_shell.cpp | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/kern_shell.cpp -o $@

build/e32load.o: variant/e32load.cpp variant/qemu_variant.h | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/e32load.cpp -o $@

build/userfiles.o: variant/userfiles.cpp variant/qemu_variant.h | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

build/usercon.o: variant/usercon.cpp variant/qemu_variant.h | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c $< -o $@

# Full Kern image: genuine nanokernel (knk) + genuine kernel (kern) + variant.
# Scratch link first; promoted to boot target once it survives ResetEntry.
KERNFULL_OBJS := build/multiboot.o build/bootcpu_gas.o build/bootmain_gas.o build/qemu_assp.o build/kernfull_glue.o build/qemu_variant_rom.o build/qemu_variant_asic.o build/qemu_variant_int.o build/qemu_pci.o build/qemu_ata.o build/qemu_ahci.o build/qemu_xhci.o build/fat32.o build/e32load.o build/qemu_usb.o build/qemu_usb_ohci.o build/kern_shell.o $(KNK_OBJS) $(KERN_OBJS_1) $(KERN_OBJS_2) $(KERN_OBJS_3) $(KERN_OBJS_4)

KERNFULL_OBJS += build/userfiles.o build/usercon.o

build/kernfull.elf: build/export/exec_enum.h $(KERNFULL_OBJS) boot/link.ld
	$(LD) -m elf_i386 -nostdlib -static -T boot/link.ld -o $@ $(KERNFULL_OBJS) $(LIBGCC)
	@echo "BUILT: $@"

kernfull: build/kernfull.elf
	timeout 12 qemu-system-i386 -kernel build/kernfull.elf -m 128 -display none -serial stdio -no-reboot 2>&1 | head -n 40; \
	echo "(exit $$? - 124 means still running past timeout)"
MMU_O := build/mmu_gas.o build/mmu.o
ROM_O := build/rom_image.o

# ---------------------------------------------------------------- Phase 3 slice
# Genuine EKA2 inputs for the Direct-model bare-metal kernel. NOT compiled yet
# (they need the Phase-3 QEMU ASSP + ported bootstrap + plat_priv.h shim),
# but the build already knows them: `make check-slice` proves the slice is
# complete and pinned to exact repo paths.
SLICE_BOOT_MASM := \
  $(EKA)/kernel/x86/bootmain.asm \
  $(EKA)/kernel/x86/bootcpu.asm \
  $(EKA)/kernel/x86/bootutil.asm
SLICE_NKERN_CIA := \
  $(EKA)/nkern/x86/ncsched.cia \
  $(EKA)/nkern/x86/ncthrd.cia \
  $(EKA)/nkern/x86/ncutils.cia \
  $(EKA)/nkern/x86/ncutilf.cia \
  $(EKA)/nkern/x86/ncmonitor.cia \
  $(EKA)/nkern/x86/vectors.cia \
  $(EKA)/nkern/x86/nk_entry.cia
SLICE_NKERN_CPP := \
  $(EKA)/nkern/nkern.cpp \
  $(EKA)/nkern/sched.cpp \
  $(EKA)/nkern/nk_timer.cpp \
  $(EKA)/nkern/nklib.cpp \
  $(EKA)/nkern/dfcs.cpp \
  $(EKA)/nkern/x86/ncsched.cpp \
  $(EKA)/nkern/x86/ncthrd.cpp \
  $(EKA)/nkern/x86/ncutils.cpp \
  $(EKA)/nkern/x86/ncutilf.cpp \
  $(EKA)/nkern/x86/vectors.cpp \
  $(EKA)/nkern/x86/nk_entry.cpp \
  $(EKA)/nkern/x86/ncglob.cpp \
  $(EKA)/nkern/x86/nctimer.cpp
SLICE_KERNEL_X86 := \
  $(EKA)/kernel/x86/cinit.cpp \
  $(EKA)/kernel/x86/k_entry.cpp \
  $(EKA)/kernel/x86/ckernel.cpp \
  $(EKA)/kernel/x86/cutils.cpp \
  $(EKA)/kernel/x86/cexec.cpp
SLICE_MEMMODEL_DIRECT := \
  $(EKA)/memmodel/epoc/direct/mchunk.cpp \
  $(EKA)/memmodel/epoc/direct/mglobals.cpp \
  $(EKA)/memmodel/epoc/direct/minit.cpp \
  $(EKA)/memmodel/epoc/direct/mkernel.cpp \
  $(EKA)/memmodel/epoc/direct/mutils.cpp \
  $(EKA)/memmodel/epoc/direct/x86/xinit.cpp \
  $(EKA)/memmodel/epoc/direct/x86/xkernel.cpp \
  $(EKA)/memmodel/epoc/direct/x86/xglobals.cpp \
  $(EKA)/memmodel/epoc/direct/x86/xutils.cpp
SLICE_COMMON_X86 := \
  $(EKA)/common/x86/atomics.cia \
  $(EKA)/common/x86/cmem.cia

.PHONY: all check-env check-slice qemu qemu-capture qemu-cdrom qemu-cdfull qemu-hd iso isofull isogrub headers clean print-slice demo-cia-rule genuine-nk-scratch genuine-link-test genuine-kern-scratch kernfull

# Default build: both kernels plus both (hybrid CD/USB) ISOs, so the test
# artifacts are never stale.
all: $(BRIDGE_ELF) iso isofull isogrub userland

check-env:
	sh tools/check_env.sh

build:
	mkdir -p build

build/multiboot.o: boot/multiboot.S | build
	$(AS) $(ASFLAGS) -o $@ boot/multiboot.S

build/bootcpu_gas.o: boot/ported/bootcpu_gas.S | build
	$(AS) $(ASFLAGS) -o $@ boot/ported/bootcpu_gas.S

build/bootmain_gas.o: boot/ported/bootmain_gas.S | build
	$(AS) $(ASFLAGS) -o $@ boot/ported/bootmain_gas.S

build/qemu_assp.o: variant/qemu_assp.cpp variant/qemu_assp.h | build
	$(CXX) $(CXXFLAGS) -Ivariant -c variant/qemu_assp.cpp -o $@

build/qemu_kernel.o: variant/qemu_kernel.cpp variant/qemu_assp.h variant/bootinfo.h rom/rom_image.h | build
	$(CXX) $(CXXFLAGS) -Ivariant -c variant/qemu_kernel.cpp -o $@

build/bootsect.o: boot/bootsect.S | build
	$(AS) $(ASFLAGS) -o $@ boot/bootsect.S

build/mbr.o: boot/mbr.S | build
	$(AS) $(ASFLAGS) -o $@ boot/mbr.S

build/mbr.bin: build/mbr.o boot/mbr.ld
	$(LD) -m elf_i386 -nostdlib -static -T boot/mbr.ld -o $@ build/mbr.o
	@size=$$(stat -c%s $@); echo "mbr.bin: $$size bytes (code must fit 446)"; \
	test $$size -le 446 || (echo "MBR CODE TOO BIG"; exit 1)
	@size=$$(stat -c%s $@); pad=$$((512 - size)); \
	python3 -c "open('$@','ab').write(bytes($$pad))" && echo "padded to 512"

# UEFI loader (freestanding X64 PE32+): MS ABI, short wchar, stripped of
# sections firmware rejects (.comment at RVA 0, .note, .eh_frame).
EFI_FLAGS := -m64 -mabi=ms -fshort-wchar -ffreestanding -nostdlib -fno-builtin -fno-pic -fno-pie -fno-common -fno-stack-protector -mno-red-zone -fno-unwind-tables -fno-asynchronous-unwind-tables -O2
EFI_INCLUDES := -Iboot/efi

build/efi:
	mkdir -p build/efi

build/efi/loader.o: boot/efi/loader.c boot/efi/efi.h | build/efi
	$(CC) $(EFI_FLAGS) $(EFI_INCLUDES) -c boot/efi/loader.c -o $@

build/efi/trans64.o: boot/efi/trans64.S | build/efi
	$(AS) --64 -o $@ boot/efi/trans64.S

build/efi/BOOTX64.EFI: build/efi/loader.o build/efi/trans64.o
	$(LD) -mi386pep -o $@ --entry efi_main --subsystem 10 --image-base 0 $^
	objcopy -R .comment -R .note.gnu.property -R .note -R .eh_frame $@
	@echo "BUILT: $@"

build/bootsect.bin: build/bootsect.o boot/bootsect.ld
	$(LD) -m elf_i386 -nostdlib -static -T boot/bootsect.ld -o $@ build/bootsect.o
	@size=$$(stat -c%s $@); echo "bootsect.bin: $$size bytes (must be <= 4096)"; \
	test $$size -le 4096 || (echo "BOOTSECT TOO BIG"; exit 1)
	@size=$$(stat -c%s $@); pad=$$((4096 - size)); \
	python3 -c "open('$@','ab').write(bytes($$pad))" && echo "padded to 4096"

build/irq.o: variant/irq.cpp variant/qemu_assp.h | build
	$(CXX) $(CXXFLAGS) -Ivariant -c variant/irq.cpp -o $@

build/irq_gas.o: variant/irq_gas.S | build
	$(AS) $(ASFLAGS) -o $@ variant/irq_gas.S

build/kernel_main.o: variant/kernel_main.cpp variant/qemu_assp.h rom/rom_image.h | build
	$(CXX) $(CXXFLAGS) -Ivariant -c variant/kernel_main.cpp -o $@

build/mmu_gas.o: mmu/mmu_gas.S | build
	$(AS) $(ASFLAGS) -o $@ mmu/mmu_gas.S

build/mmu.o: mmu/mmu.cpp variant/qemu_assp.h | build
	$(CXX) $(CXXFLAGS) -Ivariant -c mmu/mmu.cpp -o $@

build/rom_image.o: rom/rom_image.cpp rom/rom_image.h variant/qemu_assp.h | build
	$(CXX) $(CXXFLAGS) -Ivariant -c rom/rom_image.cpp -o $@

build/nk_ctx_gas.o: nk/nk_ctx_gas.S | build
	$(AS) $(ASFLAGS) -o $@ nk/nk_ctx_gas.S

build/nk_sched.o: nk/nk_sched.cpp nk/nk_sched.h variant/qemu_assp.h console/shell.h | build
	$(CXX) $(CXXFLAGS) -Ivariant -Ink -c nk/nk_sched.cpp -o $@

build/vga.o: console/vga.cpp console/vga.h variant/qemu_assp.h | build
	$(CXX) $(CXXFLAGS) -Ivariant -Iconsole -c console/vga.cpp -o $@

build/kbd.o: console/kbd.cpp console/kbd.h variant/qemu_assp.h | build
	$(CXX) $(CXXFLAGS) -Ivariant -Iconsole -c console/kbd.cpp -o $@

build/shell.o: console/shell.cpp console/shell.h console/vga.h console/kbd.h nk/nk_sched.h rom/rom_image.h variant/bootinfo.h | build
	$(CXX) $(CXXFLAGS) -Ivariant -Iconsole -Ink -c console/shell.cpp -o $@

build/bootinfo.o: variant/bootinfo.cpp variant/bootinfo.h | build
	$(CXX) $(CXXFLAGS) -Ivariant -c variant/bootinfo.cpp -o $@

# Phase 8: the primary build IS the genuine nanokernel. Legacy shim objects
# (nk/nk_sched.o, QemuKernelMain, shell, VGA, ROM synth) are retired from the
# link; their sources remain on disk, unreferenced. Shim retirement complete.
$(BRIDGE_ELF): $(GEN_LINK_OBJS) boot/link.ld
	$(LD) -m elf_i386 -nostdlib -static -T boot/link.ld -o $@ $(GEN_LINK_OBJS) $(LIBGCC)
	@echo "BUILT: $@"
	file $@

# Genuine .cia files ARE C++ (with __GCC32__ GAS branches). Phase-3 pattern rule:
#   build/%.o: $(EKA)/%.cia -> $(CXX) $(CXXFLAGS) -x c++ -c $< -o $@
# Kept as an explicit named rule so Phase 2 never accidentally compiles the world.
demo-cia-rule:
	@echo 'Pattern (Phase 3): $$(CXX) $$(CXXFLAGS) $$(KERNEL_INCLUDES) -x c++ -c <file.cia> -o <file.o>'

check-slice:
	@echo "Checking genuine EKA2 Direct-model slice..."
	missing=0; \
	for f in $(SLICE_BOOT_MASM) $(SLICE_NKERN_CIA) $(SLICE_NKERN_CPP) \
	         $(SLICE_KERNEL_X86) $(SLICE_MEMMODEL_DIRECT) $(SLICE_COMMON_X86); do \
	  if [ ! -f "$$f" ]; then echo "MISSING: $$f"; missing=1; fi; \
	done; \
	if [ $$missing -eq 0 ]; then \
	  echo "SLICE-OK: all genuine files present."; \
	else exit 1; fi

print-slice:
	@echo "BOOT_MASM: $(SLICE_BOOT_MASM)"
	@echo "NKERN_CIA: $(SLICE_NKERN_CIA)"
	@echo "MEMMODEL_DIRECT: $(SLICE_MEMMODEL_DIRECT)"

headers: $(BRIDGE_ELF)
	@echo "--- ELF sections (multiboot must be first, file offset < 8KiB) ---"
	objdump -h $(BRIDGE_ELF)
	@echo "--- multiboot header words (expect 1BADB002) ---"
	objdump -s -j .multiboot $(BRIDGE_ELF)

# Smoke-boot: expect ResetEntry -> InitCpu -> _E32Startup_Body -> HwInit0 ->
# genuine KPrintf banner -> KernelMain placeholder (then halt, by design).
qemu: $(BRIDGE_ELF)
	timeout 5 qemu-system-i386 -kernel $(BRIDGE_ELF) -m 128 -display none -serial stdio \
	  -no-reboot 2>&1 | head -n 30; \
	echo "(exit $$? - 124 means the halt loop ran past timeout, which is expected)"

# Standalone artifact: GRUB-free El Torito ISO (bootsect + pycdlib).
# Streaming loader: no staging limit (chunks stream to PT_LOAD addresses).
# Kernel entry must stay 0x100030 (bootsect hardcodes it; see Phase 7 notes).
iso: $(BRIDGE_ELF) build/bootsect.bin build/mbr.bin build/efi/BOOTX64.EFI
	@size=$$(stat -c%s build/multiboot.elf); \
	if [ $$size -gt 4194304 ]; then echo "KERNEL TOO BIG ($$size > 4M)"; exit 1; fi
	@entry=$$(nm build/multiboot.elf | awk '$$3=="_mb_entry"{print $$1}'); \
	if [ "$$entry" != "00100030" ]; then echo "ENTRY MOVED ($$entry != 00100030): update bootsect + link.ld"; exit 1; fi
	python3 tools/mkiso.py

# Full-Kern ISO: same El Torito path with kernfull.elf as payload.
isofull: build/kernfull.elf build/bootsect.bin build/mbr.bin build/efi/BOOTX64.EFI
	@size=$$(stat -c%s build/kernfull.elf); \
	if [ $$size -gt 4194304 ]; then echo "KERNFULL TOO BIG ($$size > 4M)"; exit 1; fi
	@entry=$$(nm build/kernfull.elf | awk '$$3=="_mb_entry"{print $$1}'); \
	if [ "$$entry" != "00100030" ]; then echo "ENTRY MOVED ($$entry != 00100030)"; exit 1; fi
	python3 tools/mkiso.py build/kernfull.elf build/symbian-pc-full.iso

# GRUB combo ISO (BIOS El Torito + UEFI El Torito in one image): GRUB's own
# disk drivers replace our bootsect EDD path (which hangs VirtualBox's BIOS
# on the 2nd read). Needs grub2-mkimage (GRUB 2.12 i386-pc + x86_64-efi).
isogrub: build/kernfull.elf build/multiboot.elf
	python3 tools/mkgrubiso.py build/symbian-grub.iso

qemu-cdfull: isofull
	timeout 10 qemu-system-i386 -cdrom build/symbian-pc-full.iso -m 128 -display none -serial stdio \
	  -no-reboot 2>&1 | head -n 25; \
	echo "(exit $$? - 124 means kernel ran past timeout, which is expected)"

# UEFI boot test under OVMF (needs a private VARS copy per run).
qemu-uefi: isofull
	cp /usr/share/OVMF/OVMF_VARS.fd /tmp/ovmf_vars_test.fd
	chmod 644 /tmp/ovmf_vars_test.fd
	timeout 60 qemu-system-x86_64 -machine q35 -m 256 -display none -serial stdio \
	  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \
	  -drive if=pflash,format=raw,file=/tmp/ovmf_vars_test.fd \
	  -cdrom build/symbian-pc-full.iso -boot d 2>&1 | head -n 30; \
	echo "(exit $$? - 124 means still running past timeout)"

# Storage testing: blank 64MB HDD (created once; delete to start over).
build/hd-blank.img: tools/mkdisk.py
	python3 tools/mkdisk.py blank build/hd-blank.img 64

qemu-hd: build/kernfull.elf build/hd-blank.img
	timeout 60 qemu-system-i386 -kernel build/kernfull.elf -m 128 -display none -serial stdio \
	  -no-reboot -drive file=build/hd-blank.img,format=raw,if=ide,index=0 2>&1 | head -n 40; \
	echo "(exit $$? - 124 means kernel ran past timeout, which is expected)"

# Boot the ISO from CD-ROM emulation (out-of-the-box QEMU path).
qemu-cdrom: iso
	timeout 8 qemu-system-i386 -cdrom build/symbian-pc.iso -m 128 -display none -serial stdio \
	  -no-reboot 2>&1 | head -n 25; \
	echo "(exit $$? - 124 means GRUB+kernel ran past timeout, which is expected)"

# Serial capture to file for milestone reports.
qemu-capture: $(BRIDGE_ELF)
	timeout 5 qemu-system-i386 -kernel $(BRIDGE_ELF) -m 128 -display none -serial stdio \
	  -no-reboot > build/serial.log 2>&1; \
	cat build/serial.log

# ---------------------------------------------------------------- Genuine nanokernel scratch (compile-only)
# Genuine EKA2 nanokernel slice, compiled read-only. NOT linked into boot.
# Flags and defines are exactly per the Phase-8 objective.
GEN_FLAGS := -m32 -march=i486 -std=gnu++98 -ffreestanding -nostdlib -fno-builtin -fno-pic -fno-pie -fno-stack-protector -mno-red-zone -fno-exceptions -fno-rtti -fno-threadsafe-statics -O2
GEN_DEFS := -D__GCC32__ -D__X86__ -D__STANDALONE_NANOKERNEL__ -D__KERNEL_MODE__ -DIMPORT_C= -DEXPORT_C= -D'__NO_THROW=throw()' -D'TEMPLATE_SPECIALIZATION=template<>' -D'__cdecl=__attribute__((cdecl))' -D'NONSHARABLE_CLASS(x)=class x' -D'NONSHARABLE_STRUCT(x)=struct x'
GEN_CIA_DEFS := -D__GCC32__ -D__X86__ -D__STANDALONE_NANOKERNEL__ -D__KERNEL_MODE__ -DIMPORT_C= -DEXPORT_C= -D'__NO_THROW=throw()' -D'TEMPLATE_SPECIALIZATION=template<>' -D__CIA__ -masm=intel -D'NONSHARABLE_CLASS(x)=class x' -D'NONSHARABLE_STRUCT(x)=struct x'
GEN_INCLUDES := -I$(EKA)/include -I$(EKA)/include/nkern -I$(EKA)/include/kernel/x86 -I$(EKA)/include/nkern/x86 -I$(EKA)/common
# (GEN_BUILD/GEN_OBJS/GEN_LINK_OBJS are defined near the top: prerequisites
# expand at parse time, so they must precede the $(BRIDGE_ELF) rule above.)

genuine-nk-scratch: $(GEN_OBJS)

$(GEN_BUILD):
	mkdir -p $(GEN_BUILD)

$(GEN_BUILD)/nkern.o: $(EKA)/nkern/nkern.cpp | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_DEFS) $(GEN_INCLUDES) -c $(EKA)/nkern/nkern.cpp -o $@

$(GEN_BUILD)/sched.o: $(EKA)/nkern/sched.cpp | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_DEFS) $(GEN_INCLUDES) -c $(EKA)/nkern/sched.cpp -o $@

$(GEN_BUILD)/nk_timer.o: $(EKA)/nkern/nk_timer.cpp | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_DEFS) $(GEN_INCLUDES) -c $(EKA)/nkern/nk_timer.cpp -o $@

$(GEN_BUILD)/dfcs.o: $(EKA)/nkern/dfcs.cpp | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_DEFS) $(GEN_INCLUDES) -c $(EKA)/nkern/dfcs.cpp -o $@

$(GEN_BUILD)/nklib.o: $(EKA)/nkern/nklib.cpp | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_DEFS) $(GEN_INCLUDES) -c $(EKA)/nkern/nklib.cpp -o $@

$(GEN_BUILD)/ncglob.o: $(EKA)/nkern/x86/ncglob.cpp | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_DEFS) $(GEN_INCLUDES) -c $(EKA)/nkern/x86/ncglob.cpp -o $@

$(GEN_BUILD)/atomics.o: $(EKA)/common/x86/atomics.cia | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_CIA_DEFS) $(GEN_INCLUDES) -x c++ -c $(EKA)/common/x86/atomics.cia -o $@

$(GEN_BUILD)/ncsched.o: $(EKA)/nkern/x86/ncsched.cia | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_CIA_DEFS) $(GEN_INCLUDES) -x c++ -c $(EKA)/nkern/x86/ncsched.cia -o $@

$(GEN_BUILD)/ncthrd.o: $(EKA)/nkern/x86/ncthrd.cia | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_CIA_DEFS) $(GEN_INCLUDES) -x c++ -c $(EKA)/nkern/x86/ncthrd.cia -o $@

$(GEN_BUILD)/ncutils.o: $(EKA)/nkern/x86/ncutils.cia | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_CIA_DEFS) $(GEN_INCLUDES) -x c++ -c $(EKA)/nkern/x86/ncutils.cia -o $@

$(GEN_BUILD)/vectors.o: $(EKA)/nkern/x86/vectors.cia | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_CIA_DEFS) $(GEN_INCLUDES) -x c++ -c $(EKA)/nkern/x86/vectors.cia -o $@

$(GEN_BUILD)/nk_entry.o: $(EKA)/nkern/x86/nk_entry.cpp | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_DEFS) $(GEN_INCLUDES) -c $(EKA)/nkern/x86/nk_entry.cpp -o $@

# Phase 8 Step 2: genuine companions (mmp X86 sources omitted from the first slice).
$(GEN_BUILD)/nkerns.o: $(EKA)/nkern/nkerns.cpp | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_DEFS) $(GEN_INCLUDES) -c $(EKA)/nkern/nkerns.cpp -o $@

$(GEN_BUILD)/ncsched_cpp.o: $(EKA)/nkern/x86/ncsched.cpp | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_DEFS) $(GEN_INCLUDES) -c $(EKA)/nkern/x86/ncsched.cpp -o $@

$(GEN_BUILD)/ncthrd_cpp.o: $(EKA)/nkern/x86/ncthrd.cpp | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_DEFS) $(GEN_INCLUDES) -c $(EKA)/nkern/x86/ncthrd.cpp -o $@

$(GEN_BUILD)/ncutils_cpp.o: $(EKA)/nkern/x86/ncutils.cpp | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_DEFS) $(GEN_INCLUDES) -c $(EKA)/nkern/x86/ncutils.cpp -o $@

$(GEN_BUILD)/vectors_cpp.o: $(EKA)/nkern/x86/vectors.cpp | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_DEFS) $(GEN_INCLUDES) -c $(EKA)/nkern/x86/vectors.cpp -o $@

# Genuine kernel memset/memclr/wordmove/memmove (common/x86/cmem.cia).
$(GEN_BUILD)/cmem.o: $(EKA)/common/x86/cmem.cia | $(GEN_BUILD)
	$(CXX) $(GEN_FLAGS) $(GEN_CIA_DEFS) $(GEN_INCLUDES) -x c++ -c $(EKA)/common/x86/cmem.cia -o $@

# Phase 8 Step 2: variant glue (our code; GEN flags so genuine mangling matches).
build/genuine_glue.o: variant/genuine_glue.cpp | build
	$(CXX) $(GEN_FLAGS) $(GEN_DEFS) $(GEN_INCLUDES) -c variant/genuine_glue.cpp -o $@

# Phase 10: genuine console (VGA + PS/2 + shell NThread, genuine types).
build/genuine_console.o: variant/genuine_console.cpp | build
	$(CXX) $(GEN_FLAGS) $(GEN_DEFS) $(GEN_INCLUDES) -c variant/genuine_console.cpp -o $@

# Phase 8 Step 2: link-test. Same genuine image under its milestone name.
build/genuine_kernel.elf: $(BRIDGE_ELF)
	cp $< $@

genuine-link-test: build/genuine_kernel.elf

clean:
	rm -rf build
