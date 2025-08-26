#pragma once
#include <stdint.h>

void serial_putc_direct(char c);
void serial_puts(const char *s);
void serial_puthex32(uint32_t v);
