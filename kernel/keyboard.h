#ifndef KEYBOARD_H
#define KEYBOARD_H

// Le handler appelé par l'ISR pour traiter une interruption clavier
void keyboard_handler();

// Récupère un caractère depuis le buffer clavier (utilisé par le kernel)
char keyboard_getc(void);

#endif

