/*
*  kernel.c, ver. 0.1
*/

#include "keyboard_map.h"

#define LINES 25
#define COLUMNS 80
#define SCREENSIZE 4000

#define VGA_X 320
#define VGA_Y 200

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define IDT_SIZE 256

typedef int size_t;

extern unsigned char keyboard_map[128];
extern void keyboard_handler();
extern void enable_a20();
extern char read_port(unsigned short port);
extern void write_port(unsigned short port, unsigned char data);
extern void load_idt(unsigned long *idt_ptr);
extern void display_mode(unsigned short mode);

extern void *memcpy(void *dest, const void *src, size_t count) {
    
	const char *sp = (const char *)src;
	char *dp = (char *)dest;
	for(; count != 0; count--) *dp++ = *sp++;
	return dest;
}

extern char *memsetw(char *dest, unsigned short val, size_t count) {
    
	unsigned short *temp = (unsigned short *)dest;
	for( ; count != 0; count--) *temp++ = val;
	return dest;
}

extern size_t strlen(const char *str) {

	size_t retval;
	for(retval = 0; *str != '\0'; str++) retval++;
	return retval;
}

const char *str = "$****************************$_ Wolf's __ Kernel _$****************************$";
unsigned int current_loc = 0;
char *vidptr = (char *)0xb8000; //video memory begins here
int csr_x = 0, csr_y = 0;

struct IDT_entry {
        unsigned short int offset_lowerbits;
        unsigned short int selector;
        unsigned char zero;
        unsigned char type_attr;
        unsigned short int offset_higherbits;
};

struct IDT_entry IDT[IDT_SIZE];

/* Scrolls the screen */
void scroll() {

        unsigned blank, temp;

	blank = 0x20 | (0x0b << 8);

        /* Row 25 is the end, this means we need to scroll up */
        if(csr_y >= LINES) {
                /* Move the current text chunk that makes up the screen
                *  back in the buffer by a line */
                temp = csr_y - LINES + 1;
                memcpy(vidptr, vidptr + temp * COLUMNS, (LINES - temp) * COLUMNS * 2);

                /* Finally, we set the chunk of memory that occupies
                *  the last line of text to our 'blank' character */
                memsetw (vidptr + (LINES - temp) * COLUMNS, blank, COLUMNS);
                csr_y = LINES - 1;
        }
}

/* Updates the hardware cursor: the little blinking line
*  on the screen under the last character pressed! */
void move_csr() {

	/* This sends a command to indicies 14 and 15 in the
        *  CRT Control Register of the VGA controller. These
        *  are the high and low bytes of the index that show
        *  where the hardware cursor is to be 'blinking'. To
        *  learn more, you should look up some VGA specific
        *  programming documents. A great start to graphics:
        *  http://www.brackeen.com/home/vga */
        write_port(0x03d4, 14);
        write_port(0x03d5, (current_loc / 2) >> 8);
        write_port(0x03d4, 15);
        write_port(0x03d5, current_loc / 2);
}

void idt_init() {

	unsigned long keyboard_address;
	unsigned long idt_address;
	unsigned long idt_ptr[2];

	/* populate IDT entry of keyboard's interrupt */
	keyboard_address = (unsigned long)keyboard_handler; 
	IDT[0x21].offset_lowerbits = keyboard_address & 0xffff;
	IDT[0x21].selector = 0x08; /* KERNEL_CODE_SEGMENT_OFFSET */
	IDT[0x21].zero = 0;
	IDT[0x21].type_attr = 0x8e; /* INTERRUPT_GATE */
	IDT[0x21].offset_higherbits = (keyboard_address & 0xffff0000) >> 16;

	/*     	    Ports
	*	 PIC1	PIC2
	*Command 0x20   0xA0
	*Data    0x21   0xA1
	*/

	/* ICW1 - begin initialization */
	write_port(0x20 , 0x11);
	write_port(0xa0 , 0x11);

	/* ICW2 - remap offset address of IDT */
	/*
	* In x86 protected mode, we have to remap the PICs beyond 0x20 because
	* Intel have designated the first 32 interrupts as "reserved" for cpu exceptions
	*/
	write_port(0x21 , 0x20);
	write_port(0xa1 , 0x28);

	/* ICW3 - setup cascading */
	write_port(0x21 , 0x00);  
	write_port(0xa1 , 0x00);  

	/* ICW4 - environment info */
	write_port(0x21 , 0x01);
	write_port(0xa1 , 0x01);
	/* Initialization finished */

	/* mask interrupts */
	write_port(0x21 , 0xff);
	write_port(0xa1 , 0xff);

	/* fill the IDT descriptor */
	idt_address = (unsigned long)IDT ;
	idt_ptr[0] = (sizeof (struct IDT_entry) * IDT_SIZE) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16 ;

	load_idt(idt_ptr);
}

