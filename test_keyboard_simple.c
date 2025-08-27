#include <stdint.h>

// Fonctions externes
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);
extern void print_string_serial(const char* str);
extern void write_serial(char c);

// Test simple du clavier
int main() {
    print_string_serial("=== Test Clavier Simple ===\n");
    
    // Tester la lecture directe du port clavier
    print_string_serial("Lecture directe du port 0x60...\n");
    
    for (int i = 0; i < 10; i++) {
        // Vérifier s'il y a des données disponibles
        uint8_t status = inb(0x64);
        if (status & 1) {
            uint8_t scancode = inb(0x60);
            print_string_serial("Scancode reçu: 0x");
            char hex[3] = "00";
            hex[0] = (scancode >> 4) < 10 ? '0' + (scancode >> 4) : 'A' + (scancode >> 4) - 10;
            hex[1] = (scancode & 0xF) < 10 ? '0' + (scancode & 0xF) : 'A' + (scancode & 0xF) - 10;
            print_string_serial(hex);
            print_string_serial("\n");
        } else {
            print_string_serial("Aucune donnée disponible\n");
        }
        
        // Attendre un peu
        for (volatile int j = 0; j < 1000000; j++);
    }
    
    print_string_serial("Test terminé\n");
    return 0;
}

