#include "keyboard.h"
#include "kernel.h"
#include <stdint.h>
#include "input/kbd_buffer.h"

// Fonctions externes pour les ports I/O et autres
extern unsigned char inb(unsigned short port);
extern void print_char_vga(char c, int x, int y, char color);
extern void write_serial(char c);
extern int vga_x, vga_y;

// Buffer clavier amélioré pour caractères ASCII
#define KBD_BUF_SIZE 256
static volatile unsigned char kbd_buf[KBD_BUF_SIZE];
static volatile unsigned int kbd_head = 0;
static volatile unsigned int kbd_tail = 0;

// Pointeur vers la tâche en attente (simplifié pour ce projet)
static volatile void *kbd_waiting = NULL;

// Fonctions pour gérer le buffer ASCII
void kbd_put(char c) {
    unsigned int next = (kbd_head + 1) & (KBD_BUF_SIZE - 1);
    if (next != kbd_tail) { // buffer not full
        kbd_buf[kbd_head] = c;
        kbd_head = next;
    }
    // Si buffer plein, on ignore le caractère (overflow)
}

int kbd_get_nonblock(char *out) {
    if (kbd_head == kbd_tail) return -1; // buffer vide
    *out = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) & (KBD_BUF_SIZE - 1);
    return 0;
}

// Table de correspondance complète Scancode -> ASCII (pour un clavier US QWERTY)
// ... (le reste de la table est inchangé)
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


// Le handler appelé par l'ISR.
void keyboard_interrupt_handler() {
    print_string_serial("=== INTERRUPTION CLAVIER RECUE ===\n");
    
    uint8_t scancode = inb(0x60); // Lit le scancode
    
    // Debug : envoie scancode sur port série
    print_string_serial("KBD sc=0x");
    char hex[3] = "00";
    hex[0] = (scancode >> 4) < 10 ? '0' + (scancode >> 4) : 'A' + (scancode >> 4) - 10;
    hex[1] = (scancode & 0xF) < 10 ? '0' + (scancode & 0xF) : 'A' + (scancode & 0xF) - 10;
    print_string_serial(hex);
    print_string_serial("\n");
    
    // Ajouter le scancode au buffer pour les syscalls (pour compatibilité)
    kbd_push_scancode(scancode);
    
    // Convertir en ASCII et stocker dans le buffer local (prioritaire)
    if (!(scancode & 0x80)) { // Ignore les key releases (bit 7 = 1)
        char c = scancode_to_ascii(scancode);
        if (c) {
            kbd_put(c);
            print_string_serial("KBD char='");
            write_serial(c);
            print_string_serial("' ajouté au buffer ASCII\n");
            
            // Forcer un reschedule pour réveiller les tâches en attente
            extern volatile int g_reschedule_needed;
            g_reschedule_needed = 1;
            print_string_serial("Reschedule déclenché\n");
        } else {
            print_string_serial("KBD: scancode non convertible en ASCII\n");
        }
    } else {
        print_string_serial("KBD: key release ignoré\n");
    }
    
    // S'assurer que les interruptions sont toujours activées
    asm volatile("sti");
    
    print_string_serial("=== FIN INTERRUPTION CLAVIER ===\n");
}

// Convertit un scancode en caractère ASCII (implémentation simplifiée)
char scancode_to_ascii(uint8_t scancode) {
    // On ne gère pas les majuscules ou les modificateurs ici, pour rester simple.
    // On ignore les codes de relâchement de touche.
    if (scancode & 0x80) {
        return 0;
    }
    if (scancode >= 128) {
        return 0;
    }
    return scancode_map[scancode];
}

