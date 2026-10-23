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
ISO ?= dist/gathaos-stage11-https$(SUFFIX)$(INPUT_SUFFIX).iso
PAGE_ISO ?= build/gathaos-stage11-https-page.iso
DISK ?= dist/gathaos-stage11-disk.img
RUNTIME_DISK ?= dist/gathaos-https-disk.img
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
OBJECTS += $(BUILD)/kernel/drivers/pcnet.o $(addprefix $(BUILD)/kernel/net/,ethernet.o dhcp.o dns.o tcp.o syscall.o)
OBJECTS += $(BUILD)/kernel/drivers/security.o $(BUILD)/libgatha/crypto/sha256.o
CRYPTO := $(addprefix $(BUILD)/libgatha/crypto/,sha256.o sha512.o aes_gcm.o x25519.o rsa.o)
TLS := $(BUILD)/libgatha/tls/tls.o $(BUILD)/libgatha/tls/x509.o $(CRYPTO)
APPS := terminal files editor calculator settings sysmon paint about
OBJECTS += $(BUILD)/kernel/power.o $(addprefix $(BUILD)/,$(addsuffix -gex.o,$(APPS)))
OBJECTS += $(BUILD)/demo-gex.o $(BUILD)/widgets-gex.o
OBJECTS += $(BUILD)/gui/wm/wm.o $(BUILD)/gui/wm/launcher.o
.PHONY: all iso run test test-boot clean
all: iso
$(addprefix $(BUILD)/,$(addsuffix .elf,$(APPS) demo widgets foundation)): $(BUILD)/libgatha/alloc.o
$(BUILD)/terminal.elf: $(BUILD)/libgatha/network.o $(TLS)
$(BUILD)/tlscheck.elf: $(BUILD)/user-start.o $(BUILD)/apps/tlscheck/main.o $(BUILD)/libgatha/memory.o $(BUILD)/libgatha/string.o $(BUILD)/libgatha/alloc.o $(TLS) libgatha/user.ld
	$(LD) -m elf_i386 --fatal-warnings -T libgatha/user.ld -o $@ $(filter %.o,$^)
$(BUILD)/tlscheck.gex: $(BUILD)/tlscheck.elf tools/gex.py
	$(OBJCOPY) -O binary $< $(BUILD)/tlscheck.bin
	$(PYTHON) tools/gex.py $< $(BUILD)/tlscheck.bin $@
