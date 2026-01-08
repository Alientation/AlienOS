# Tools
CC = i686-elf-gcc
AR = i686-elf-ar
AS = i686-elf-as

# Include paths to kernel and c library headers
INCLUDES = -Iinclude -Ilibc/include

# Flags
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra $(INCLUDES)
LDFLAGS = -ffreestanding -fno-builtin -nostdinc -O2 -nostdlib -lgcc -T linker.ld

# Sources
KERNEL_CSRCS = $(wildcard src/*/*.c)
KERNEL_ASRCS = $(wildcard src/*/*.s)
LIBC_SRCS = $(wildcard libc/src/*.c)

# Objects
KERNEL_OBJS := $(patsubst src/%.c, build/%.o, $(KERNEL_CSRCS))
KERNEL_OBJS += $(patsubst src/%.s, build/%.o, $(KERNEL_ASRCS))
LIBC_OBJS := $(patsubst libc/src/%.c, build/libc/%.o, $(LIBC_SRCS))

.PHONY: all clean qemu test build build/isodir/boot/grub

all: iso/alienos.iso

test: clean
	$(eval CFLAGS += -DALIENOS_TEST)

# Create build directories
build build/isodir/boot/grub:
	@mkdir -p $@

# Build c library archive
build/libk.a: $(LIBC_OBJS)
	$(AR) rcs $@ $(LIBC_OBJS)

# Kernel C files
build/%.o: src/%.c | build
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

# Kernel Assembly files
build/%.o: src/%.s | build
	@mkdir -p $(dir $@)
	$(AS) $< -o $@

# C Library files
build/libc/%.o: libc/src/%.c | build
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

# Link
iso/alienos.bin: $(KERNEL_OBJS) build/libk.a
	$(CC) -o $@ $(KERNEL_OBJS) build/libk.a $(LDFLAGS) -lgcc

# Create ISO
iso/alienos.iso: iso/alienos.bin | build/isodir/boot/grub
	@if grub-file --is-x86-multiboot iso/alienos.bin; then \
		echo "Multiboot confirmed"; \
		cp iso/alienos.bin build/isodir/boot/alienos.bin; \
		cp iso/grub.cfg build/isodir/boot/grub/grub.cfg; \
		grub-mkrescue -o $@ build/isodir; \
	else \
		echo "Error: The file is not multiboot"; \
		exit 1; \
	fi

# Start QEMU
qemu: all
	qemu-system-i386 -cdrom iso/alienos.iso -serial stdio
#	-accel kvm -cpu max
# REQUIRES WSL2 + WINDOWS 11

clean:
	rm -rf build iso/alienos.bin iso/alienos.iso