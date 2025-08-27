#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Initialise le clavier
void keyboard_init();

// Le handler appelé par l'ISR pour traiter une interruption clavier
void keyboard_interrupt_handler();

// Convertit un scancode en caractère ASCII.
char scancode_to_ascii(uint8_t scancode);

// Récupère un caractère depuis le buffer clavier (utilisé par le kernel)
char keyboard_getc(void);

// Lecture non-bloquante depuis le buffer ASCII du clavier
int kbd_get_nonblock(char *out);

#endif

