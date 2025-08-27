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

// Fonctions pour gérer le buffer ASCII
void kbd_put(char c) {
    unsigned int next = (kbd_head + 1) & (KBD_BUF_SIZE - 1);
    if (next != kbd_tail) { // buffer not full
        kbd_buf[kbd_head] = c;
        kbd_head = next;
        debug_buffer_puts++;
        
        // Debug ultra détaillé
        print_string_serial("BUFFER_PUT: '");
        write_serial(c);
        print_string_serial("' head=");
        if (kbd_head < 10) write_serial('0' + kbd_head);
        else write_serial('A' + (kbd_head - 10));
        print_string_serial(" puts=");
        if (debug_buffer_puts < 100) {
            if (debug_buffer_puts >= 10) write_serial('0' + (debug_buffer_puts / 10));
            write_serial('0' + (debug_buffer_puts % 10));
        } else {
            write_serial('9');
            write_serial('+');
        }
        print_string_serial("\n");
    } else {
        print_string_serial("BUFFER_FULL: overflow, char '");
        write_serial(c);
        print_string_serial("' perdu\n");
    }
}

int kbd_get_nonblock(char *out) {
    if (kbd_head == kbd_tail) {
        // Buffer vide - debug
        print_string_serial("BUFFER_EMPTY: head=tail=");
        if (kbd_head < 10) write_serial('0' + kbd_head);
        else write_serial('A' + (kbd_head - 10));
        print_string_serial("\n");
        return -1;
    }
    
    *out = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) & (KBD_BUF_SIZE - 1);
    debug_buffer_gets++;
    
    // Debug détaillé
    print_string_serial("BUFFER_GET: '");
    write_serial(*out);
    print_string_serial("' tail=");
    if (kbd_tail < 10) write_serial('0' + kbd_tail);
    else write_serial('A' + (kbd_tail - 10));
    print_string_serial(" gets=");
    if (debug_buffer_gets < 100) {
        if (debug_buffer_gets >= 10) write_serial('0' + (debug_buffer_gets / 10));
        write_serial('0' + (debug_buffer_gets % 10));
    } else {
        write_serial('9');
        write_serial('+');
    }
    print_string_serial("\n");
    
    return 0;
}

// Table de correspondance Scancode -> ASCII (inchangée)
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

// Handler d'interruption clavier avec debug ultra détaillé
void keyboard_interrupt_handler() {
    debug_interrupt_count++;
    
    // Debug FORCE pour TOUS les appels - Preuve que le handler est appelé
    print_string_serial("!!! IRQ1_HANDLER_CALLED #");
    if (debug_interrupt_count >= 100) {
        write_serial('9');
        write_serial('9');
        write_serial('+');
    } else {
        if (debug_interrupt_count >= 10) write_serial('0' + (debug_interrupt_count / 10));
        write_serial('0' + (debug_interrupt_count % 10));
    }
    print_string_serial(" !!!\n");
    
    uint8_t scancode = inb(0x60);
    
    // Debug du scancode reçu
    if (debug_interrupt_count <= 20) {
        print_string_serial("IRQ1_SCAN: 0x");
        char hex_chars[] = "0123456789ABCDEF";
        write_serial(hex_chars[scancode >> 4]);
        write_serial(hex_chars[scancode & 0xF]);
        print_string_serial("\n");
    }
    
    // Ignorer les key releases et codes de contrôle PS/2
    if (scancode & 0x80 || scancode == 0xFA || scancode == 0xFE) {
        if (debug_interrupt_count <= 20) {
            print_string_serial("IRQ1_SKIP: key release or control\n");
        }
        return;
    }
    
    // Conversion scancode vers caractère
    char c = scancode_to_ascii(scancode);
    if (c) {
        kbd_put(c);
        
        if (debug_interrupt_count <= 20) {
            print_string_serial("IRQ1_PUT: '");
            write_serial(c);
            print_string_serial("'\n");
        }
        
        // Forcer un reschedule
        extern volatile int g_reschedule_needed;
        g_reschedule_needed = 1;
    }
    
    if (debug_interrupt_count <= 10) {
        print_string_serial("IRQ1_END\n");
    }
}

