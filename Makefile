.PHONY: alienos clean

alienos:
	i686-elf-as boot.s -o boot.o
	i686-elf-gcc -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
	i686-elf-gcc -T linker.ld -o alienos.bin -ffreestanding -O2 -nostdlib boot.o kernel.o -lgcc

	@if grub-file --is-x86-multiboot alienos.bin; then \
		echo "multiboot confirmed"; \
		mkdir -p isodir/boot/grub; \
		cp alienos.bin isodir/boot/alienos.bin; \
		cp grub.cfg isodir/boot/grub/grub.cfg; \
		grub-mkrescue -o alienos.iso isodir; \
	else \
		echo "the file is not multiboot"; \
	fi

clean:
	rm -rf isodir alienos.bin alienos.iso boot.o kernel.o