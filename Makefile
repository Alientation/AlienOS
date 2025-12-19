.PHONY: qemu clean

alienos:
	i686-elf-as boot.s -o build/boot.o
	i686-elf-gcc -c kernel.c -o build/kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	i686-elf-gcc -T linker.ld -o alienos.bin -ffreestanding -O2 -nostdlib build/boot.o build/kernel.o -lgcc

	@if grub-file --is-x86-multiboot alienos.bin; then \
		echo "multiboot confirmed"; \
		mkdir -p build/isodir/boot/grub; \
		cp alienos.bin build/isodir/boot/alienos.bin; \
		cp grub.cfg build/isodir/boot/grub/grub.cfg; \
		grub-mkrescue -o alienos.iso build/isodir; \
	else \
		echo "the file is not multiboot"; \
	fi

qemu: alienos
	qemu-system-i386 -cdrom alienos.iso

clean:
	rm -rf build alienos.bin alienos.iso