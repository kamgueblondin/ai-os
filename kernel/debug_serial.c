#include "debug_serial.h"
#include "io.h"
#include <stdint.h>

#define COM1_PORT 0x3F8

static inline int serial_is_transmit_empty(void){
    return inb(COM1_PORT + 5) & 0x20;
}

void serial_putc_direct(char c){
    volatile unsigned int t = 0x10000;
    while(t-- && !serial_is_transmit_empty()) asm volatile("pause");
    outb(COM1_PORT, (uint8_t)c);
}

void serial_puts(const char *s){
    while(*s) serial_putc_direct(*s++);
}

void serial_puthex32(uint32_t v){
    const char *hex = "0123456789ABCDEF";
    for(int s = 28; s >= 0; s -= 4) serial_putc_direct(hex[(v >> s) & 0xF]);
}
