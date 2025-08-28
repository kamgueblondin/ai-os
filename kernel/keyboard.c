#include "keyboard.h"
#include "kernel.h"
#include <stdint.h>

// Fonctions externes pour les ports I/O et autres
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char value);
extern void print_char_vga(char c, int x, int y, char color);
extern void write_serial(char c);
extern int vga_x, vga_y;

#define KBD_BUF_SIZE 256
static volatile uint32_t kbd_buf[KBD_BUF_SIZE];
static volatile unsigned int kbd_head = 0;
static volatile unsigned int kbd_tail = 0;

// État des modifieurs
static int shift_pressed = 0;
static int altgr_pressed = 0;
static int ctrl_pressed = 0;

// Pour les "dead keys" (touches mortes)
static uint32_t dead_key_state = 0;

// Table de conversion scancode PS/2 Set 1 vers Unicode (AZERTY Français)
// Source: https://wiki.osdev.org/PS/2_Keyboard#Scan_Code_Set_1
// et https://www.utf8-chartable.de/

// Layout de base (sans modifieurs)
static const uint32_t ps2_map_base[128] = {
    0, 0x1B, '&', 0xE9, '"', '\'', '(', '-', 0xE8, '_', 0xE7, 0xE0, ')', '=', '\b', '\t',
    'a', 'z', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '^', '$', '\n', 0, 'q', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm', 0xF9, '%', 0, '*', '<', 'w', 'x', 'c',
    'v', 'b', 'n', '?', '.', '/', 0xA7, 0, 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Layout avec SHIFT
static const uint32_t ps2_map_shift[128] = {
    0, 0x1B, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 0xB0, '+', '\b', '\t',
    'A', 'Z', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0xA8, 0xA3, '\n', 0, 'Q', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', 'M', '?', '.', 0, '>', 'W', 'X', 'C',
    'V', 'B', 'N', ',', ';', ':', '!', 0, 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Layout avec ALT-GR
static const uint32_t ps2_map_altgr[128] = {
    0, 0, '~', '#', '{', '[', '|', '`', '\\', '^', '@', ']', '}', 0, '\b', '\t',
    0, 0, 0x20AC, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\n', 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, ' ', 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


void kbd_put_char(uint32_t c) {
    if (c == 0) return;
    unsigned int next = (kbd_head + 1) & (KBD_BUF_SIZE - 1);
    if (next != kbd_tail) {
        kbd_buf[kbd_head] = c;
        kbd_head = next;
    }
}

int kbd_get_char_nonblock(uint32_t *out) {
    if (kbd_head == kbd_tail) {
        return 0; // Vide
    }
    *out = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) & (KBD_BUF_SIZE - 1);
    return 1; // Succès
}

// Gère la logique des touches mortes
uint32_t handle_dead_key(uint32_t key) {
    if (dead_key_state == 0) return key;

    uint32_t accent = dead_key_state;
    dead_key_state = 0; // Reset state

    switch (accent) {
        case '^': // Circonflexe
            switch (key) {
                case 'a': return 0xE2; // â
                case 'e': return 0xEA; // ê
                case 'i': return 0xEE; // î
                case 'o': return 0xF4; // ô
                case 'u': return 0xFB; // û
                case 'A': return 0xC2; // Â
                case 'E': return 0xCA; // Ê
                case 'I': return 0xCE; // Î
                case 'O': return 0xD4; // Ô
                case 'U': return 0xDB; // Û
                default: return key; // Pas de combinaison, retourne la touche
            }
        case 0xA8: // Tréma (diaeresis)
            switch (key) {
                case 'a': return 0xE4; // ä
                case 'e': return 0xEB; // ë
                case 'i': return 0xEF; // ï
                case 'o': return 0xF6; // ö
                case 'u': return 0xFC; // ü
                case 'y': return 0xFF; // ÿ
                case 'A': return 0xC4; // Ä
                case 'E': return 0xCB; // Ë
                case 'I': return 0xCF; // Ï
                case 'O': return 0xD6; // Ö
                case 'U': return 0xDC; // Ü
                default: return key;
            }
    }
    return key; // Retourne la touche si pas de combinaison
}


uint32_t scancode_to_unicode(uint8_t scancode) {
    if (scancode >= 128) return 0;

    uint32_t key;

    if (altgr_pressed) {
        key = ps2_map_altgr[scancode];
    } else if (shift_pressed) {
        key = ps2_map_shift[scancode];
    } else {
        key = ps2_map_base[scancode];
    }

    if (key == 0) return 0; // Pas de mapping pour cette touche

    // Gestion des touches mortes
    if (key == '^' || key == 0xA8) { // '^' ou '¨'
        if (dead_key_state == key) { // Appuyer deux fois sur la touche morte
            dead_key_state = 0;
            return key; // Retourne le caractère lui-même
        }
        dead_key_state = key;
        return 0; // Ne retourne rien, attend la prochaine touche
    }

    return handle_dead_key(key);
}

void keyboard_interrupt_handler() {
    uint8_t scancode = inb(0x60);
    
    // Scancodes pour les modifieurs (utilisez #define pour les case-labels)
    #define LSHIFT_PRESS    0x2A
    #define LSHIFT_RELEASE  0xAA
    #define RSHIFT_PRESS    0x36
    #define RSHIFT_RELEASE  0xB6
    #define LCTRL_PRESS     0x1D
    #define LCTRL_RELEASE   0x9D
    #define ALTGR_PRESS     0xE0 // Préfixe pour AltGr
    
    static int altgr_prefix = 0;

    if (scancode == ALTGR_PRESS) {
        altgr_prefix = 1;
        return;
    }

    if (altgr_prefix) {
        if (scancode == 0x1D) altgr_pressed = 1; // AltGr press
        if (scancode == 0x9D) altgr_pressed = 0; // AltGr release
        altgr_prefix = 0;
        return;
    }

    switch (scancode) {
        case LSHIFT_PRESS:
        case RSHIFT_PRESS:
            shift_pressed = 1;
            return;
        case LSHIFT_RELEASE:
        case RSHIFT_RELEASE:
            shift_pressed = 0;
            return;
        case LCTRL_PRESS:
            ctrl_pressed = 1;
            return;
        case LCTRL_RELEASE:
            ctrl_pressed = 0;
            return;
    }

    // C'est un "key press" si le bit 7 n'est pas à 1
    if (!(scancode & 0x80)) {
        uint32_t unicode_char = scancode_to_unicode(scancode);
        if (unicode_char != 0) {
            kbd_put_char(unicode_char);
        }
    }
}

void keyboard_init() {
    print_string_serial("=== UNICODE KEYBOARD INIT (AZERTY) ===\n");
    // La logique d'initialisation du contrôleur PS/2 reste la même
    // car elle est indépendante du mapping des scancodes.
    
    // Vider le buffer du clavier
    while (inb(0x64) & 1) {
        inb(0x60);
    }
    
    // Envoyer la commande de self-test (0xAA) au clavier
    outb(0x64, 0xAA);
    // Attendre la réponse
    while(inb(0x60) != 0x55);

    // Configuration du contrôleur
    outb(0x64, 0x20); // Lire le "command byte"
    uint8_t status = inb(0x60);
    status |= 1;  // Activer l'IRQ1
    status &= ~0x10; // Désactiver la translation de scancodes (on le fait nous-mêmes)
    outb(0x64, 0x60); // Ecrire le "command byte"
    outb(0x60, status);

    // Activer le clavier
    outb(0x60, 0xF4);
    while(inb(0x60) != 0xFA); // Attendre l'ACK

    print_string_serial("Keyboard init done.\n");
}

uint32_t keyboard_getc(void) {
    uint32_t c;
    
    // Attendre qu'un caractère soit disponible
    while (kbd_get_char_nonblock(&c) == 0) {
        // On pourrait ajouter un "hlt" ici pour économiser le CPU
        // en attendant la prochaine interruption clavier.
        asm volatile("hlt");
    }
    
    return c;
}

// Fonction de compatibilité gardée pour ne pas casser d'autres parties du code
// qui pourraient encore l'appeler. Elle est maintenant obsolète.
char scancode_to_ascii(uint8_t scancode) {
    uint32_t unicode = scancode_to_unicode(scancode);
    if (unicode > 0 && unicode < 128) {
        return (char)unicode;
    }
    return 0; // Retourne 0 pour les caractères non-ASCII
}
