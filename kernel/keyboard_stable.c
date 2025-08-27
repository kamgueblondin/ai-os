#include "keyboard.h"
#include "kernel.h"
#include <stdint.h>

// Fonctions externes pour les ports I/O et autres
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char value);
extern void print_char_vga(char c, int x, int y, char color);
extern void write_serial(char c);
extern int vga_x, vga_y;

// Buffer clavier stable avec protection anti-spam
#define KBD_BUF_SIZE 256
static volatile unsigned char kbd_buf[KBD_BUF_SIZE];
static volatile unsigned int kbd_head = 0;
static volatile unsigned int kbd_tail = 0;

// Système de contrôle de qualité des caractères
static volatile int interrupt_count = 0;
static volatile int valid_chars_received = 0;
static volatile int spam_protection_counter = 0;
static volatile int polling_active = 0;
static volatile uint8_t last_scancode = 0;
static volatile int duplicate_protection = 0;

// Délais optimisés
void stable_delay() {
    for (volatile int i = 0; i < 500; i++);
}

void long_delay() {
    for (volatile int i = 0; i < 10000; i++);
}

// Attendre que le contrôleur soit prêt
int wait_controller_ready(int for_command) {
    int timeout = 5000;
    uint8_t status;
    
    while (timeout-- > 0) {
        status = inb(0x64);
        if (for_command) {
            if (!(status & 0x02)) return 1; // Prêt pour commande
        } else {
            if (status & 0x01) return 1;    // Données disponibles
        }
        stable_delay();
    }
    return 0;
}

// Buffer management avec protection anti-spam
void kbd_put_filtered(char c) {
    // Protection anti-spam: ignorer les caractères trop rapides
    spam_protection_counter++;
    if (spam_protection_counter > 1000) {
        spam_protection_counter = 0;
        return; // Skip ce caractère pour éviter le spam
    }
    
    // Protection anti-doublons
    static char last_char = 0;
    if (c == last_char && c != '\n' && c != ' ') {
        duplicate_protection++;
        if (duplicate_protection < 3) return; // Skip les doublons rapides
    } else {
        duplicate_protection = 0;
    }
    last_char = c;
    
    unsigned int next = (kbd_head + 1) & (KBD_BUF_SIZE - 1);
    if (next != kbd_tail) {
        kbd_buf[kbd_head] = c;
        kbd_head = next;
        valid_chars_received++;
    }
}

int kbd_get_stable(char *out) {
    if (kbd_head == kbd_tail) {
        return 0; // Vide
    }
    
    *out = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) & (KBD_BUF_SIZE - 1);
    return 1;
}

// Table scancode stable et vérifiée
const char stable_scancode_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

char stable_scancode_to_ascii(uint8_t scancode) {
    if (scancode >= 128) return 0;
    return stable_scancode_map[scancode];
}

// Polling contrôlé (beaucoup moins agressif)
void controlled_keyboard_poll() {
    static int poll_counter = 0;
    poll_counter++;
    
    // Polling très espacé si les interruptions marchent
    if (interrupt_count > 0) {
        if (poll_counter % 50000 != 0) return; // Très peu fréquent
    } else {
        if (poll_counter % 5000 != 0) return; // Modérément fréquent
    }
    
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) return; // Pas de données
    
    uint8_t scancode = inb(0x60);
    
    // Protection contre les répétitions de scancode
    if (scancode == last_scancode) {
        return; // Ignorer les répétitions immédiates
    }
    last_scancode = scancode;
    
    // Filtrer les codes de contrôle
    if (scancode == 0xFA || scancode == 0xFE || scancode == 0xAA) {
        return;
    }
    
    // Seulement les key presses (pas les releases)
    if (!(scancode & 0x80)) {
        char c = stable_scancode_to_ascii(scancode);
        if (c != 0) {
            polling_active = 1;
            kbd_put_filtered(c);
            
            // Debug limité
            if (valid_chars_received <= 3) {
                print_string_serial("POLL: '");
                write_serial(c);
                print_string_serial("'\n");
            }
        }
    }
}

// Handler d'interruption stable
void keyboard_interrupt_handler() {
    interrupt_count++;
    
    if (interrupt_count <= 2) {
        print_string_serial("KBD_IRQ: #");
        write_serial('0' + (interrupt_count % 10));
        print_string_serial("\n");
    }
    
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) return;
    
    uint8_t scancode = inb(0x60);
    
    // Protection contre les répétitions
    if (scancode == last_scancode) {
        return;
    }
    last_scancode = scancode;
    
    // Debug limité
    if (interrupt_count <= 5) {
        print_string_serial("IRQ_SCAN: 0x");
        char hex[] = "0123456789ABCDEF";
        write_serial(hex[scancode >> 4]);
        write_serial(hex[scancode & 0xF]);
        print_string_serial("\n");
    }
    
    // Filtrer les codes de contrôle
    if (scancode == 0xFA || scancode == 0xFE || scancode == 0xAA) {
        return;
    }
    
    // Seulement les key presses
    if (!(scancode & 0x80)) {
        char c = stable_scancode_to_ascii(scancode);
        if (c != 0) {
            kbd_put_filtered(c);
        }
    }
}

