#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Initialise le clavier
void keyboard_init();

// Le handler appelé par l'ISR pour traiter une interruption clavier
void keyboard_interrupt_handler();

// Convertit un scancode en caractère Unicode.
uint32_t scancode_to_unicode(uint8_t scancode);

// Récupère un caractère depuis le buffer clavier (utilisé par le kernel)
uint32_t keyboard_getc(void);

// Lecture non-bloquante depuis le buffer Unicode du clavier
int kbd_get_char_nonblock(uint32_t *out);

// Fonction pour ajouter un caractère au buffer
void kbd_put_char(uint32_t c);

// Helper function pour afficher un byte en hexadécimal
void print_hex_byte_serial(uint8_t byte);

#endif

