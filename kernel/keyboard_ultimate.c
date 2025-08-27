#include "keyboard.h"
#include "kernel.h"
#include <stdint.h>

// Fonctions externes pour les ports I/O et autres
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char value);
extern void print_char_vga(char c, int x, int y, char color);
extern void write_serial(char c);
extern int vga_x, vga_y;

// Buffer clavier hybride (interruption + polling)
#define KBD_BUF_SIZE 256
static volatile unsigned char kbd_buf[KBD_BUF_SIZE];
static volatile unsigned int kbd_head = 0;
static volatile unsigned int kbd_tail = 0;

// Système de fallback hybride
static volatile int interrupt_mode_active = 1;
static volatile int polling_fallback_active = 0;
static volatile int debug_interrupt_count = 0;
static volatile int debug_polling_count = 0;
static volatile uint32_t last_poll_time = 0;

// Variables de diagnostic
static volatile int initialization_phase = 0;

// Délai optimisé pour QEMU
void qemu_delay() {
    for (volatile int i = 0; i < 1000; i++);
}

// Délai plus long pour les opérations critiques
void qemu_long_delay() {
    for (volatile int i = 0; i < 50000; i++);
}

// Attendre que le contrôleur soit prêt (version optimisée QEMU)
int wait_kbd_ready(int is_command) {
    int timeout = 10000; // Timeout plus court pour QEMU
    uint8_t status;
    
    while (timeout-- > 0) {
        status = inb(0x64);
        if (is_command) {
            if (!(status & 0x02)) return 1; // Prêt pour commande
        } else {
            if (status & 0x01) return 1;    // Données disponibles
        }
        qemu_delay();
    }
    
    return 0; // Timeout
}

// Buffer management amélioré
void kbd_put_char(char c) {
    unsigned int next = (kbd_head + 1) & (KBD_BUF_SIZE - 1);
    if (next != kbd_tail) {
        kbd_buf[kbd_head] = c;
        kbd_head = next;
        
        // Debug limité
        if (debug_interrupt_count + debug_polling_count <= 5) {
            print_string_serial("KBD_PUT: '");
            write_serial(c);
            print_string_serial("'\n");
        }
    }
}

int kbd_get_char_nonblock(char *out) {
    if (kbd_head == kbd_tail) {
        return 0; // Vide
    }
    
    *out = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) & (KBD_BUF_SIZE - 1);
    return 1; // Succès
}

// Table scancode optimisée pour QEMU
const char qemu_scancode_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Convertir scancode en ASCII pour QEMU
char qemu_scancode_to_ascii(uint8_t scancode) {
    if (scancode >= 128) return 0;
    return qemu_scancode_map[scancode];
}

// Mécanisme de polling de secours
void keyboard_poll_check() {
    static uint32_t poll_counter = 0;
    poll_counter++;
    
    // Ne faire le polling qu'occasionnellement si les interruptions fonctionnent
    if (interrupt_mode_active && debug_interrupt_count > 0) {
        if (poll_counter % 10000 != 0) return;
    }
    
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) return; // Pas de données
    
    uint8_t scancode = inb(0x60);
    debug_polling_count++;
    
    // Si on reçoit des données par polling, basculer en mode hybride
    if (!polling_fallback_active && debug_interrupt_count == 0) {
        polling_fallback_active = 1;
        print_string_serial("KBD_POLL: Mode polling activé\n");
    }
    
    // Traiter le scancode
    if (scancode != 0xFA && scancode != 0xFE && scancode != 0xAA) {
        if (!(scancode & 0x80)) { // Key press seulement
            char c = qemu_scancode_to_ascii(scancode);
            if (c) {
                kbd_put_char(c);
                
                if (debug_polling_count <= 5) {
                    print_string_serial("KBD_POLL: '");
                    write_serial(c);
                    print_string_serial("' (scan=0x");
                    char hex[] = "0123456789ABCDEF";
                    write_serial(hex[scancode >> 4]);
                    write_serial(hex[scancode & 0xF]);
                    print_string_serial(")\n");
                }
            }
        }
    }
}

