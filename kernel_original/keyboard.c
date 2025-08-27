#include "keyboard.h"
#include <stdint.h>

// Déclaration de fonctions définies ailleurs
extern void print_char_vga(char c, int x, int y, char color); // Depuis kernel.c
extern void write_serial(char c); // Depuis kernel.c
extern int vga_x, vga_y;
extern unsigned char inb(unsigned short port);
extern void pic_send_eoi(unsigned char irq);
extern void syscall_add_input_char(char c); // Depuis syscall.c

// Table de correspondance complète Scancode -> ASCII (pour un clavier US QWERTY)
// Index = scancode, valeur = caractère ASCII correspondant
const char scancode_map[128] = {
    // 0x00-0x0F
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    // 0x10-0x1F (QWERTY première ligne)
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
    // 0x20-0x2F (ASDF ligne du milieu)
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    // 0x30-0x3F (ZXCV ligne du bas)
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    // 0x40-0x4F (Touches fonction F1-F10)
    0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
    // 0x50-0x5F (Pavé numérique suite)
    '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x60-0x6F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // 0x70-0x7F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Le handler appelé par l'ISR
void keyboard_handler() {
    unsigned char scancode = inb(0x60); // Lit le scancode depuis le port du clavier
    
    // DEBUG: Log tous les scancodes reçus
    extern void print_string_serial(const char* str);
    print_string_serial("KBD: scancode=0x");
    
    // Convertir scancode en hex pour debug
    char hex[4] = "00\n";
    hex[0] = (scancode >> 4) < 10 ? '0' + (scancode >> 4) : 'A' + (scancode >> 4) - 10;
    hex[1] = (scancode & 0xF) < 10 ? '0' + (scancode & 0xF) : 'A' + (scancode & 0xF) - 10;
    print_string_serial(hex);

    // Ignorer les codes de relâchement (bit 7 = 1)
    if (scancode & 0x80) {
        print_string_serial("KBD: key release ignored\n");
        pic_send_eoi(1);
        return;
    }

    // Vérifier que le scancode est dans la plage valide
    if (scancode >= 128) {
        print_string_serial("KBD: scancode out of range\n");
        pic_send_eoi(1);
        return;
    }

    // Obtenir le caractère correspondant
    char c = scancode_map[scancode];
    
    if (c != 0) {
        print_string_serial("KBD: char='");
        write_serial(c);
        print_string_serial("'\n");
        
        // Ajouter le caractère au buffer des syscalls
        syscall_add_input_char(c);
    } else {
        print_string_serial("KBD: no mapping for this scancode\n");
    }
    
    // Envoie EOI au PIC
    pic_send_eoi(1);
}

