#include "keyboard.h"
#include <stdint.h>

// Déclaration de fonctions définies ailleurs
extern void print_char_vga(char c, int x, int y, char color); // Depuis kernel.c
extern void write_serial(char c); // Depuis kernel.c
extern int vga_x, vga_y;
extern unsigned char inb(unsigned short port);
extern void pic_send_eoi(unsigned char irq);

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
        
        // Affiche le caractère à la position actuelle du curseur sur VGA
        print_char_vga(c, vga_x, vga_y, 0x1F);
        
        // Affiche aussi sur le port série pour le debug
        write_serial(c);
        
        // Gestion du curseur
        if (c == '\n') {
            vga_x = 0;
            vga_y++;
        } else if (c == '\b') {
            if (vga_x > 0) {
                vga_x--;
                print_char_vga(' ', vga_x, vga_y, 0x1F); // Efface le caractère
            }
        } else {
            vga_x++;
            if (vga_x >= 80) {
                vga_x = 0;
                vga_y++;
            }
        }
        
        // Gestion du défilement si on dépasse l'écran
        if (vga_y >= 25) {
            vga_y = 24;
        }
    }
    
    // Envoie EOI au PIC
    pic_send_eoi(1);
}

