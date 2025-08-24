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
static void kbd_put(char c) {
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
    uint8_t scancode = inb(0x60); // Lit le scancode
    
    // Debug : envoie scancode sur port série
    print_string_serial("KBD sc=0x");
    char hex[3] = "00";
    hex[0] = (scancode >> 4) < 10 ? '0' + (scancode >> 4) : 'A' + (scancode >> 4) - 10;
    hex[1] = (scancode & 0xF) < 10 ? '0' + (scancode & 0xF) : 'A' + (scancode & 0xF) - 10;
    print_string_serial(hex);
    print_string_serial("\n");
    
    // Ajouter le scancode au buffer pour les syscalls
    kbd_push_scancode(scancode);
    
    // Aussi convertir en ASCII et stocker dans le buffer local pour compatibilité
    if (!(scancode & 0x80)) {
        char c = scancode_to_ascii(scancode);
        if (c) {
            kbd_put(c);
            print_string_serial("KBD char='");
            print_string_serial(&c);
            print_string_serial("'\n");
        }
    }
    
    // Réveiller une tâche en attente (implémentation simplifiée)
    if (kbd_waiting) {
        // TODO: Implémenter wake_task(kbd_waiting) quand le système de tâches sera disponible
        kbd_waiting = NULL;
    }
    
    // Important : envoyer EOI au PIC pour permettre d'autres IRQ
    outb(0x20, 0x20);
}

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
    while (kbd_get_nonblock(&c) == -1) {
        // Buffer vide, attendre (implémentation simplifiée avec hlt)
        asm volatile("hlt");
    }
    return c;
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
    print_string_serial("Initialisation du clavier PS/2...\n");
    
    // Step 1: Disable devices
    print_string_serial("KBD: Desactivation des ports PS/2...\n");
    keyboard_send_command(0xAD); // Disable first PS/2 port
    keyboard_send_command(0xA7); // Disable second PS/2 port (if exists)

    // Step 2: Flush output buffer
    print_string_serial("KBD: Vidage du buffer de sortie...\n");
    int flush_count = 0;
    while ((inb(0x64) & 1) && flush_count < 100) {
        inb(0x60);
        flush_count++;
    }
    print_string_serial("KBD: Buffer vide.\n");

    // Step 3: Set controller configuration byte
    print_string_serial("KBD: Configuration du controleur...\n");
    keyboard_send_command(0x20); // Read config byte
    uint8_t config = keyboard_read_data();
    
    print_string_serial("KBD: Config actuelle: 0x");
    char hex[3] = "00";
    hex[0] = (config >> 4) < 10 ? '0' + (config >> 4) : 'A' + (config >> 4) - 10;
    hex[1] = (config & 0xF) < 10 ? '0' + (config & 0xF) : 'A' + (config & 0xF) - 10;
    print_string_serial(hex);
    print_string_serial("\n");
    
    config |= 1;     // Enable interrupt for port 1
    config &= ~0x10; // Enable clock for port 1
    config &= ~0x40; // Disable translation
    
    keyboard_send_command(0x60); // Write config byte
    keyboard_send_data(config);
    print_string_serial("KBD: Nouvelle configuration appliquee.\n");

    // Step 4: Enable device
    print_string_serial("KBD: Activation du premier port...\n");
    keyboard_send_command(0xAE); // Enable first PS/2 port

    // Step 5: Reset device and wait for response
    print_string_serial("KBD: Reset du clavier...\n");
    keyboard_send_data(0xFF); // Reset command

    // Attendre ACK et le résultat du self-test avec timeout
    int timeout = 1000000;
    while (timeout-- > 0 && !(inb(0x64) & 1));
    
    if (timeout > 0) {
        uint8_t ack = keyboard_read_data();
        if (ack == 0xFA) {
            print_string_serial("KBD: Keyboard ACK recu.\n");
            
            // Attendre le résultat du self-test
            timeout = 1000000;
            while (timeout-- > 0 && !(inb(0x64) & 1));
            
            if (timeout > 0) {
                uint8_t test_result = keyboard_read_data();
                if (test_result == 0xAA) {
                    print_string_serial("KBD: Self-test reussi.\n");
                } else {
                    print_string_serial("KBD: Self-test echoue.\n");
                }
            } else {
                print_string_serial("KBD: Timeout en attente du self-test.\n");
            }
        } else {
            print_string_serial("KBD: Pas d'ACK recu.\n");
        }
    } else {
        print_string_serial("KBD: Timeout en attente de l'ACK.\n");
    }

    print_string_serial("PS/2 Keyboard initialise et pret.\n");
    
    // Debug : vérifier le masque PIC
    uint8_t pic_mask = inb(0x21);
    print_string_serial("PIC1 mask: 0x");
    char hex_mask[3] = "00";
    hex_mask[0] = (pic_mask >> 4) < 10 ? '0' + (pic_mask >> 4) : 'A' + (pic_mask >> 4) - 10;
    hex_mask[1] = (pic_mask & 0xF) < 10 ? '0' + (pic_mask & 0xF) : 'A' + (pic_mask & 0xF) - 10;
    print_string_serial(hex_mask);
    print_string_serial("\n");
    
    // S'assurer que IRQ1 n'est pas masqué
    if (pic_mask & (1 << 1)) {
        print_string_serial("IRQ1 est masque, demasquage...\n");
        pic_mask &= ~(1 << 1);
        outb(0x21, pic_mask);
        print_string_serial("IRQ1 demasque.\n");
    } else {
        print_string_serial("IRQ1 n'est pas masque.\n");
    }
}