// Initialisation simplifiée et stable
void keyboard_init() {
    print_string_serial("=== KEYBOARD STABLE INIT ===\n");
    
    // Reset variables
    interrupt_count = 0;
    valid_chars_received = 0;
    spam_protection_counter = 0;
    polling_active = 0;
    kbd_head = 0;
    kbd_tail = 0;
    last_scancode = 0;
    duplicate_protection = 0;
    
    // Nettoyage initial simple
    int flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 20) {
        inb(0x60);
        flush_count++;
        stable_delay();
    }
    
    // Configuration minimale mais efficace
    print_string_serial("KBD: Configuration de base...\n");
    
    // Désactiver les ports temporairement
    if (wait_controller_ready(1)) {
        outb(0x64, 0xAD); // Disable port 1
        stable_delay();
    }
    
    // Nettoyer après désactivation
    flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 10) {
        inb(0x60);
        flush_count++;
        stable_delay();
    }
    
    // Configuration du contrôleur
    if (wait_controller_ready(1)) {
        outb(0x64, 0x20); // Read config
        stable_delay();
        
        if (wait_controller_ready(0)) {
            uint8_t config = inb(0x60);
            stable_delay();
            
            // Configuration stable
            config |= 0x01;  // Enable interrupt port 1
            config &= ~0x10; // Enable clock port 1
            config &= ~0x40; // No translation
            
            if (wait_controller_ready(1)) {
                outb(0x64, 0x60); // Write config
                stable_delay();
                
                if (wait_controller_ready(1)) {
                    outb(0x60, config);
                    stable_delay();
                }
            }
        }
    }
    
    // Test optionnel du contrôleur
    if (wait_controller_ready(1)) {
        outb(0x64, 0xAA); // Self-test
        stable_delay();
        
        if (wait_controller_ready(0)) {
            uint8_t result = inb(0x60);
            if (result == 0x55) {
                print_string_serial("KBD: Contrôleur OK\n");
            }
            stable_delay();
        }
    }
    
    // Réactiver le port
    if (wait_controller_ready(1)) {
        outb(0x64, 0xAE); // Enable port 1
        stable_delay();
    }
    
    // Configuration périphérique simple
    if (wait_controller_ready(1)) {
        outb(0x60, 0xF4); // Enable scanning
        stable_delay();
        
        // Attendre ACK éventuel
        int ack_wait = 1000;
        while (ack_wait-- > 0) {
            if (inb(0x64) & 1) {
                uint8_t response = inb(0x60);
                if (response == 0xFA) {
                    print_string_serial("KBD: Scanning activé\n");
                    break;
                }
            }
            stable_delay();
        }
    }
    
    // Pause de stabilisation
    long_delay();
    
    // Nettoyage final
    flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 5) {
        inb(0x60);
        flush_count++;
        stable_delay();
    }
    
    print_string_serial("=== KEYBOARD STABLE READY ===\n");
    print_string_serial("Mode: Interruption avec polling contrôlé\n");
    print_string_serial("Anti-spam: Activé\n");
    print_string_serial("============================\n");
}

// Fonction getchar stable avec timeout raisonnable
char keyboard_getc(void) {
    static int getc_calls = 0;
    char c;
    int attempts = 0;
    const int MAX_ATTEMPTS = 100000; // Timeout plus court
    
    getc_calls++;
    
    if (getc_calls <= 2) {
        print_string_serial("GETC: start\n");
    }
    
    // Réactiver les interruptions
    asm volatile("sti");
    
    while (attempts < MAX_ATTEMPTS) {
        // 1. Vérifier le buffer principal
        if (kbd_get_stable(&c)) {
            if (getc_calls <= 3) {
                print_string_serial("GETC: got '");
                write_serial(c);
                print_string_serial("'\n");
            }
            return c;
        }
        
        // 2. Polling contrôlé (très espacé)
        if (attempts % 1000 == 0) {
            controlled_keyboard_poll();
        }
        
        // 3. Re-vérifier après polling
        if (kbd_get_stable(&c)) {
            return c;
        }
        
        attempts++;
        
        // Debug espacé
        if (attempts % 25000 == 0 && getc_calls <= 2) {
            print_string_serial("GETC: wait...\n");
        }
    }
    
    // Timeout doux - retourner null sans bloquer
    if (getc_calls <= 2) {
        print_string_serial("GETC: timeout\n");
    }
    
    return 0;
}

// Fonctions utilitaires
char scancode_to_ascii(uint8_t scancode) {
    return stable_scancode_to_ascii(scancode);
}

// Diagnostic complet
void keyboard_diagnostic() {
    print_string_serial("=== DIAGNOSTIC CLAVIER STABLE ===\n");
    
    uint8_t status = inb(0x64);
    uint8_t pic_mask = inb(0x21);
    
    print_string_serial("Status: 0x");
    char hex[] = "0123456789ABCDEF";
    write_serial(hex[status >> 4]);
    write_serial(hex[status & 0xF]);
    print_string_serial(" | PIC: 0x");
    write_serial(hex[pic_mask >> 4]);
    write_serial(hex[pic_mask & 0xF]);
    print_string_serial("\n");
    
    print_string_serial("IRQ1: ");
    print_string_serial((pic_mask & 2) ? "MASKED" : "ACTIVE");
    print_string_serial("\n");
    
    print_string_serial("Interruptions: ");
    write_serial('0' + (interrupt_count % 10));
    print_string_serial(" | Chars: ");
    write_serial('0' + (valid_chars_received % 10));
    print_string_serial("\n");
    
    print_string_serial("Mode: ");
    if (interrupt_count > 0) {
        print_string_serial("INTERRUPTION");
        if (polling_active) print_string_serial(" + POLL");
    } else if (polling_active) {
        print_string_serial("POLLING");
    } else {
        print_string_serial("ATTENTE");
    }
    print_string_serial("\n");
    
    print_string_serial("===============================\n");
}
