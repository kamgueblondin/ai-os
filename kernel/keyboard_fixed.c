#include "keyboard.h"
#include "kernel.h"
#include <stdint.h>

// Fonctions externes pour les ports I/O et autres
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char value);
extern void print_char_vga(char c, int x, int y, char color);
extern void write_serial(char c);
extern int vga_x, vga_y;

// Buffer clavier amélioré pour caractères ASCII
#define KBD_BUF_SIZE 256
static volatile unsigned char kbd_buf[KBD_BUF_SIZE];
static volatile unsigned int kbd_head = 0;
static volatile unsigned int kbd_tail = 0;

// Variables de debug globales
static volatile int debug_interrupt_count = 0;
static volatile int debug_getc_calls = 0;
static volatile int debug_buffer_puts = 0;
static volatile int debug_buffer_gets = 0;

// Délai pour les opérations PS/2
void ps2_delay() {
    for (volatile int i = 0; i < 10000; i++);
}

// Attendre que le contrôleur soit prêt pour recevoir une commande
void wait_kbd_command_ready() {
    int timeout = 100000;
    while (timeout-- > 0) {
        if (!(inb(0x64) & 0x02)) break;
        ps2_delay();
    }
}

// Attendre qu'une donnée soit disponible
void wait_kbd_data_ready() {
    int timeout = 100000;
    while (timeout-- > 0) {
        if (inb(0x64) & 0x01) break;
        ps2_delay();
    }
}

// Fonctions pour gérer le buffer ASCII
void kbd_put(char c) {
    unsigned int next = (kbd_head + 1) & (KBD_BUF_SIZE - 1);
    if (next != kbd_tail) { // buffer not full
        kbd_buf[kbd_head] = c;
        kbd_head = next;
        debug_buffer_puts++;
        
        // Debug limité pour éviter le spam
        if (debug_buffer_puts <= 5) {
            print_string_serial("BUFFER_PUT: '");
            write_serial(c);
            print_string_serial("' success\n");
        }
    } else {
        print_string_serial("BUFFER_FULL: overflow\n");
    }
}

int kbd_get_nonblock(char *out) {
    if (kbd_head == kbd_tail) {
        return -1; // Buffer vide
    }
    
    *out = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) & (KBD_BUF_SIZE - 1);
    debug_buffer_gets++;
    
    return 0;
}

// Table de correspondance Scancode -> ASCII (scancode set 1)
const char scancode_map[128] = {
    // 0x00-0x0F
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    // 0x10-0x1F (QWERTY première ligne)
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
    // 0x20-0x2F (QWERTY deuxième ligne)
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    // 0x30-0x3F (QWERTY troisième ligne et plus)
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    // 0x40-0x4F (Touches de fonction F1-F10)
    0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
    // 0x50-0x5F (Pavé numérique)
    '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // Le reste à zéro
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Handler d'interruption clavier corrigé
void keyboard_interrupt_handler() {
    debug_interrupt_count++;
    
    // Debug limité pour les premiers appels
    if (debug_interrupt_count <= 3) {
        print_string_serial("IRQ1_HANDLER: appel #");
        if (debug_interrupt_count < 10) write_serial('0' + debug_interrupt_count);
        else write_serial('9');
        print_string_serial("\n");
    }
    
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) {
        // Pas de données disponibles, ignorer
        return;
    }
    
    uint8_t scancode = inb(0x60);
    
    // Debug du scancode reçu (limité aux premiers)
    if (debug_interrupt_count <= 10) {
        print_string_serial("IRQ1_SCAN: 0x");
        char hex_chars[] = "0123456789ABCDEF";
        write_serial(hex_chars[scancode >> 4]);
        write_serial(hex_chars[scancode & 0xF]);
        print_string_serial("\n");
    }
    
    // Gérer les codes PS/2 spéciaux
    if (scancode == 0xFA) {
        // ACK - commande acceptée, ignorer
        if (debug_interrupt_count <= 5) {
            print_string_serial("IRQ1_ACK: command accepted\n");
        }
        return;
    }
    
    if (scancode == 0xFE) {
        // NACK - commande rejetée, ignorer
        if (debug_interrupt_count <= 5) {
            print_string_serial("IRQ1_NACK: command rejected\n");
        }
        return;
    }
    
    if (scancode == 0xAA) {
        // Self-test passed, ignorer
        if (debug_interrupt_count <= 5) {
            print_string_serial("IRQ1_SELFTEST: passed\n");
        }
        return;
    }
    
    // Ignorer les key releases (bit 7 = 1)
    if (scancode & 0x80) {
        if (debug_interrupt_count <= 10) {
            print_string_serial("IRQ1_RELEASE: key up\n");
        }
        return;
    }
    
    // Conversion scancode vers caractère
    char c = scancode_to_ascii(scancode);
    if (c) {
        kbd_put(c);
        
        if (debug_interrupt_count <= 10) {
            print_string_serial("IRQ1_CHAR: '");
            write_serial(c);
            print_string_serial("' added\n");
        }
        
        // Forcer un reschedule
        extern volatile int g_reschedule_needed;
        g_reschedule_needed = 1;
    } else {
        if (debug_interrupt_count <= 10) {
            print_string_serial("IRQ1_UNMAPPED: no ASCII\n");
        }
    }
}

