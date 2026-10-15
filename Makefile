SHELL := /bin/bash
.DELETE_ON_ERROR:
CC := clang
LD := ld.lld
OBJCOPY := llvm-objcopy
NASM := nasm
PYTHON := python3
QEMU ?= qemu-system-i386
ISO ?= dist/gathaos-stage0-boot.iso
CFLAGS := --target=i686-elf -std=c11 -O2 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -fno-pie -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -Wall -Wextra -Werror -MMD -MP
OBJECTS := build/entry.o build/main.o build/serial.o
.PHONY: all iso run test clean
all: iso
iso: $(ISO)
build:
	mkdir -p build
build/entry.o: kernel/arch/x86/entry.asm | build
	$(NASM) -Wall -Werror -f elf32 $< -o $@
build/main.o: kernel/main.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/serial.o: kernel/drivers/serial.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/kernel.elf: $(OBJECTS) kernel/linker.ld
	$(LD) -m elf_i386 --fatal-warnings -T kernel/linker.ld -Map=build/kernel.map -o $@ $(OBJECTS)
build/kernel.bin: build/kernel.elf
	$(OBJCOPY) -O binary $< $@
build/kernel-size.inc: build/kernel.bin tools/mkboot.py
	$(PYTHON) tools/mkboot.py sizes $< $@
build/stage1.bin: boot/stage1.asm boot/disk.inc | build
	$(NASM) -Wall -Werror -f bin $< -o $@
build/stage2.bin: boot/stage2.asm boot/disk.inc build/kernel-size.inc
	$(NASM) -Wall -Werror -f bin $< -o $@
build/boot.img: build/stage1.bin build/stage2.bin build/kernel.bin tools/mkboot.py
	$(PYTHON) tools/mkboot.py image build $@
$(ISO): build/boot.img scripts/make-iso.sh
	bash scripts/make-iso.sh build/boot.img "$@"
run: iso
	QEMU="$(QEMU)" bash scripts/run-qemu.sh "$(ISO)"
test: iso
	QEMU="$(QEMU)" bash scripts/smoke-test.sh "$(ISO)"
clean:
	rm -rf build
-include build/main.d build/serial.d
