//==============================================================
// Test de debug des interruptions clavier
// Ce programme force des interruptions et vérifie leur réception
//==============================================================

#include <stdint.h>

// Fonctions externes
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char value);
extern void print_string_serial(const char* str);
extern void print_hex_serial(uint32_t num);

// Variable globale pour compter les interruptions reçues
volatile int interrupt_count = 0;

// Handler de test qui incrémente un compteur
void test_keyboard_interrupt_handler() {
    interrupt_count++;
    
    // Lire le scancode depuis le port
    uint8_t scancode = inb(0x60);
    
    print_string_serial("INT_TEST: Interruption reçue #");
    print_hex_serial(interrupt_count);
    print_string_serial(", scancode=0x");
    print_hex_serial(scancode);
    print_string_serial("\n");
    
    // Envoyer EOI au PIC
    outb(0x20, 0x20);
}

// Test d'émulation d'interruption clavier
void test_keyboard_interrupts() {
    print_string_serial("=== TEST DEBUG INTERRUPTIONS CLAVIER ===\n");
    
    // Initialisation des variables
    interrupt_count = 0;
    
    // Vérifier l'état du PIC
    uint8_t pic_mask = inb(0x21);
    print_string_serial("PIC1 mask: 0x");
    print_hex_serial(pic_mask);
    print_string_serial("\n");
    print_string_serial("IRQ1 status: ");
    print_string_serial((pic_mask & 2) ? "MASQUEE" : "ACTIVE");
    print_string_serial("\n");
    
    // Vérifier le statut du contrôleur clavier
    uint8_t kbd_status = inb(0x64);
    print_string_serial("Keyboard controller status: 0x");
    print_hex_serial(kbd_status);
    print_string_serial("\n");
    
    // Test 1: Forcer la lecture du port clavier
    print_string_serial("\nTest 1: Lecture directe du port clavier...\n");
    if (kbd_status & 1) {  // Data available
        uint8_t data = inb(0x60);
        print_string_serial("Données disponibles: 0x");
        print_hex_serial(data);
        print_string_serial("\n");
    } else {
        print_string_serial("Aucune donnée disponible dans le buffer clavier\n");
    }
    
    // Test 2: Attendre et surveiller les interruptions
    print_string_serial("\nTest 2: Surveillance des interruptions (10 secondes)...\n");
    print_string_serial("Appuyez sur des touches maintenant !\n");
    
    int timeout = 1000000;  // ~10 secondes
    int last_count = interrupt_count;
    
    while (timeout-- > 0) {
        // Vérifier si on a reçu des interruptions
        if (interrupt_count != last_count) {
            print_string_serial("Nouvelle interruption détectée ! Total: ");
            print_hex_serial(interrupt_count);
            print_string_serial("\n");
            last_count = interrupt_count;
        }
        
        // Petite pause
        for (volatile int i = 0; i < 100; i++);
    }
    
    print_string_serial("\nRésultat du test:\n");
    print_string_serial("Total interruptions reçues: ");
    print_hex_serial(interrupt_count);
    print_string_serial("\n");
    
    if (interrupt_count > 0) {
        print_string_serial("✅ Les interruptions clavier fonctionnent !\n");
    } else {
        print_string_serial("❌ PROBLEME: Aucune interruption clavier reçue\n");
        
        // Diagnostics supplémentaires
        print_string_serial("\nDiagnostics supplémentaires:\n");
        
        // Vérifier l'état final du PIC
        pic_mask = inb(0x21);
        print_string_serial("PIC1 mask final: 0x");
        print_hex_serial(pic_mask);
        print_string_serial("\n");
        
        // Vérifier l'état final du contrôleur clavier  
        kbd_status = inb(0x64);
        print_string_serial("Keyboard status final: 0x");
        print_hex_serial(kbd_status);
        print_string_serial("\n");
        
        // Test de génération d'interruption artificielle
        print_string_serial("Test génération IRQ1 artificielle...\n");
        asm volatile("int $0x21");  // IRQ1 = interruption 33 = 0x21
        print_string_serial("Interruption artificielle générée\n");
    }
    
    print_string_serial("==========================================\n");
}

// Point d'entrée principal du test
void run_interrupt_test() {
    test_keyboard_interrupts();
}
