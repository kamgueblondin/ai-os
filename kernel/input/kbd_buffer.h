#ifndef KBD_BUFFER_H
#define KBD_BUFFER_H

#include <stdint.h>

void kbd_push_scancode(uint8_t s);
int kbd_pop_scancode(uint8_t *out);

#endif // KBD_BUFFER_H