// Convertit un scancode en caractère ASCII
char scancode_to_ascii(uint8_t scancode) {
    if (scancode & 0x80) return 0;
    if (scancode >= 128) return 0;
    return scancode_map[scancode];
}

// Fonction keyboard_getc avec debug ultra détaillé
char keyboard_getc(void) {
    char c;
    int timeout = 0;
    const int MAX_TIMEOUT = 200000;
    
    debug_getc_calls++;
    
    // Debug systématique
    print_string_serial("GETC_START #");
    if (debug_getc_calls < 100) {
        if (debug_getc_calls >= 10) write_serial('0' + (debug_getc_calls / 10));
        write_serial('0' + (debug_getc_calls % 10));
    } else {
        write_serial('9');
        write_serial('+');
    }
    print_string_serial(" int_count=");
    if (debug_interrupt_count < 100) {
        if (debug_interrupt_count >= 10) write_serial('0' + (debug_interrupt_count / 10));
        write_serial('0' + (debug_interrupt_count % 10));
    } else {
        write_serial('9');
        write_serial('+');
    }
    print_string_serial("\n");
    
    // Réactiver les interruptions immédiatement
    asm volatile("sti");
    
    // Vérifier l'état initial du buffer
    print_string_serial("GETC_BUFFER: head=");
    if (kbd_head < 10) write_serial('0' + kbd_head);
    else write_serial('A' + (kbd_head - 10));
    print_string_serial(" tail=");
    if (kbd_tail < 10) write_serial('0' + kbd_tail);
    else write_serial('A' + (kbd_tail - 10));
    print_string_serial("\n");
    
    // Boucle d'attente avec debug détaillé
    while (timeout < MAX_TIMEOUT) {
        // Vérifier le buffer
        if (kbd_get_nonblock(&c) == 0) {
            print_string_serial("GETC_SUCCESS: '");
            write_serial(c);
            print_string_serial("' after ");
            if (timeout < 1000) {
                if (timeout >= 100) write_serial('0' + (timeout / 100));
                if (timeout >= 10) write_serial('0' + ((timeout / 10) % 10));
                write_serial('0' + (timeout % 10));
            } else {
                write_serial('1');
                write_serial('K');
                write_serial('+');
            }
            print_string_serial(" loops\n");
            return c;
        }
        
        // Debug chaque 10K cycles
        if (timeout % 10000 == 0 && timeout > 0) {
            print_string_serial("GETC_WAIT ");
            write_serial('0' + (timeout / 10000));
            print_string_serial("0K int=");
            if (debug_interrupt_count < 100) {
                if (debug_interrupt_count >= 10) write_serial('0' + (debug_interrupt_count / 10));
                write_serial('0' + (debug_interrupt_count % 10));
            } else {
                write_serial('9');
                write_serial('+');
            }
            
            // Vérifier l'état du contrôleur PS/2
            uint8_t status = inb(0x64);
            print_string_serial(" ps2_status=0x");
            char hex_chars[] = "0123456789ABCDEF";
            write_serial(hex_chars[status >> 4]);
            write_serial(hex_chars[status & 0xF]);
            print_string_serial("\n");
        }
        
        // Pause
        for (volatile int i = 0; i < 100; i++) {
            asm volatile("nop");
        }
        timeout++;
        
        // Céder le CPU périodiquement
        if (timeout % 1000 == 0) {
            asm volatile("int $0x30");
        }
    }
    
    print_string_serial("GETC_TIMEOUT: no input detected after ");
    if (MAX_TIMEOUT >= 100000) {
        write_serial('0' + (MAX_TIMEOUT / 100000));
        print_string_serial("00K cycles\n");
    }
    print_string_serial("GETC_STATS: interrupts=");
    if (debug_interrupt_count < 100) {
        if (debug_interrupt_count >= 10) write_serial('0' + (debug_interrupt_count / 10));
        write_serial('0' + (debug_interrupt_count % 10));
    } else {
        write_serial('9');
        write_serial('+');
    }
    print_string_serial(" puts=");
    if (debug_buffer_puts < 100) {
        if (debug_buffer_puts >= 10) write_serial('0' + (debug_buffer_puts / 10));
        write_serial('0' + (debug_buffer_puts % 10));
    } else {
        write_serial('9');
        write_serial('+');
    }
    print_string_serial(" gets=");
    if (debug_buffer_gets < 100) {
        if (debug_buffer_gets >= 10) write_serial('0' + (debug_buffer_gets / 10));
        write_serial('0' + (debug_buffer_gets % 10));
    } else {
        write_serial('9');
        write_serial('+');
    }
    print_string_serial("\n");
    
    return '\n'; // Timeout - retourner ENTER par défaut
}

