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
    
    // DEBUG: Log tous les scancodes reçus
    extern void print_string_serial(const char* str);
    print_string_serial("KBD: scancode=");
    
    // Convertir scancode en hex pour debug
    char hex[4] = "00\n";
    hex[0] = (scancode >> 4) < 10 ? '0' + (scancode >> 4) : 'A' + (scancode >> 4) - 10;
    hex[1] = (scancode & 0xF) < 10 ? '0' + (scancode & 0xF) : 'A' + (scancode & 0xF) - 10;
    print_string_serial(hex);

    // On ne gère que les pressions de touche (pas les relâchements pour l'instant)
    if (scancode < sizeof(scancode_map) && scancode_map[scancode] != 0) {
        char c = scancode_map[scancode];
        
        print_string_serial("KBD: char=");
        write_serial(c);
        print_string_serial("\n");
        
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

