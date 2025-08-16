#include "keyboard.h"
#include <stdint.h>

// Déclaration de fonctions définies ailleurs
extern void print_char_vga(char c, int x, int y, char color); // Depuis kernel.c
extern void write_serial(char c); // Depuis kernel.c
extern int vga_x, vga_y;
extern unsigned char inb(unsigned short port);
extern void pic_send_eoi(unsigned char irq);
extern void syscall_add_input_char(char c); // Depuis syscall.c

// Table de correspondance simple Scancode -> ASCII (pour un clavier US QWERTY)
const char scancode_map[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5',
    '6', '+', '1', '2', '3', '0', '.'
};

// Le handler appelé par l'ISR
void keyboard_handler() {
    unsigned char scancode = inb(0x60); // Lit le scancode depuis le port du clavier

    // On ne gère que les pressions de touche (pas les relâchements pour l'instant)
    if (scancode < sizeof(scancode_map) && scancode_map[scancode] != 0) {
        char c = scancode_map[scancode];
        
        // NOUVEAU: Ajouter le caractère au buffer des syscalls pour SYS_GETS
        syscall_add_input_char(c);
        
        // Note: L'affichage est maintenant géré par syscall_add_input_char
        // pour une meilleure cohérence avec le shell interactif
        
        // Affiche aussi sur le port série pour le debug
        write_serial(c);
    }
    
    // Envoie EOI au PIC
    pic_send_eoi(1);
}

