#include <stdint.h>

// Fonctions externes
extern void outb(unsigned short port, unsigned char data);
extern void print_string_serial(const char* str);
extern void keyboard_interrupt_handler();

// Test qui force une interruption clavier
void test_force_keyboard_interrupt() {
    print_string_serial("=== TEST FORCE INTERRUPTION CLAVIER ===\n");
    
    // Simuler une pression de touche en écrivant directement dans le port clavier
    // Scancode pour 'h' = 0x23
    outb(0x60, 0x23);
    
    // Appeler directement le handler pour tester
    print_string_serial("Appel direct du handler clavier...\n");
    keyboard_interrupt_handler();
    
    print_string_serial("=== FIN TEST ===\n");
}