$(BUILD)/libgatha/tls/tls.o: $(BUILD)/trust-roots.h
$(BUILD)/libgatha/tls/tls.o: CFLAGS += -I$(BUILD)
$(BUILD)/trust-roots.h: tools/embed_roots.py $(wildcard certs/*)
	$(PYTHON) tools/embed_roots.py $@
$(BUILD)/network.elf: $(BUILD)/user-start.o $(BUILD)/apps/network/main.o $(BUILD)/libgatha/memory.o $(BUILD)/libgatha/string.o libgatha/user.ld
	$(LD) -m elf_i386 --fatal-warnings -T libgatha/user.ld -o $@ $(filter %.o,$^)
$(BUILD)/network.gex: $(BUILD)/network.elf tools/gex.py
	$(OBJCOPY) -O binary $< $(BUILD)/network.bin
	$(PYTHON) tools/gex.py $< $(BUILD)/network.bin $@
$(BUILD)/foundation.elf: $(BUILD)/user-start.o $(BUILD)/apps/foundation/main.o $(BUILD)/libgatha/memory.o $(BUILD)/libgatha/string.o libgatha/user.ld
	$(LD) -m elf_i386 --fatal-warnings -T libgatha/user.ld -o $@ $(filter %.o,$^)
$(BUILD)/foundation.gex: $(BUILD)/foundation.elf tools/gex.py
	$(OBJCOPY) -O binary $< $(BUILD)/foundation.bin
	$(PYTHON) tools/gex.py $< $(BUILD)/foundation.bin $@
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
$(BUILD)/terminal.elf: $(BUILD)/user-start.o $(BUILD)/apps/terminal/main.o $(BUILD)/apps/common.o $(BUILD)/gui/toolkit/ui.o $(BUILD)/libgatha/memory.o $(BUILD)/libgatha/string.o $(BUILD)/libgatha/arith.o libgatha/user.ld
	$(LD) -m elf_i386 --fatal-warnings -T libgatha/user.ld -o $@ $(filter %.o,$^)
$(BUILD)/terminal.gex: $(BUILD)/terminal.elf tools/gex.py
	$(OBJCOPY) -O binary $< $(BUILD)/terminal.bin
	$(PYTHON) tools/gex.py $< $(BUILD)/terminal.bin $@ terminal_gex
$(BUILD)/terminal-gex.o: $(BUILD)/terminal.gex
	$(CC) $(CFLAGS) -c $(BUILD)/terminal.c -o $@
$(BUILD)/files.elf: $(BUILD)/user-start.o $(BUILD)/apps/files/main.o $(BUILD)/apps/common.o $(BUILD)/gui/toolkit/ui.o $(BUILD)/libgatha/memory.o $(BUILD)/libgatha/string.o $(BUILD)/libgatha/arith.o libgatha/user.ld
	$(LD) -m elf_i386 --fatal-warnings -T libgatha/user.ld -o $@ $(filter %.o,$^)
$(BUILD)/files.gex: $(BUILD)/files.elf tools/gex.py
	$(OBJCOPY) -O binary $< $(BUILD)/files.bin
	$(PYTHON) tools/gex.py $< $(BUILD)/files.bin $@ files_gex
$(BUILD)/files-gex.o: $(BUILD)/files.gex
	$(CC) $(CFLAGS) -c $(BUILD)/files.c -o $@
$(BUILD)/editor.elf: $(BUILD)/user-start.o $(BUILD)/apps/editor/main.o $(BUILD)/apps/common.o $(BUILD)/gui/toolkit/ui.o $(BUILD)/libgatha/memory.o $(BUILD)/libgatha/string.o $(BUILD)/libgatha/arith.o libgatha/user.ld
	$(LD) -m elf_i386 --fatal-warnings -T libgatha/user.ld -o $@ $(filter %.o,$^)
$(BUILD)/editor.gex: $(BUILD)/editor.elf tools/gex.py
	$(OBJCOPY) -O binary $< $(BUILD)/editor.bin
	$(PYTHON) tools/gex.py $< $(BUILD)/editor.bin $@ editor_gex
$(BUILD)/editor-gex.o: $(BUILD)/editor.gex
	$(CC) $(CFLAGS) -c $(BUILD)/editor.c -o $@
$(BUILD)/calculator.elf: $(BUILD)/user-start.o $(BUILD)/apps/calculator/main.o $(BUILD)/apps/common.o $(BUILD)/gui/toolkit/ui.o $(BUILD)/libgatha/memory.o $(BUILD)/libgatha/string.o $(BUILD)/libgatha/arith.o libgatha/user.ld
	$(LD) -m elf_i386 --fatal-warnings -T libgatha/user.ld -o $@ $(filter %.o,$^)
$(BUILD)/calculator.gex: $(BUILD)/calculator.elf tools/gex.py
	$(OBJCOPY) -O binary $< $(BUILD)/calculator.bin
	$(PYTHON) tools/gex.py $< $(BUILD)/calculator.bin $@ calculator_gex
$(BUILD)/calculator-gex.o: $(BUILD)/calculator.gex
	$(CC) $(CFLAGS) -c $(BUILD)/calculator.c -o $@
$(BUILD)/settings.elf: $(BUILD)/user-start.o $(BUILD)/apps/settings/main.o $(BUILD)/apps/common.o $(BUILD)/gui/toolkit/ui.o $(BUILD)/libgatha/memory.o $(BUILD)/libgatha/string.o $(BUILD)/libgatha/arith.o libgatha/user.ld
	$(LD) -m elf_i386 --fatal-warnings -T libgatha/user.ld -o $@ $(filter %.o,$^)
$(BUILD)/settings.gex: $(BUILD)/settings.elf tools/gex.py
	$(OBJCOPY) -O binary $< $(BUILD)/settings.bin
	$(PYTHON) tools/gex.py $< $(BUILD)/settings.bin $@ settings_gex
$(BUILD)/settings-gex.o: $(BUILD)/settings.gex
	$(CC) $(CFLAGS) -c $(BUILD)/settings.c -o $@
$(BUILD)/sysmon.elf: $(BUILD)/user-start.o $(BUILD)/apps/sysmon/main.o $(BUILD)/apps/common.o $(BUILD)/gui/toolkit/ui.o $(BUILD)/libgatha/memory.o $(BUILD)/libgatha/string.o $(BUILD)/libgatha/arith.o libgatha/user.ld
	$(LD) -m elf_i386 --fatal-warnings -T libgatha/user.ld -o $@ $(filter %.o,$^)
$(BUILD)/sysmon.gex: $(BUILD)/sysmon.elf tools/gex.py
	$(OBJCOPY) -O binary $< $(BUILD)/sysmon.bin
	$(PYTHON) tools/gex.py $< $(BUILD)/sysmon.bin $@ sysmon_gex
$(BUILD)/sysmon-gex.o: $(BUILD)/sysmon.gex
	$(CC) $(CFLAGS) -c $(BUILD)/sysmon.c -o $@
$(BUILD)/paint.elf: $(BUILD)/user-start.o $(BUILD)/apps/paint/main.o $(BUILD)/apps/common.o $(BUILD)/gui/toolkit/ui.o $(BUILD)/libgatha/memory.o $(BUILD)/libgatha/string.o $(BUILD)/libgatha/arith.o libgatha/user.ld
	$(LD) -m elf_i386 --fatal-warnings -T libgatha/user.ld -o $@ $(filter %.o,$^)
$(BUILD)/paint.gex: $(BUILD)/paint.elf tools/gex.py
	$(OBJCOPY) -O binary $< $(BUILD)/paint.bin
	$(PYTHON) tools/gex.py $< $(BUILD)/paint.bin $@ paint_gex
$(BUILD)/paint-gex.o: $(BUILD)/paint.gex
	$(CC) $(CFLAGS) -c $(BUILD)/paint.c -o $@
$(BUILD)/about.elf: $(BUILD)/user-start.o $(BUILD)/apps/about/main.o $(BUILD)/apps/common.o $(BUILD)/gui/toolkit/ui.o $(BUILD)/libgatha/memory.o $(BUILD)/libgatha/string.o $(BUILD)/libgatha/arith.o libgatha/user.ld
	$(LD) -m elf_i386 --fatal-warnings -T libgatha/user.ld -o $@ $(filter %.o,$^)
$(BUILD)/about.gex: $(BUILD)/about.elf tools/gex.py
	$(OBJCOPY) -O binary $< $(BUILD)/about.bin
	$(PYTHON) tools/gex.py $< $(BUILD)/about.bin $@ about_gex
$(BUILD)/about-gex.o: $(BUILD)/about.gex
	$(CC) $(CFLAGS) -c $(BUILD)/about.c -o $@
iso: $(ISO) $(DISK) $(RUNTIME_DISK)
ifeq ($(ISO),dist/gathaos-stage8-release.iso)
iso: dist/gathaos-1.0-rc1.iso
dist/gathaos-1.0-rc1.iso: $(ISO)
	if test -e "$@"; then cmp "$<" "$@"; else cp "$<" "$@"; fi
endif
$(DISK): tools/mkgathafs.py $(shell find rootfs -type f) $(addprefix $(BUILD)/,$(addsuffix .gex,$(APPS))) $(BUILD)/foundation.gex $(BUILD)/network.gex $(BUILD)/tlscheck.gex
	mkdir -p $(BUILD)/rootfs/System/Apps
	cp -a rootfs/. $(BUILD)/rootfs/
	cp $(addprefix $(BUILD)/,$(addsuffix .gex,$(APPS))) $(BUILD)/rootfs/System/Apps/
	mkdir -p $(BUILD)/rootfs/System/Tests
	cp $(BUILD)/foundation.gex $(BUILD)/rootfs/System/Tests/
	cp $(BUILD)/network.gex $(BUILD)/rootfs/System/Tests/
	cp $(BUILD)/tlscheck.gex $(BUILD)/rootfs/System/Tests/
	$(PYTHON) tools/mkgathafs.py $(BUILD)/rootfs "$@"
$(RUNTIME_DISK): $(DISK)
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
	QEMU="$(QEMU)" bash scripts/run-qemu.sh "$(ISO)" "$(RUNTIME_DISK)"
test-boot: iso
	QEMU="$(QEMU)" PS2_SET=$(PS2_SET) DISK="$(DISK)" bash scripts/smoke-test.sh "$(ISO)" $(DEBUG_FAULT) $(BUILD)/kernel.elf $(RAM)
test: test-boot
	$(PYTHON) tools/crypto_test.py
	$(PYTHON) tools/x509_test.py
	$(PYTHON) tools/test_gathafs.py
	clang -std=c11 -Wall -Wextra -Werror -I. -Ikernel tools/fs_native_test.c kernel/fs/gathafs.c libgatha/string.c -o build/fs-native-test
	build/fs-native-test "$(DISK)"
	$(PYTHON) tools/mkgathafs.py check build/native-full-disk.img
ifeq ($(DEBUG_FAULT),none)
	$(MAKE) RAM=64 test-boot
	$(MAKE) RAM=512 test-boot
	$(MAKE) PS2_SET=1 BUILD=build/set1 ISO=build/gathaos-stage10-set1.iso test-boot
	$(MAKE) DEBUG_FAULT=page ISO="$(PAGE_ISO)" test-boot
	$(MAKE) DEBUG_FAULT=readonly ISO=build/gathaos-stage10-network-readonly.iso test-boot
	$(MAKE) DEBUG_FAULT=div0 ISO=build/gathaos-stage10-network-div0.iso test-boot
	$(MAKE) DEBUG_FAULT=gp ISO=build/gathaos-stage10-network-gp.iso test-boot
	$(PYTHON) tools/foundation_test.py "$(ISO)" "$(DISK)"
	$(PYTHON) tools/network_test.py "$(ISO)" "$(DISK)"
	$(PYTHON) tools/tls_test.py "$(ISO)" "$(DISK)"
	$(PYTHON) tools/power_test.py "$(ISO)" "$(DISK)"
	$(PYTHON) tools/soak_test.py "$(ISO)" "$(DISK)"
endif
clean:
	rm -rf build
-include $(OBJECTS:.o=.d)
-include $(BUILD)/libgatha/network.d $(BUILD)/apps/network/main.d
-include $(TLS:.o=.d)
