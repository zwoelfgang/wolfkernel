#!/bin/bash

nasm -f elf32 kernel.asm -o kasm.o
i686-elf-gcc -Wall -m32 -c kernel.c -o kc.o
ld -m elf_i386 -T link.ld -o kernel-0.1 kasm.o kc.o

qemu-system-i386 -kernel kernel-0.1