// Handler d'interruption clavier (nouvelle version)
void keyboard_interrupt_handler() {
    debug_interrupt_count++;
    
    if (debug_interrupt_count <= 3) {
        print_string_serial("KBD_IRQ: handler #");
        write_serial('0' + (debug_interrupt_count % 10));
        print_string_serial("\n");
    }
    
    // Marquer que le mode interruption fonctionne
    if (debug_interrupt_count == 1) {
        print_string_serial("KBD_IRQ: Mode interruption confirmé\n");
    }
    
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) return;
    
    uint8_t scancode = inb(0x60);
    
    // Debug scancode
    if (debug_interrupt_count <= 5) {
        print_string_serial("KBD_IRQ: scan=0x");
        char hex[] = "0123456789ABCDEF";
        write_serial(hex[scancode >> 4]);
        write_serial(hex[scancode & 0xF]);
        print_string_serial("\n");
    }
    
    // Traiter seulement les vrais scancodes
    if (scancode != 0xFA && scancode != 0xFE && scancode != 0xAA) {
        if (!(scancode & 0x80)) { // Key press
            char c = qemu_scancode_to_ascii(scancode);
            if (c) {
                kbd_put_char(c);
            }
        }
    }
}

// Initialisation triple : Standard PS/2 + QEMU + Fallback
void keyboard_init() {
    print_string_serial("=== KEYBOARD INIT ULTIMATE ===\n");
    initialization_phase = 1;
    
    // Reset variables
    debug_interrupt_count = 0;
    debug_polling_count = 0;
    interrupt_mode_active = 1;
    polling_fallback_active = 0;
    kbd_head = 0;
    kbd_tail = 0;
    
    // Phase 1: Nettoyage initial
    print_string_serial("Phase 1: Nettoyage complet...\n");
    int flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 100) {
        inb(0x60);
        flush_count++;
        qemu_delay();
    }
    
    // Phase 2: Configuration minimale pour QEMU
    print_string_serial("Phase 2: Configuration QEMU...\n");
    
    // Désactivation temporaire des ports
    if (wait_kbd_ready(1)) {
        outb(0x64, 0xAD); // Disable port 1
        qemu_delay();
    }
    
    if (wait_kbd_ready(1)) {
        outb(0x64, 0xA7); // Disable port 2  
        qemu_delay();
    }
    
    // Flush après désactivation
    flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 50) {
        inb(0x60);
        flush_count++;
        qemu_delay();
    }
    
    // Configuration du contrôleur
    if (wait_kbd_ready(1)) {
        outb(0x64, 0x20); // Read configuration
        qemu_delay();
        
        if (wait_kbd_ready(0)) {
            uint8_t config = inb(0x60);
            qemu_delay();
            
            // Configuration optimale pour QEMU
            config |= 0x01;  // Enable port 1 interrupt
            config &= ~0x10; // Enable port 1 clock  
            config &= ~0x40; // Disable scancode translation
            
            if (wait_kbd_ready(1)) {
                outb(0x64, 0x60); // Write configuration
                qemu_delay();
                
                if (wait_kbd_ready(1)) {
                    outb(0x60, config);
                    qemu_delay();
                    print_string_serial("Phase 2: Configuration appliquée\n");
                }
            }
        }
    }
    
    // Test rapide du contrôleur (optionnel pour QEMU)
    if (wait_kbd_ready(1)) {
        outb(0x64, 0xAA); // Self-test
        qemu_delay();
        
        if (wait_kbd_ready(0)) {
            uint8_t result = inb(0x60);
            if (result == 0x55) {
                print_string_serial("Phase 2: Contrôleur OK\n");
            }
        }
    }
    
    // Réactivation du port 1
    if (wait_kbd_ready(1)) {
        outb(0x64, 0xAE); // Enable port 1
        qemu_delay();
        print_string_serial("Phase 2: Port 1 réactivé\n");
    }
    
    // Phase 3: Configuration du périphérique (simple pour QEMU)
    print_string_serial("Phase 3: Configuration périphérique...\n");
    
    // Reset optionnel (peut ne pas marcher dans QEMU)
    if (wait_kbd_ready(1)) {
        outb(0x60, 0xFF); // Reset device
        qemu_long_delay();
        
        // Vider les réponses possibles
        for (int i = 0; i < 10; i++) {
            if (inb(0x64) & 1) {
                uint8_t response = inb(0x60);
                if (response == 0xAA) {
                    print_string_serial("Phase 3: Reset réussi\n");
                    break;
                }
                qemu_delay();
            }
        }
    }
    
    // Configuration scancode set 1 (par défaut généralement)
    if (wait_kbd_ready(1)) {
        outb(0x60, 0xF0); // Scancode set command
        qemu_delay();
        
        // Attendre ACK potentiel
        for (int i = 0; i < 100; i++) {
            if (inb(0x64) & 1) {
                uint8_t ack = inb(0x60);
                if (ack == 0xFA) break;
            }
            qemu_delay();
        }
        
        if (wait_kbd_ready(1)) {
            outb(0x60, 0x01); // Set 1
            qemu_delay();
        }
    }
    
    // Activation du scanning
    if (wait_kbd_ready(1)) {
        outb(0x60, 0xF4); // Enable scanning
        qemu_delay();
        print_string_serial("Phase 3: Scanning activé\n");
    }
    
    // Phase 4: Stabilisation et test
    print_string_serial("Phase 4: Finalisation...\n");
    qemu_long_delay(); // Pause de stabilisation
    
    // Nettoyage final
    flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 20) {
        inb(0x60);
        flush_count++;
        qemu_delay();
    }
    
    initialization_phase = 0;
    print_string_serial("=== KEYBOARD INIT COMPLETE ===\n");
    print_string_serial("Mode: Interruption + Polling Fallback\n");
    print_string_serial("Ready for input!\n");
    print_string_serial("===============================\n");
}