// Convertit un scancode en caractère ASCII
char scancode_to_ascii(uint8_t scancode) {
    if (scancode >= 128) return 0;
    return scancode_map[scancode];
}

// Fonction keyboard_getc avec timeout amélioré
char keyboard_getc(void) {
    char c;
    int timeout = 0;
    const int MAX_TIMEOUT = 1000000; // Timeout plus long
    
    debug_getc_calls++;
    
    // Debug limité
    if (debug_getc_calls <= 3) {
        print_string_serial("GETC_START: waiting for input\n");
    }
    
    // Réactiver les interruptions immédiatement
    asm volatile("sti");
    
    while (timeout < MAX_TIMEOUT) {
        if (kbd_get_nonblock(&c) == 0) {
            if (debug_getc_calls <= 5) {
                print_string_serial("GETC_SUCCESS: got '");
                write_serial(c);
                print_string_serial("'\n");
            }
            return c;
        }
        
        timeout++;
        if (timeout % 100000 == 0) {
            // Périodiquement, vérifier le statut du buffer
            if (debug_getc_calls <= 3 && timeout % 500000 == 0) {
                print_string_serial("GETC_WAIT: still waiting...\n");
            }
        }
    }
    
    // Timeout - retourner un caractère par défaut
    if (debug_getc_calls <= 3) {
        print_string_serial("GETC_TIMEOUT: no input received\n");
    }
    
    return 0; // ou peut-être '\n' pour débloquer le shell
}

// Fonctions d'aide pour l'initialisation
void keyboard_send_command(uint8_t cmd) {
    wait_kbd_command_ready();
    outb(0x64, cmd);
    ps2_delay();
}

void keyboard_send_data(uint8_t data) {
    wait_kbd_command_ready();
    outb(0x60, data);
    ps2_delay();
}

uint8_t keyboard_read_data() {
    wait_kbd_data_ready();
    uint8_t data = inb(0x60);
    ps2_delay();
    return data;
}