// Fonctions d'initialisation PS/2 (inchangées mais avec debug)
void keyboard_wait_for_input() {
    int retries = 100000;
    while (retries-- > 0 && (inb(0x64) & 1) == 0);
    if (retries <= 0) {
        print_string_serial("PS2_TIMEOUT: wait_for_input\n");
    }
}

void keyboard_wait_for_output() {
    int retries = 100000;
    while (retries-- > 0 && (inb(0x64) & 2) != 0);
    if (retries <= 0) {
        print_string_serial("PS2_TIMEOUT: wait_for_output\n");
    }
}

void keyboard_send_command(uint8_t cmd) {
    keyboard_wait_for_output();
    outb(0x64, cmd);
}

void keyboard_send_data(uint8_t data) {
    keyboard_wait_for_output();
    outb(0x60, data);
}

uint8_t keyboard_read_data() {
    keyboard_wait_for_input();
    return inb(0x60);
}

// Initialisation PS/2 avec debug détaillé
void keyboard_init() {
    print_string_serial("=== INIT CLAVIER DEBUG MODE ===\n");
    
    // Reset des compteurs debug
    debug_interrupt_count = 0;
    debug_getc_calls = 0;
    debug_buffer_puts = 0;
    debug_buffer_gets = 0;
    kbd_head = 0;
    kbd_tail = 0;
    
    // Étape 0: Vider le buffer initial
    print_string_serial("KBD: Vidage préliminaire...\n");
    int initial_flush = 0;
    while ((inb(0x64) & 1) && initial_flush < 100) {
        uint8_t data = inb(0x60);
        print_string_serial("KBD: flush 0x");
        char hex[] = "0123456789ABCDEF";
        write_serial(hex[data >> 4]);
        write_serial(hex[data & 0xF]);
        print_string_serial("\n");
        initial_flush++;
    }
    
    // Désactiver les ports PS/2
    print_string_serial("KBD: Désactivation ports...\n");
    keyboard_send_command(0xAD);
    keyboard_send_command(0xA7);

    // Vider le buffer après désactivation
    print_string_serial("KBD: Vidage post-désactivation...\n");
    int flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 100) {
        inb(0x60);
        flush_count++;
    }

    // Configuration du contrôleur
    print_string_serial("KBD: Configuration contrôleur...\n");
    keyboard_send_command(0x20);
    uint8_t config = keyboard_read_data();
    
    print_string_serial("KBD: Config actuelle: 0x");
    char hex[] = "0123456789ABCDEF";
    write_serial(hex[config >> 4]);
    write_serial(hex[config & 0xF]);
    print_string_serial("\n");
    
    config |= 0x01;  // Enable interrupt for port 1
    config &= ~0x10; // Enable clock for port 1
    config &= ~0x40; // Disable translation
    
    keyboard_send_command(0x60);
    keyboard_send_data(config);
    print_string_serial("KBD: Nouvelle config appliquée\n");

    // Self-test du contrôleur
    print_string_serial("KBD: Test contrôleur...\n");
    keyboard_send_command(0xAA);
    uint8_t test_result = keyboard_read_data();
    
    if (test_result == 0x55) {
        print_string_serial("KBD: Contrôleur OK\n");
    } else {
        print_string_serial("KBD: ERREUR contrôleur: 0x");
        write_serial(hex[test_result >> 4]);
        write_serial(hex[test_result & 0xF]);
        print_string_serial("\n");
    }

    // Test du port 1
    print_string_serial("KBD: Test port 1...\n");
    keyboard_send_command(0xAB);
    uint8_t port_test = keyboard_read_data();
    
    if (port_test == 0x00) {
        print_string_serial("KBD: Port 1 OK\n");
    } else {
        print_string_serial("KBD: ERREUR port 1: 0x");
        write_serial(hex[port_test >> 4]);
        write_serial(hex[port_test & 0xF]);
        print_string_serial("\n");
    }

    // Activer le port 1
    print_string_serial("KBD: Activation port 1...\n");
    keyboard_send_command(0xAE);

    // Configuration du clavier
    print_string_serial("KBD: Config périphérique...\n");
    
    // Désactiver scanning
    keyboard_send_data(0xF5);
    int timeout = 100000;
    while (timeout-- > 0 && !(inb(0x64) & 1));
    if (timeout > 0) {
        uint8_t ack = keyboard_read_data();
        if (ack == 0xFA) {
            print_string_serial("KBD: Scanning désactivé\n");
        } else {
            print_string_serial("KBD: ACK inattendu: 0x");
            write_serial(hex[ack >> 4]);
            write_serial(hex[ack & 0xF]);
            print_string_serial("\n");
        }
    }
    
    // Configurer scancode set 1
    keyboard_send_data(0xF0);
    timeout = 100000;
    while (timeout-- > 0 && !(inb(0x64) & 1));
    if (timeout > 0) {
        uint8_t ack = keyboard_read_data();
        if (ack == 0xFA) {
            keyboard_send_data(0x01);
            print_string_serial("KBD: Scancode set 1 configuré\n");
        }
    }
    
    // Réactiver scanning
    keyboard_send_data(0xF4);
    timeout = 100000;
    while (timeout-- > 0 && !(inb(0x64) & 1));
    if (timeout > 0) {
        uint8_t ack = keyboard_read_data();
        if (ack == 0xFA) {
            print_string_serial("KBD: Scanning réactivé\n");
        }
    }

    // Test final
    uint8_t final_status = inb(0x64);
    print_string_serial("KBD: Status final: 0x");
    write_serial(hex[final_status >> 4]);
    write_serial(hex[final_status & 0xF]);
    print_string_serial("\n");

    // Diagnostic PIC
    print_string_serial("=== DIAGNOSTIC PIC ===\n");
    uint8_t pic_mask = inb(0x21);
    print_string_serial("PIC1 mask: 0x");
    write_serial(hex[pic_mask >> 4]);
    write_serial(hex[pic_mask & 0xF]);
    print_string_serial("\n");
    print_string_serial("IRQ1 (clavier): ");
    print_string_serial((pic_mask & 2) ? "MASQUÉ" : "ACTIF");
    print_string_serial("\n");
    
    print_string_serial("=== CLAVIER INIT DEBUG TERMINÉ ===\n");
}

void print_hex_byte_serial(uint8_t byte) {
    char hex[3] = "00";
    hex[0] = (byte >> 4) < 10 ? '0' + (byte >> 4) : 'A' + (byte >> 4) - 10;
    hex[1] = (byte & 0xF) < 10 ? '0' + (byte & 0xF) : 'A' + (byte & 0xF) - 10;
    print_string_serial(hex);
}