// Fonction getchar hybride avec plusieurs mécanismes
char keyboard_getc(void) {
    static int getc_calls = 0;
    char c;
    int attempts = 0;
    const int MAX_ATTEMPTS = 200000; // Timeout raisonnable
    
    getc_calls++;
    
    // Debug limité
    if (getc_calls <= 3) {
        print_string_serial("GETC: start (call #");
        write_serial('0' + (getc_calls % 10));
        print_string_serial(")\n");
    }
    
    // Réactiver les interruptions
    asm volatile("sti");
    
    while (attempts < MAX_ATTEMPTS) {
        // 1. Essayer d'abord le buffer d'interruptions
        if (kbd_get_char_nonblock(&c)) {
            if (getc_calls <= 3) {
                print_string_serial("GETC: got '");
                write_serial(c);
                print_string_serial("' from buffer\n");
            }
            return c;
        }
        
        // 2. Polling de secours (actif même avec interruptions)
        keyboard_poll_check();
        
        // 3. Vérifier à nouveau le buffer
        if (kbd_get_char_nonblock(&c)) {
            if (getc_calls <= 3) {
                print_string_serial("GETC: got '");
                write_serial(c);
                print_string_serial("' from polling\n");
            }
            return c;
        }
        
        attempts++;
        
        // Debug périodique
        if (attempts % 50000 == 0 && getc_calls <= 2) {
            print_string_serial("GETC: waiting... (");
            write_serial('0' + (attempts / 50000));
            print_string_serial(")\n");
        }
    }
    
    // Timeout
    if (getc_calls <= 2) {
        print_string_serial("GETC: timeout\n");
    }
    
    return 0; // Caractère null en cas de timeout
}

// Fonctions utilitaires exposées
char scancode_to_ascii(uint8_t scancode) {
    return qemu_scancode_to_ascii(scancode);
}

// Diagnostic système complet
void keyboard_diagnostic() {
    print_string_serial("=== DIAGNOSTIC CLAVIER ULTIMATE ===\n");
    
    uint8_t status = inb(0x64);
    print_string_serial("Status: 0x");
    char hex[] = "0123456789ABCDEF";
    write_serial(hex[status >> 4]);
    write_serial(hex[status & 0xF]);
    
    uint8_t pic_mask = inb(0x21);
    print_string_serial(" | PIC: 0x");
    write_serial(hex[pic_mask >> 4]);
    write_serial(hex[pic_mask & 0xF]);
    print_string_serial("\n");
    
    print_string_serial("IRQ1: ");
    print_string_serial((pic_mask & 2) ? "MASKED" : "ACTIVE");
    print_string_serial("\n");
    
    print_string_serial("Mode: ");
    if (debug_interrupt_count > 0) {
        print_string_serial("INTERRUPTION");
    } else if (polling_fallback_active) {
        print_string_serial("POLLING");
    } else {
        print_string_serial("WAIT_FOR_INPUT");
    }
    print_string_serial("\n");
    
    print_string_serial("Stats: IRQ=");
    write_serial('0' + (debug_interrupt_count % 10));
    print_string_serial(" POLL=");
    write_serial('0' + (debug_polling_count % 10));
    print_string_serial("\n");
    
    print_string_serial("===============================\n");
}
