#include "keyboard.h"
#include "kernel.h"
#include <stdint.h>

// Fonctions externes pour les ports I/O et autres
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char value);
extern void print_char_vga(char c, int x, int y, char color);
extern void write_serial(char c);
extern int vga_x, vga_y;

// Buffer clavier simple et efficace
#define KBD_BUF_SIZE 256
static volatile char kbd_buffer[KBD_BUF_SIZE];
static volatile unsigned int kbd_head = 0;
static volatile unsigned int kbd_tail = 0;

// Statistiques simples
static volatile int interrupt_count = 0;
static volatile int chars_processed = 0;

// Délai basique
void kbd_delay() {
    for (volatile int i = 0; i < 1000; i++);
}

// Attendre que le contrôleur soit prêt
int wait_keyboard_ready(int for_command) {
    int timeout = 10000;
    uint8_t status;
    
    while (timeout-- > 0) {
        status = inb(0x64);
        if (for_command) {
            if (!(status & 0x02)) return 1; // Input buffer empty, ready for command
        } else {
            if (status & 0x01) return 1;    // Output buffer full, data available
        }
        kbd_delay();
    }
    return 0;
}

// Table de conversion scancode simple et vérifiée
static const char scancode_to_char[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Buffer management simple
void kbd_put_char(char c) {
    if (c == 0) return; // Ignorer les caractères nuls
    
    unsigned int next_head = (kbd_head + 1) & (KBD_BUF_SIZE - 1);
    if (next_head != kbd_tail) {
        kbd_buffer[kbd_head] = c;
        kbd_head = next_head;
        chars_processed++;
    }
}

int kbd_get_char(char *c) {
    if (kbd_head == kbd_tail) {
        return 0; // Buffer vide
    }
    
    *c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) & (KBD_BUF_SIZE - 1);
    return 1;
}

// Handler d'interruption simple et efficace
void keyboard_interrupt_handler() {
    interrupt_count++;
    
    // Lire le status du contrôleur
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) {
        return; // Pas de données disponibles
    }
    
    // Lire le scancode
    uint8_t scancode = inb(0x60);
    
    // Debug limité (seulement les 3 premières)
    if (interrupt_count <= 3) {
        print_string_serial("KBD_IRQ: scancode=0x");
        char hex[] = "0123456789ABCDEF";
        write_serial(hex[(scancode >> 4) & 0xF]);
        write_serial(hex[scancode & 0xF]);
        print_string_serial("\n");
    }
    
    // Filtrer les codes de contrôle et les ACK
    if (scancode == 0xFA || scancode == 0xFE || scancode == 0xAA || scancode == 0xFC) {
        return; // Ignorer les codes de contrôle
    }
    
    // Seulement traiter les key press (pas les releases)
    if (!(scancode & 0x80)) {
        char c = scancode_to_char[scancode & 0x7F];
        if (c != 0) {
            kbd_put_char(c);
            
            // Debug limité
            if (chars_processed <= 5) {
                print_string_serial("KBD: char='");
                write_serial(c);
                print_string_serial("'\n");
            }
        }
    }
}

