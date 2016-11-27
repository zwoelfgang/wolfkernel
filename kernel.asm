;;kernel.asm
bits 32					;nasm directive - 32 bit
section .text
	;multiboot spec
	align 4
        dd 0x1BADB002            	;magic
        dd 0x00                  	;flags
        dd - (0x1BADB002 + 0x00) 	;checksum, m+f+c should = 0

global boot
global keyboard_handler
global enable_a20
global read_port
global write_port
global load_idt
global display_mode

extern kmain        			;kmain/keyboard_handler_main are defined in the C file
extern keyboard_handler_main

enable_a20:
   cli

   call a20_wait
   mov al, 0xad
   out 0x64, al

   call a20_wait
   mov al, 0xd0
   out 0x64, al
 
   call a20_wait2
   in al, 0x60
   push eax
 
   call a20_wait
   mov al, 0xd1
   out 0x64, al
 
   call a20_wait
   pop eax
   or al, 2
   out 0x60, al
 
   call a20_wait
   mov al, 0xae
   out 0x64, al
 
   call a20_wait
   sti
   ret

a20_wait:
   in al, 0x64
   test al, 2
   jnz a20_wait
   ret

a20_wait2:
   in al, 0x64
   test al, 1
   jz a20_wait2
   ret 

read_port:
   mov edx, [esp + 4]
   in al, dx               		;al is the lower 8 bits of eax
   ret					;dx is the lower 16 bits of edx

write_port:
   mov edx, [esp + 4]    
   mov al, [esp + 4 + 4]  
   out dx, al
   ret

load_idt:
   mov edx, [esp + 4]
   lidt [edx]
   sti
   ret

display_mode:
   mov edx, [esp + 4]
   mov ah, 0x00
   out dx, al				;switch between ASCII text and 256-color VGA modes
   int 0x10
   ret

boot:
   cli                                  ;block interrupts
   mov esp, stack_space                 ;set stack pointer
   call kmain
   hlt                                  ;halt the CPU
   jmp 1b                               ;jump to 'hlt' in case of unmaskable interrupt

keyboard_handler:
   call keyboard_handler_main
   iretd

section .bss
align 16
resb 65536				;64KB for stack
stack_space:
