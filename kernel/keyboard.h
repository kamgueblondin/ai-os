#pragma once
#include <stdint.h>
#include <stddef.h>

void keyboard_init(void);
int kbd_pop_char(void); // Returns -1 if buffer is empty
void keyboard_interrupt_handler(void); // Hooked on IRQ1