// Fonction pour lire un caractère depuis le buffer (utilisée par les syscalls)
char keyboard_getc(void) {
    char c;
    int timeout = 0;
    const int MAX_TIMEOUT = 10000; // Timeout plus court
    
    // Essayer de lire un caractère du buffer de manière non-bloquante
    if (kbd_get_nonblock(&c) == 0) {
        print_string_serial("keyboard_getc: caractère lu du buffer: '");
        write_serial(c);
        print_string_serial("'\n");
        return c;
    }
    
    // Si aucun caractère n'est disponible, utiliser le polling direct
    print_string_serial("keyboard_getc: buffer vide, polling direct du clavier...\n");
    
    // Réactiver les interruptions
    asm volatile("sti");
    
    while (timeout < MAX_TIMEOUT) {
        // D'abord essayer le buffer (au cas où une interruption arriverait)
        if (kbd_get_nonblock(&c) == 0) {
            print_string_serial("keyboard_getc: caractère reçu via interruption: '");
            write_serial(c);
            print_string_serial("'\n");
            return c;
        }
        
        // Polling direct du clavier (méthode alternative)
        uint8_t status = inb(0x64);
        if (status & 0x01) { // Données disponibles
            uint8_t scancode = inb(0x60);
            
            // Ne traiter que les key press (pas key release)
            if (!(scancode & 0x80)) {
                char ch = scancode_to_ascii(scancode);
                if (ch) {
                    // Ajouter au buffer pour cohérence
                    kbd_put(ch);
                    print_string_serial("keyboard_getc: caractère lu par polling: '");
                    write_serial(ch);
                    print_string_serial("'\n");
                    return ch;
                }
            }
        }
        
        // Courte pause
        for (volatile int i = 0; i < 100; i++) {
            asm volatile("nop");
        }
        timeout++;
        
        // Céder le CPU périodiquement
        if (timeout % 100 == 0) {
            asm volatile("int $0x30");
        }
    }
    
    print_string_serial("keyboard_getc: TIMEOUT - retour caractère par défaut\n");
    return '\n'; // Retourner une nouvelle ligne en cas de timeout
}


// Helper functions for PS/2 controller communication
void keyboard_wait_for_input() {
    int retries = 100000;
    while (retries-- > 0 && (inb(0x64) & 1) == 0);
}