void kb_init() {

	/* 0xFD is 11111101 - enables only IRQ1 (keyboard)*/
	write_port(0x21 , 0xfd);
}

void kb_newline() {

	unsigned int line_size = 2 * COLUMNS;
	current_loc = current_loc + (line_size - current_loc % (line_size));
}

void clear_screen() {

	unsigned int i = 0;

	while(i < SCREENSIZE) {
		vidptr[i++] = ' ';
		vidptr[i++] = 0x0b;
	}
}

void keyboard_handler_main() {

	unsigned char status;
	char keycode;
	unsigned int i = 0, j = 0, x = 0, y = 0;

	/* write EOI */
	write_port(0x20, 0x20);
	status = read_port(KEYBOARD_STATUS_PORT);

	/* Lowest bit of status will be set if buffer is not empty */
	if(status & 0x01) {
		keycode = read_port(KEYBOARD_DATA_PORT);

		if(keycode < 0) {
		return;
		} else if(keycode == 0x0e) {
		vidptr[--current_loc] = 0x0b;
		vidptr[--current_loc] = ' ';
		} else if(keycode == 0x01) {
		clear_screen();
		while(str[x] != '\0') {

                	vidptr[y] = str[x];
                	vidptr[y + 1] = 0x0b;
                	++x;
                	y = y + 2;
                }
                current_loc = y;
                kb_newline();
                kb_newline();
		} else if(keycode == 0x1c) {
		kb_newline();
		} else if(keycode == 0xe048) {
                current_loc = current_loc - (LINES * 2);
		} else if(keycode == 0xe050) {
                current_loc = current_loc + (LINES * 2);
		} else if(keycode == 0xe04b) {
		current_loc = current_loc - 2;
		} else if(keycode == 0xe04d) {
                current_loc = current_loc + 2;
		} else if(keycode == 0x29) {
			if(j == 0) {
			clear_screen();
			display_mode(0x13);
			j++;
			} else if(j == 1) {
			clear_screen();
			display_mode(0x03);
        		while(str[x] != '\0') {
                
				vidptr[y] = str[x];
                		vidptr[y + 1] = 0x0b;
                		++x;
                		y = y + 2;
        		}
			current_loc = y;
        		kb_newline();
        		kb_newline();
			j--;
			}
		} else if(keycode == 0x0f) {
			while(i < 5) {
				vidptr[current_loc++] = ' ';
				vidptr[current_loc++] = 0x0b;
				i++;
			}
		} else {
		vidptr[current_loc++] = keyboard_map[(unsigned char)keycode];
		vidptr[current_loc++] = 0x0b;
		}

		csr_x = current_loc % COLUMNS;
		csr_y = current_loc / COLUMNS;

		scroll();
		move_csr();
	}
}

void kmain() {

	unsigned int x = 0, y = 0;

	enable_a20();
        clear_screen();

	while(str[x] != '\0') {

		vidptr[y] = str[x];
		vidptr[y + 1] = 0x0b;
		++x;
		y = y + 2;
	}

	current_loc = y;
	kb_newline();
	kb_newline();

        idt_init();
	kb_init();

        while(1);
}
