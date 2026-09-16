CROSS ?= x86_64-elf
CC := $(CROSS)-gcc
LD := $(CROSS)-ld
AS := nasm

CFLAGS := -std=c11 -ffreestanding -O2 -Wall -Wextra -Werror -mno-red-zone -mno-mmx -mno-sse -fno-stack-protector -fno-pic -I src/include
LDFLAGS := -nostdlib -z max-page-size=0x1000

C_SOURCES := $(shell find src -name '*.c')
ASM_SOURCES := $(shell find src -name '*.asm')
C_OBJECTS := $(patsubst src/%.c,build/%.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst src/%.asm,build/%.o,$(ASM_SOURCES))
OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)

.PHONY: all iso run clean
all: iso

build/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.asm
	mkdir -p $(dir $@)
	$(AS) -f elf64 $< -o $@

dist/chutos.kernel: $(OBJECTS) x86_64/linker.ld
	mkdir -p dist
	$(LD) $(LDFLAGS) -T x86_64/linker.ld -o $@ $(OBJECTS)

iso: dist/chutos.kernel
	mkdir -p dist/iso/boot/grub
	cp dist/chutos.kernel dist/iso/boot/chutos.kernel
	grub-mkrescue -o dist/chutos.iso dist/iso

run: iso
	qemu-system-x86_64 -cdrom dist/chutos.iso -serial stdio -no-reboot

clean:
	rm -rf build dist