void keyboard_wait_for_output() {
    int retries = 100000;
    while (retries-- > 0 && (inb(0x64) & 2) != 0);
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

// Initializes the PS/2 Keyboard
void keyboard_init() {
    print_string_serial("=== INITIALISATION CLAVIER PS/2 ===\n");
    
    // Étape 0: Vider le buffer de sortie initial
    print_string_serial("KBD: Vidage préliminaire du buffer...\n");
    int initial_flush = 0;
    while ((inb(0x64) & 1) && initial_flush < 100) {
        inb(0x60);
        initial_flush++;
    }
    
    // Étape 1: Désactiver les ports PS/2
    print_string_serial("KBD: Désactivation des ports PS/2...\n");
    keyboard_send_command(0xAD); // Disable first PS/2 port
    keyboard_send_command(0xA7); // Disable second PS/2 port (if exists)

    // Étape 2: Vider le buffer de sortie après désactivation
    print_string_serial("KBD: Vidage du buffer après désactivation...\n");
    int flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 100) {
        inb(0x60);
        flush_count++;
    }
    print_string_serial("KBD: Buffer vidé.\n");

    // Étape 3: Lire et modifier la configuration du contrôleur
    print_string_serial("KBD: Configuration du contrôleur...\n");
    keyboard_send_command(0x20); // Read config byte
    uint8_t config = keyboard_read_data();
    
    print_string_serial("KBD: Config actuelle: 0x");
    print_hex_byte_serial(config);
    print_string_serial("\n");
    
    // Modifier la configuration pour QEMU:
    config |= 0x01;  // Enable interrupt for port 1 (IRQ1)
    config &= ~0x10; // Enable clock for port 1
    config &= ~0x20; // Enable clock for port 2 (si présent)
    config &= ~0x40; // Disable translation (scancode set 1)
    
    print_string_serial("KBD: Nouvelle config: 0x");
    print_hex_byte_serial(config);
    print_string_serial("\n");
    
    keyboard_send_command(0x60); // Write config byte
    keyboard_send_data(config);
    print_string_serial("KBD: Configuration appliquée.\n");

    // Étape 4: Effectuer un self-test du contrôleur
    print_string_serial("KBD: Test du contrôleur PS/2...\n");
    keyboard_send_command(0xAA); // Controller self test
    uint8_t test_result = keyboard_read_data();
    
    if (test_result == 0x55) {
        print_string_serial("KBD: Contrôleur PS/2 OK\n");
    } else {
        print_string_serial("KBD: ERREUR contrôleur PS/2!\n");
    }

    // Étape 5: Tester le port 1
    print_string_serial("KBD: Test du port 1...\n");
    keyboard_send_command(0xAB); // Test port 1
    uint8_t port_test = keyboard_read_data();
    
    if (port_test == 0x00) {
        print_string_serial("KBD: Port 1 OK\n");
    } else {
        print_string_serial("KBD: ERREUR port 1!\n");
    }

    // Étape 6: Activer le port 1
    print_string_serial("KBD: Activation du port 1...\n");
    keyboard_send_command(0xAE); // Enable first PS/2 port

    // Étape 7: Configurer le clavier lui-même
    print_string_serial("KBD: Configuration du périphérique clavier...\n");
    
    // Désactiver le scanning pendant la configuration
    keyboard_send_data(0xF5); // Disable scanning
    
    int timeout = 100000;
    while (timeout-- > 0 && !(inb(0x64) & 1));
    if (timeout > 0) {
        uint8_t ack = keyboard_read_data();
        if (ack == 0xFA) {
            print_string_serial("KBD: Scanning désactivé (ACK)\n");
        }
    }
    
    // Configurer le scancode set 1 (compatible QEMU)
    keyboard_send_data(0xF0); // Set scancode set
    timeout = 100000;
    while (timeout-- > 0 && !(inb(0x64) & 1));
    if (timeout > 0) {
        uint8_t ack = keyboard_read_data();
        if (ack == 0xFA) {
            keyboard_send_data(0x01); // Scancode set 1
            print_string_serial("KBD: Scancode set 1 configuré\n");
        }
    }
    
    // Réactiver le scanning
    keyboard_send_data(0xF4); // Enable scanning
    timeout = 100000;
    while (timeout-- > 0 && !(inb(0x64) & 1));
    if (timeout > 0) {
        uint8_t ack = keyboard_read_data();
        if (ack == 0xFA) {
            print_string_serial("KBD: Scanning réactivé (ACK)\n");
        }
    }

    // Étape 8: Test final - Lire l'état du contrôleur
    print_string_serial("KBD: Test final...\n");
    uint8_t final_status = inb(0x64);
    print_string_serial("KBD: Status final du contrôleur: 0x");
    print_hex_byte_serial(final_status);
    print_string_serial("\n");

    print_string_serial("=== CLAVIER PS/2 INITIALISE ===\n");
    
    // Diagnostic final du PIC après init clavier
    print_string_serial("=== DIAGNOSTIC PIC APRES INIT CLAVIER ===\n");
    uint8_t pic_mask = inb(0x21);
    print_string_serial("PIC1 mask: 0x");
    print_hex_byte_serial(pic_mask);
    print_string_serial("\n");
    print_string_serial("IRQ1 (clavier): ");
    print_string_serial((pic_mask & 2) ? "MASQUE" : "ACTIVE");
    print_string_serial("\n");
    print_string_serial("==========================================\n");
}

// Helper function pour afficher un byte en hexadécimal
void print_hex_byte_serial(uint8_t byte) {
    char hex[3] = "00";
    hex[0] = (byte >> 4) < 10 ? '0' + (byte >> 4) : 'A' + (byte >> 4) - 10;
    hex[1] = (byte & 0xF) < 10 ? '0' + (byte & 0xF) : 'A' + (byte & 0xF) - 10;
    print_string_serial(hex);
}
