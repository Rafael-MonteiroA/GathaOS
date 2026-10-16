SHELL := /bin/bash
.DELETE_ON_ERROR:
CC := clang
LD := ld.lld
OBJCOPY := llvm-objcopy
NASM := nasm
PYTHON := python3
QEMU ?= qemu-system-i386
DEBUG_FAULT ?= none
PS2_SET ?= 2
INPUT_SUFFIX := $(if $(filter 1,$(PS2_SET)),-set1,)
ifeq ($(DEBUG_FAULT),none)
FAULT_ID := 0
SUFFIX :=
else ifeq ($(DEBUG_FAULT),div0)
FAULT_ID := 1
SUFFIX := -div0
else ifeq ($(DEBUG_FAULT),gp)
FAULT_ID := 2
SUFFIX := -gp
else ifeq ($(DEBUG_FAULT),page)
FAULT_ID := 3
SUFFIX := -page
else ifeq ($(DEBUG_FAULT),readonly)
FAULT_ID := 4
SUFFIX := -readonly
else
$(error DEBUG_FAULT must be none, div0, gp, page or readonly)
endif
BUILD := build/$(DEBUG_FAULT)$(INPUT_SUFFIX)
ISO ?= dist/gathaos-stage6-desktop$(SUFFIX)$(INPUT_SUFFIX).iso
PAGE_ISO ?= build/gathaos-stage6-desktop-page.iso
DISK ?= dist/gathaos-stage6-disk.img
RAM ?= 256
ifneq ($(filter $(PS2_SET),1 2),$(PS2_SET))
$(error PS2_SET must be 1 or 2)
endif
CFLAGS := --target=i686-elf -std=c11 -O2 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -Wall -Wextra -Werror -MMD -MP -Ikernel -DDEBUG_FAULT=$(FAULT_ID)
SOURCES := kernel/main.c kernel/drivers/serial.c kernel/panic.c kernel/arch/x86/gdt.c kernel/arch/x86/idt.c kernel/arch/x86/pic.c kernel/arch/x86/pit.c kernel/mm/pmm.c kernel/mm/paging.c kernel/mm/heap.c kernel/mm/selftest.c libgatha/memory.c
CFLAGS += -DPS2_SET=$(PS2_SET)
SOURCES += kernel/drivers/framebuffer.c kernel/drivers/ps2.c kernel/drivers/keyboard.c kernel/drivers/mouse.c kernel/drivers/input_selftest.c kernel/graphics_demo.c gui/gfx/gfx.c
SOURCES += kernel/sched/task.c kernel/sched/demo.c kernel/syscall/syscall.c libgatha/string.c
SOURCES += kernel/drivers/ata.c kernel/drivers/rtc.c kernel/fs/gathafs.c kernel/fs/vfs.c kernel/fs/selftest.c
OBJECTS := $(BUILD)/entry.o $(BUILD)/interrupts.o $(patsubst %.c,$(BUILD)/%.o,$(SOURCES))
OBJECTS += $(BUILD)/demo-gex.o $(BUILD)/widgets-gex.o
OBJECTS += $(BUILD)/gui/wm/wm.o $(BUILD)/gui/wm/launcher.o
.PHONY: all iso run test test-boot clean
all: iso
$(BUILD)/user-start.o: libgatha/start.asm | $(BUILD)
	$(NASM) -Wall -Werror -f elf32 $< -o $@
$(BUILD)/demo-user.o: apps/demo/main.c libgatha/api.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/demo.elf: $(BUILD)/user-start.o $(BUILD)/demo-user.o $(BUILD)/libgatha/memory.o $(BUILD)/libgatha/string.o libgatha/user.ld
	$(LD) -m elf_i386 --fatal-warnings -T libgatha/user.ld -o $@ $(filter %.o,$^)
$(BUILD)/demo.gex: $(BUILD)/demo.elf tools/gex.py
	$(OBJCOPY) -O binary $< $(BUILD)/demo.bin
	$(PYTHON) tools/gex.py $< $(BUILD)/demo.bin $@ demo_gex
$(BUILD)/demo-gex.o: $(BUILD)/demo.gex
	$(CC) $(CFLAGS) -c $(BUILD)/demo.c -o $@
