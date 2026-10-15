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
ISO ?= dist/gathaos-stage3-drivers$(SUFFIX)$(INPUT_SUFFIX).iso
PAGE_ISO ?= build/gathaos-stage3-drivers-page.iso
RAM ?= 256
ifneq ($(filter $(PS2_SET),1 2),$(PS2_SET))
$(error PS2_SET must be 1 or 2)
endif
CFLAGS := --target=i686-elf -std=c11 -O2 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -Wall -Wextra -Werror -MMD -MP -Ikernel -DDEBUG_FAULT=$(FAULT_ID)
SOURCES := kernel/main.c kernel/drivers/serial.c kernel/panic.c kernel/arch/x86/gdt.c kernel/arch/x86/idt.c kernel/arch/x86/pic.c kernel/arch/x86/pit.c kernel/mm/pmm.c kernel/mm/paging.c kernel/mm/heap.c kernel/mm/selftest.c libgatha/memory.c
CFLAGS += -DPS2_SET=$(PS2_SET)
SOURCES += kernel/drivers/framebuffer.c kernel/drivers/ps2.c kernel/drivers/keyboard.c kernel/drivers/mouse.c kernel/drivers/input_selftest.c kernel/graphics_demo.c gui/gfx/gfx.c
OBJECTS := $(BUILD)/entry.o $(BUILD)/interrupts.o $(patsubst %.c,$(BUILD)/%.o,$(SOURCES))
.PHONY: all iso run test test-boot clean
all: iso
iso: $(ISO)
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
	QEMU="$(QEMU)" bash scripts/run-qemu.sh "$(ISO)"
test-boot: iso
	QEMU="$(QEMU)" PS2_SET=$(PS2_SET) bash scripts/smoke-test.sh "$(ISO)" $(DEBUG_FAULT) $(BUILD)/kernel.elf $(RAM)
test: test-boot
ifeq ($(DEBUG_FAULT),none)
	$(MAKE) RAM=64 test-boot
	$(MAKE) RAM=512 test-boot
	$(MAKE) PS2_SET=1 BUILD=build/set1 ISO=build/gathaos-stage3-set1.iso test-boot
	$(MAKE) DEBUG_FAULT=page ISO="$(PAGE_ISO)" test-boot
	$(MAKE) DEBUG_FAULT=readonly ISO=build/gathaos-stage3-drivers-readonly.iso test-boot
	$(MAKE) DEBUG_FAULT=div0 ISO=build/gathaos-stage3-drivers-div0.iso test-boot
	$(MAKE) DEBUG_FAULT=gp ISO=build/gathaos-stage3-drivers-gp.iso test-boot
endif
clean:
	rm -rf build
-include $(OBJECTS:.o=.d)