// Initialisation clavier simplifiée
void keyboard_init() {
    print_string_serial("=== KEYBOARD CLEAN INIT ===\n");
    
    // Reset des variables
    interrupt_count = 0;
    chars_processed = 0;
    kbd_head = 0;
    kbd_tail = 0;
    
    // Nettoyer le buffer du contrôleur
    int flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 16) {
        inb(0x60); // Vider les données en attente
        flush_count++;
        kbd_delay();
    }
    print_string_serial("KBD: Buffer nettoyé\n");
    
    // Configuration basique du contrôleur
    // Désactiver les ports temporairement
    if (wait_keyboard_ready(1)) {
        outb(0x64, 0xAD); // Désactiver port 1
        kbd_delay();
    }
    
    if (wait_keyboard_ready(1)) {
        outb(0x64, 0xA7); // Désactiver port 2 (souris)
        kbd_delay();
    }
    
    // Vider encore après désactivation
    flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 8) {
        inb(0x60);
        flush_count++;
        kbd_delay();
    }
    
    // Lire et modifier la configuration
    if (wait_keyboard_ready(1)) {
        outb(0x64, 0x20); // Lire configuration byte
        kbd_delay();
        
        if (wait_keyboard_ready(0)) {
            uint8_t config = inb(0x60);
            kbd_delay();
            
            // Configuration simple : activer interruption port 1, désactiver port 2
            config |= 0x01;  // Enable interrupt port 1
            config &= ~0x02; // Disable interrupt port 2
            config &= ~0x10; // Enable clock port 1
            config |= 0x20;  // Disable clock port 2
            
            if (wait_keyboard_ready(1)) {
                outb(0x64, 0x60); // Écrire configuration
                kbd_delay();
                
                if (wait_keyboard_ready(1)) {
                    outb(0x60, config);
                    kbd_delay();
                }
            }
        }
    }
    
    // Réactiver le port clavier
    if (wait_keyboard_ready(1)) {
        outb(0x64, 0xAE); // Activer port 1
        kbd_delay();
    }
    
    // Activer le scanning du clavier
    if (wait_keyboard_ready(1)) {
        outb(0x60, 0xF4); // Enable scanning
        kbd_delay();
        
        // Attendre l'ACK
        int ack_wait = 5000;
        while (ack_wait-- > 0) {
            if (inb(0x64) & 1) {
                uint8_t response = inb(0x60);
                if (response == 0xFA) {
                    print_string_serial("KBD: Scanning activé (ACK reçu)\n");
                    break;
                }
            }
            kbd_delay();
        }
    }
    
    // Nettoyage final
    flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 4) {
        inb(0x60);
        flush_count++;
        kbd_delay();
    }
    
    print_string_serial("=== KEYBOARD READY (INTERRUPT-ONLY) ===\n");
}

// Fonction getchar simple - SANS POLLING
char keyboard_getc(void) {
    static int getc_call_count = 0;
    char c;
    int wait_cycles = 0;
    const int MAX_WAIT = 200000; // Timeout raisonnable
    
    getc_call_count++;
    
    if (getc_call_count <= 2) {
        print_string_serial("GETC_START: waiting for input\n");
    }
    
    // Assurer que les interruptions sont activées
    asm volatile("sti");
    
    // Attente simple basée sur les interruptions UNIQUEMENT
    while (wait_cycles < MAX_WAIT) {
        // Vérifier le buffer
        if (kbd_get_char(&c)) {
            if (getc_call_count <= 3) {
                print_string_serial("GETC_SUCCESS: got '");
                write_serial(c);
                print_string_serial("'\n");
            }
            return c;
        }
        
        // Attendre un peu (pas de polling du hardware)
        for (volatile int i = 0; i < 100; i++);
        wait_cycles++;
        
        // Debug périodique
        if (wait_cycles % 50000 == 0 && getc_call_count <= 2) {
            print_string_serial("GETC: still waiting...\n");
        }
    }
    
    // Timeout - retourner caractère spécial ou rien
    if (getc_call_count <= 2) {
        print_string_serial("GETC_TIMEOUT: no input received\n");
    }
    
    return 0; // Timeout sans caractère
}

// Fonctions de compatibilité
char scancode_to_ascii(uint8_t scancode) {
    if (scancode >= 128) return 0;
    return scancode_to_char[scancode];
}

// Diagnostic simple
void keyboard_diagnostic() {
    print_string_serial("=== DIAGNOSTIC CLAVIER ===\n");
    
    uint8_t status = inb(0x64);
    uint8_t pic_mask = inb(0x21);
    
    print_string_serial("Controller Status: 0x");
    char hex[] = "0123456789ABCDEF";
    write_serial(hex[(status >> 4) & 0xF]);
    write_serial(hex[status & 0xF]);
    print_string_serial("\n");
    
    print_string_serial("PIC Mask: 0x");
    write_serial(hex[(pic_mask >> 4) & 0xF]);
    write_serial(hex[pic_mask & 0xF]);
    print_string_serial(" (IRQ1 ");
    print_string_serial((pic_mask & 2) ? "BLOCKED" : "ACTIVE");
    print_string_serial(")\n");
    
    print_string_serial("Interruptions reçues: ");
    write_serial('0' + (interrupt_count % 10));
    print_string_serial("\n");
    
    print_string_serial("Caractères traités: ");
    write_serial('0' + (chars_processed % 10));
    print_string_serial("\n");
    
    print_string_serial("Mode: INTERRUPT ONLY (no polling)\n");
    print_string_serial("========================\n");
}