$(BUILD)/widgets.elf: $(BUILD)/user-start.o $(BUILD)/apps/widgets/main.o $(BUILD)/gui/toolkit/ui.o $(BUILD)/libgatha/memory.o $(BUILD)/libgatha/string.o libgatha/user.ld
	$(LD) -m elf_i386 --fatal-warnings -T libgatha/user.ld -o $@ $(filter %.o,$^)
$(BUILD)/widgets.gex: $(BUILD)/widgets.elf tools/gex.py
	$(OBJCOPY) -O binary $< $(BUILD)/widgets.bin
	$(PYTHON) tools/gex.py $< $(BUILD)/widgets.bin $@ widgets_gex
$(BUILD)/widgets-gex.o: $(BUILD)/widgets.gex
	$(CC) $(CFLAGS) -c $(BUILD)/widgets.c -o $@
iso: $(ISO) $(DISK) dist/gathaos-disk.img
$(DISK): tools/mkgathafs.py $(shell find rootfs -type f)
	$(PYTHON) tools/mkgathafs.py rootfs "$@"
dist/gathaos-disk.img: $(DISK)
	test -e "$@" || cp "$(DISK)" "$@"
$(BUILD):
	mkdir -p $@
$(BUILD)/entry.o: kernel/arch/x86/entry.asm | $(BUILD)
	$(NASM) -Wall -Werror -f elf32 $< -o $@
$(BUILD)/interrupts.o: kernel/arch/x86/interrupts.asm | $(BUILD)
	$(NASM) -Wall -Werror -DDEBUG_FAULT=$(FAULT_ID) -f elf32 $< -o $@
$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/kernel.elf: $(OBJECTS) kernel/linker.ld
	$(LD) -m elf_i386 --fatal-warnings -T kernel/linker.ld -Map=$(BUILD)/kernel.map -o $@ $(OBJECTS)
$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@
$(BUILD)/kernel-size.inc: $(BUILD)/kernel.bin tools/mkboot.py
	$(PYTHON) tools/mkboot.py sizes $< $@
$(BUILD)/stage1.bin: boot/stage1.asm boot/disk.inc | $(BUILD)
	$(NASM) -Wall -Werror -f bin $< -o $@
$(BUILD)/stage2.bin: boot/stage2.asm boot/disk.inc $(BUILD)/kernel-size.inc
	$(NASM) -Wall -Werror -I$(BUILD)/ -f bin $< -o $@
$(BUILD)/boot.img: $(BUILD)/stage1.bin $(BUILD)/stage2.bin $(BUILD)/kernel.bin tools/mkboot.py
	$(PYTHON) tools/mkboot.py image $(BUILD) $@
$(ISO): $(BUILD)/boot.img scripts/make-iso.sh
	bash scripts/make-iso.sh $(BUILD)/boot.img "$@" $(BUILD)
run: iso
	QEMU="$(QEMU)" bash scripts/run-qemu.sh "$(ISO)" dist/gathaos-disk.img
test-boot: iso
	QEMU="$(QEMU)" PS2_SET=$(PS2_SET) DISK="$(DISK)" bash scripts/smoke-test.sh "$(ISO)" $(DEBUG_FAULT) $(BUILD)/kernel.elf $(RAM)
test: test-boot
	$(PYTHON) tools/test_gathafs.py
ifeq ($(DEBUG_FAULT),none)
	$(MAKE) RAM=64 test-boot
	$(MAKE) RAM=512 test-boot
	$(MAKE) PS2_SET=1 BUILD=build/set1 ISO=build/gathaos-stage6-set1.iso test-boot
	$(MAKE) DEBUG_FAULT=page ISO="$(PAGE_ISO)" test-boot
	$(MAKE) DEBUG_FAULT=readonly ISO=build/gathaos-stage6-desktop-readonly.iso test-boot
	$(MAKE) DEBUG_FAULT=div0 ISO=build/gathaos-stage6-desktop-div0.iso test-boot
	$(MAKE) DEBUG_FAULT=gp ISO=build/gathaos-stage6-desktop-gp.iso test-boot
endif
clean:
	rm -rf build
-include $(OBJECTS:.o=.d)