// Initialisation complète du clavier corrigée
void keyboard_init() {
    print_string_serial("=== INIT CLAVIER CORRIGÉ ===\n");
    
    // Reset des compteurs debug
    debug_interrupt_count = 0;
    debug_getc_calls = 0;
    debug_buffer_puts = 0;
    debug_buffer_gets = 0;
    kbd_head = 0;
    kbd_tail = 0;
    
    // Étape 1: Vider complètement le buffer
    print_string_serial("KBD: Vidage initial...\n");
    int flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 200) {
        inb(0x60);
        ps2_delay();
        flush_count++;
    }
    
    // Étape 2: Désactiver les ports PS/2
    print_string_serial("KBD: Désactivation ports...\n");
    keyboard_send_command(0xAD); // Disable first PS/2 port
    keyboard_send_command(0xA7); // Disable second PS/2 port
    
    // Étape 3: Vider le buffer après désactivation
    flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 200) {
        inb(0x60);
        ps2_delay();
        flush_count++;
    }
    
    // Étape 4: Configuration du contrôleur
    print_string_serial("KBD: Configuration contrôleur...\n");
    keyboard_send_command(0x20);
    uint8_t config = keyboard_read_data();
    
    // Modifier la configuration
    config |= 0x01;  // Enable interrupt for port 1
    config &= ~0x10; // Enable clock for port 1
    config &= ~0x20; // Enable clock for port 2 (au cas où)
    config &= ~0x40; // Disable translation (mode scancode set 1)
    
    keyboard_send_command(0x60);
    keyboard_send_data(config);
    print_string_serial("KBD: Configuration appliquée\n");
    
    // Étape 5: Test du contrôleur
    print_string_serial("KBD: Test contrôleur...\n");
    keyboard_send_command(0xAA);
    uint8_t test_result = keyboard_read_data();
    
    if (test_result == 0x55) {
        print_string_serial("KBD: Contrôleur OK\n");
    } else {
        print_string_serial("KBD: ERREUR contrôleur\n");
    }
    
    // Étape 6: Test du port 1
    print_string_serial("KBD: Test port 1...\n");
    keyboard_send_command(0xAB);
    uint8_t port_test = keyboard_read_data();
    
    if (port_test == 0x00) {
        print_string_serial("KBD: Port 1 OK\n");
    } else {
        print_string_serial("KBD: ERREUR port 1\n");
    }
    
    // Étape 7: Activer le port 1
    print_string_serial("KBD: Activation port 1...\n");
    keyboard_send_command(0xAE);
    
    // Étape 8: Reset complet du clavier
    print_string_serial("KBD: Reset du périphérique...\n");
    keyboard_send_data(0xFF); // Reset keyboard
    
    // Attendre la réponse au reset
    int reset_timeout = 1000000;
    uint8_t reset_response = 0;
    while (reset_timeout-- > 0) {
        if (inb(0x64) & 1) {
            reset_response = inb(0x60);
            break;
        }
        ps2_delay();
    }
    
    if (reset_response == 0xAA) {
        print_string_serial("KBD: Reset réussi\n");
    } else {
        print_string_serial("KBD: Reset échoué\n");
    }
    
    // Attendre et vider les éventuels ACK supplémentaires
    for (int i = 0; i < 100000; i++) {
        if (inb(0x64) & 1) {
            uint8_t data = inb(0x60);
            if (data != 0xFA) break; // Stop si ce n'est pas un ACK
        }
        ps2_delay();
    }
    
    // Étape 9: Configuration du scancode set 1 (optionnel, souvent déjà par défaut)
    print_string_serial("KBD: Configuration scancode set...\n");
    keyboard_send_data(0xF0); // Set scancode set command
    
    // Attendre ACK
    int ack_timeout = 100000;
    while (ack_timeout-- > 0) {
        if (inb(0x64) & 1) {
            uint8_t ack = inb(0x60);
            if (ack == 0xFA) break;
        }
        ps2_delay();
    }
    
    keyboard_send_data(0x01); // Scancode set 1
    
    // Attendre ACK final
    ack_timeout = 100000;
    while (ack_timeout-- > 0) {
        if (inb(0x64) & 1) {
            uint8_t ack = inb(0x60);
            if (ack == 0xFA) break;
        }
        ps2_delay();
    }
    
    // Étape 10: Activer le scanning
    print_string_serial("KBD: Activation du scanning...\n");
    keyboard_send_data(0xF4); // Enable scanning
    
    // Attendre ACK
    ack_timeout = 100000;
    while (ack_timeout-- > 0) {
        if (inb(0x64) & 1) {
            uint8_t ack = inb(0x60);
            if (ack == 0xFA) {
                print_string_serial("KBD: Scanning activé\n");
                break;
            }
        }
        ps2_delay();
    }
    
    // Grande pause pour stabilisation
    print_string_serial("KBD: Pause de stabilisation...\n");
    for (volatile int i = 0; i < 1000000; i++);
    
    // Vider une dernière fois le buffer
    flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 50) {
        uint8_t data = inb(0x60);
        flush_count++;
        ps2_delay();
    }
    
    print_string_serial("=== CLAVIER INIT TERMINÉ ===\n");
}

// Diagnostic du clavier
void keyboard_diagnostic() {
    print_string_serial("=== DIAGNOSTIC CLAVIER ===\n");
    
    uint8_t status = inb(0x64);
    print_string_serial("Status contrôleur: 0x");
    char hex[] = "0123456789ABCDEF";
    write_serial(hex[status >> 4]);
    write_serial(hex[status & 0xF]);
    print_string_serial("\n");
    
    uint8_t pic_mask = inb(0x21);
    print_string_serial("Masque PIC: 0x");
    write_serial(hex[pic_mask >> 4]);
    write_serial(hex[pic_mask & 0xF]);
    print_string_serial("\n");
    
    print_string_serial("IRQ1 (clavier): ");
    print_string_serial((pic_mask & 2) ? "MASQUÉ" : "ACTIF");
    print_string_serial("\n");
    
    print_string_serial("Compteurs:\n");
    print_string_serial("- Interruptions: ");
    if (debug_interrupt_count < 100) {
        if (debug_interrupt_count >= 10) write_serial('0' + (debug_interrupt_count / 10));
        write_serial('0' + (debug_interrupt_count % 10));
    } else {
        write_serial('9');
        write_serial('+');
    }
    print_string_serial("\n- Buffer puts: ");
    if (debug_buffer_puts < 100) {
        if (debug_buffer_puts >= 10) write_serial('0' + (debug_buffer_puts / 10));
        write_serial('0' + (debug_buffer_puts % 10));
    } else {
        write_serial('9');
        write_serial('+');
    }
    print_string_serial("\n- Buffer gets: ");
    if (debug_buffer_gets < 100) {
        if (debug_buffer_gets >= 10) write_serial('0' + (debug_buffer_gets / 10));
        write_serial('0' + (debug_buffer_gets % 10));
    } else {
        write_serial('9');
        write_serial('+');
    }
    print_string_serial("\n");
    
    print_string_serial("========================\n");
}
