//==============================================================
// Version simplifiée du driver clavier pour debug
// Cette version remplace temporairement les fonctions complexes
// par des versions minimales pour identifier le problème
//==============================================================

#include <stdint.h>

// Fonctions externes
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char value);
extern void print_string_serial(const char* str);
extern void print_hex_serial(uint32_t num);
extern void write_serial(char c);

// Compteur simple pour les interruptions
volatile int simple_interrupt_count = 0;
volatile char last_scancode = 0;

// Version ultra-simple du handler d'interruption clavier
void simple_keyboard_interrupt_handler() {
    // Incrémenter le compteur
    simple_interrupt_count++;
    
    // Lire le scancode
    uint8_t scancode = inb(0x60);
    last_scancode = scancode;
    
    // Debug immédiat
    print_string_serial("KBD_SIMPLE: INT#");
    print_hex_serial(simple_interrupt_count);
    print_string_serial(" SC=0x");
    print_hex_serial(scancode);
    print_string_serial("\n");
    
    // Envoyer EOI immédiatement
    outb(0x20, 0x20);
}

// Version simplifiée de keyboard_getc qui utilise polling directement
char simple_keyboard_getc() {
    print_string_serial("SIMPLE_GETC: En attente d'une touche...\n");
    
    // Vérifier d'abord si on a eu des interruptions
    if (simple_interrupt_count > 0) {
        print_string_serial("SIMPLE_GETC: Interruptions détectées (");
        print_hex_serial(simple_interrupt_count);
        print_string_serial(")\n");
    }
    
    int timeout = 500000;  // Timeout plus long
    while (timeout-- > 0) {
        // Méthode 1: Vérifier le buffer d'interruption
        if (simple_interrupt_count > 0) {
            print_string_serial("SIMPLE_GETC: Utilisation du scancode d'interruption\n");
            
            // Convertir le scancode en caractère simple
            if (last_scancode == 0x1C) return '\n';  // ENTER
            if (last_scancode == 0x39) return ' ';   // SPACE  
            if (last_scancode == 0x1E) return 'a';   // A
            if (last_scancode == 0x48) return 'h';   // H
            if (last_scancode == 0x23) return 'h';   // H (autre code)
            
            // Réinitialiser le compteur
            simple_interrupt_count = 0;
            return '?';  // Caractère par défaut
        }
        
        // Méthode 2: Polling direct (backup)
        if ((inb(0x64) & 1)) {  // Data available
            uint8_t scancode = inb(0x60);
            print_string_serial("SIMPLE_GETC: Polling direct, scancode=0x");
            print_hex_serial(scancode);
            print_string_serial("\n");
            
            // Convertir scancode simple
            if (scancode == 0x1C) return '\n';  // ENTER
            if (scancode == 0x39) return ' ';   // SPACE  
            if (scancode == 0x1E) return 'a';   // A
            if (scancode & 0x80) continue;      // Ignorer key releases
            
            return '?';
        }
        
        // Petite pause
        for (volatile int i = 0; i < 10; i++);
    }
    
    print_string_serial("SIMPLE_GETC: Timeout - pas de saisie détectée\n");
    return '\n';  // Timeout
}

// Test complet du système simplifié
void test_simple_keyboard_system() {
    print_string_serial("=== TEST SYSTEME CLAVIER SIMPLIFIE ===\n");
    
    // Reset des compteurs
    simple_interrupt_count = 0;
    last_scancode = 0;
    
    // Vérifications initiales
    uint8_t pic_mask = inb(0x21);
    print_string_serial("PIC mask: 0x");
    print_hex_serial(pic_mask);
    print_string_serial(" (IRQ1 ");
    print_string_serial((pic_mask & 2) ? "MASQUEE)" : "ACTIVE)");
    print_string_serial("\n");
    
    uint8_t kbd_status = inb(0x64);
    print_string_serial("Keyboard status: 0x");
    print_hex_serial(kbd_status);
    print_string_serial("\n");
    
    print_string_serial("\nTest actif - Appuyez sur une touche (ENTER pour terminer):\n");
    
    // Boucle de test
    for (int i = 0; i < 5; i++) {
        print_string_serial("\n[Test ");
        print_hex_serial(i + 1);
        print_string_serial("/5] ");
        
        char c = simple_keyboard_getc();
        
        print_string_serial("Reçu: '");
        write_serial(c);
        print_string_serial("'\n");
        
        if (c == '\n') {
            print_string_serial("ENTER détecté - fin du test\n");
            break;
        }
    }
    
    print_string_serial("\n=== RESUME DU TEST ===\n");
    print_string_serial("Total interruptions: ");
    print_hex_serial(simple_interrupt_count);
    print_string_serial("\n");
    print_string_serial("Dernier scancode: 0x");
    print_hex_serial(last_scancode);
    print_string_serial("\n");
    print_string_serial("=======================\n");
}
