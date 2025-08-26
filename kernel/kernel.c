#include "gdt.h"
#include "idt.h"
#include "interrupts.h"
#include "multiboot.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "task/task.h"
#include "timer.h"
#include "syscall/syscall.h"
#include "elf.h"
#include "../fs/initrd.h"
#include "keyboard.h"
#include "debug_serial.h"
#include "io.h"
#include <stddef.h>

// --- BEGIN DIAGNOSTIC FUNCTIONS ---
void dump_pic_masks_and_unmask_irq1(void){
    uint8_t master = inb(0x21);
    uint8_t slave  = inb(0xA1);
    serial_puts("PIC M:");
    serial_puthex32(master);
    serial_puts(" S:");
    serial_puthex32(slave);
    serial_putc_direct('\n');

    if(master & (1 << 1)){
        serial_puts("Unmasking IRQ1\n");
        master &= ~(1 << 1);
        outb(0x21, master);
    } else {
        serial_puts("IRQ1 already unmasked\n");
    }
}

extern struct idt_entry idt[];

void dump_idt_for_keyboard(void){
    unsigned vec = 33; // IRQ1 is remapped to INT 33
    struct idt_entry *e = &idt[vec];
    uint32_t off = ((uint32_t)e->base_high << 16) | e->base_low;
    serial_puts("IDT_VEC:");
    serial_puthex32(vec);
    serial_puts(" -> ");
    serial_puthex32(off);
    serial_putc_direct('\n');
}
// --- END DIAGNOSTIC FUNCTIONS ---

// Function to print string to serial port (forward declaration)
void print_string_serial(const char* str);

void serial_init() {
    // Disable all interrupts
    outb(0x3F8 + 1, 0x00);
    // Enable DLAB (set baud rate divisor)
    outb(0x3F8 + 3, 0x80);
    // Set baud rate to 38400 (divisor = 3)
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    // Disable DLAB, set 8 data bits, 1 stop bit, no parity
    outb(0x3F8 + 3, 0x03);
    // Enable FIFO, clear them, with 14-byte threshold
    outb(0x3F8 + 2, 0xC7);
    // IRQs enabled, RTS/DSR set
    outb(0x3F8 + 4, 0x0B);
}

int is_transmit_empty() {
    return inb(0x3F8 + 5) & 0x20;
}

void write_serial(char a) {
    while (!is_transmit_empty());
    outb(0x3F8, a);
}

// Fonction pour vérifier si des données sont disponibles en lecture sur le port série
int is_receive_ready() {
    return inb(0x3F8 + 5) & 0x01;
}

// Fonction pour lire un caractère depuis le port série (non-bloquante)
char read_serial() {
    if (is_receive_ready()) {
        return inb(0x3F8);
    }
    return 0; // Aucun caractère disponible
}

// Function to read a byte from a port
unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

// Function to write a byte to a port
void outb(unsigned short port, unsigned char data) {
    asm volatile ("outb %0, %1" : : "a"(data), "dN"(port));
}

// Fonction pic_send_eoi définie dans interrupts.c
extern void pic_send_eoi(unsigned char irq);

// Pointeur vers la mémoire vidéo VGA. L'adresse 0xB8000 est standard.
volatile unsigned short* vga_buffer = (unsigned short*)0xB8000;
// Position actuelle du curseur
int vga_x = 0;
int vga_y = 0;

// Fonction pour faire défiler l'écran vers le haut
void scroll_screen() {
    // Déplacer toutes les lignes vers le haut
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 80; x++) {
            vga_buffer[y * 80 + x] = vga_buffer[(y + 1) * 80 + x];
        }
    }

    // Effacer la dernière ligne
    for (int x = 0; x < 80; x++) {
        vga_buffer[24 * 80 + x] = (unsigned short)' ' | (unsigned short)0x07 << 8;
    }
}

// Fonction pour afficher un caractère à une position donnée avec une couleur donnée
void print_char_vga(char c, int x, int y, char color) {
    if (x >= 0 && y >= 0 && x < 80 && y < 25) {
        vga_buffer[y * 80 + x] = (unsigned short)c | (unsigned short)color << 8;
    }
}

// Définir les états pour le parseur de codes ANSI
typedef enum {
    NORMAL,
    ESCAPE,
    BRACKET,
    PARAM
} AnsiState;

// Variables statiques pour conserver l'état du parseur
static AnsiState ansi_state = NORMAL;
static char ansi_buffer[16];
static int ansi_pos = 0;
static char current_color = 0x0F; // Blanc sur noir par défaut

void clear_screen_vga() {
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 80; x++) {
            vga_buffer[y * 80 + x] = (unsigned short)' ' | ((unsigned short)current_color << 8);
        }
    }
    vga_x = 0;
    vga_y = 0;
}

// Fonction pour parser les paramètres numériques des codes ANSI
int ansi_parse_param() {
    int val = 0;
    for (int i = 0; i < ansi_pos; i++) {
        val = val * 10 + (ansi_buffer[i] - '0');
    }
    return val;
}


// Remplace l'ancienne fonction print_char par celle-ci
void print_char(char c, int x, int y, char color) {
    if (ansi_state == NORMAL && c != '\x1b') {
        if (x == -1 && y == -1) {
            if (c == '\n') {
                vga_x = 0; vga_y++;
            } else if (c == '\b') {
                if (vga_x > 0) vga_x--;
                print_char_vga(' ', vga_x, vga_y, current_color);
            } else {
                print_char_vga(c, vga_x, vga_y, current_color);
                vga_x++;
            }
            if (vga_x >= 80) { vga_x = 0; vga_y++; }
            if (vga_y >= 25) { scroll_screen(); vga_y = 24; }
        } else {
            print_char_vga(c, x, y, color);
        }
        return;
    }

    // Gestion de la machine à états ANSI
    switch (ansi_state) {
        case NORMAL:
            if (c == '\x1b') {
                ansi_state = ESCAPE;
            }
            break;

        case ESCAPE:
            if (c == '[') {
                ansi_state = BRACKET;
                ansi_pos = 0;
                for(int i=0; i<16; ++i) ansi_buffer[i] = 0;
            } else {
                ansi_state = NORMAL;
            }
            break;

        case BRACKET:
            if ((c >= '0' && c <= '9') || c == ';') {
                if (ansi_pos < 15) ansi_buffer[ansi_pos++] = c;
                ansi_state = PARAM;
            } else if (c == 'H') { // Cursor to Home (0,0)
                vga_x = 0;
                vga_y = 0;
                ansi_state = NORMAL;
            } else if (c == 'J') { // Erase screen
                clear_screen_vga();
                ansi_state = NORMAL;
            } else if (c == 'm') { // Reset color
                current_color = 0x0F;
                ansi_state = NORMAL;
            } else {
                ansi_state = NORMAL;
            }
            break;

        case PARAM:
            if ((c >= '0' && c <= '9') || c == ';') {
                if (ansi_pos < 15) ansi_buffer[ansi_pos++] = c;
            } else {
                ansi_buffer[ansi_pos] = '\0';
                if (c == 'm') {
                    // For now, we only parse the first parameter for simplicity
                    int code = ansi_parse_param();
                    switch (code) {
                        case 0: current_color = 0x0F; break; // Reset
                        case 1: /* Ignore Bright */ break;
                        case 30: current_color = 0x00; break; // Black
                        case 31: current_color = 0x04; break; // Red
                        case 32: current_color = 0x02; break; // Green
                        case 33: current_color = 0x06; break; // Yellow
                        case 34: current_color = 0x01; break; // Blue
                        case 35: current_color = 0x05; break; // Magenta
                        case 36: current_color = 0x03; break; // Cyan
                        case 37: current_color = 0x07; break; // White
                    }
                } else if (c == 'J') {
                    int code = ansi_parse_param();
                    if (code == 2) clear_screen_vga();
                } else if (c == 'H') {
                    // For now, ignore params and just go to 0,0
                    vga_x = 0;
                    vga_y = 0;
                }
                ansi_state = NORMAL;
            }
            break;
    }
}

// Fonction pour afficher une chaîne de caractères sur VGA
void print_string_vga(const char* str, char color) {
    for (int i = 0; str[i] != '\0'; i++) {
        print_char(str[i], -1, -1, color);
    }
}

// Fonction pour afficher une chaîne de caractères sur le port série
void print_string_serial(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        write_serial(str[i]);
    }
}

// Fonction pour afficher un uint32_t en hexadecimal sur le port série
void print_hex_serial(uint32_t n) {
    char* hex = "0123456789abcdef";
    write_serial('0');
    write_serial('x');
    for (int i = 28; i >= 0; i -= 4) {
        write_serial(hex[(n >> i) & 0xF]);
    }
}

// Fonction pour afficher sur les deux sorties
void print_string(const char* str) {
    print_string_vga(str, 0x1F);
    print_string_serial(str);
}

// Fonction de comparaison de chaînes simple
int strcmp_simple(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) {
            return s1[i] - s2[i];
        }
        i++;
    }
    return s1[i] - s2[i];
}

// Tâches de test pour démontrer le multitâche
void task_A_function() {
    int counter = 0;
    while(1) {
        print_char_vga('A', 78, 24, 0x1C); // Affiche 'A' en rouge dans le coin

        // Petit délai pour ralentir l'affichage
        for (volatile int i = 0; i < 1000000; i++);

        counter++;
        if (counter > 50) {
            print_string_serial("Tache A se termine\n");
            task_exit();
        }
    }
}

void task_B_function() {
    int counter = 0;
    while(1) {
        print_char_vga('B', 79, 24, 0x1A); // Affiche 'B' en vert juste à côté

        // Petit délai pour ralentir l'affichage
        for (volatile int i = 0; i < 1500000; i++);

        counter++;
        if (counter > 30) {
            print_string_serial("Tache B se termine\n");
            task_exit();
        }
    }
}

void task_C_function() {
    int counter = 0;
    while(1) {
        print_char_vga('C', 77, 24, 0x1E); // Affiche 'C' en jaune

        // Petit délai différent
        for (volatile int i = 0; i < 2000000; i++);

        counter++;
        if (counter > 20) {
            print_string_serial("Tache C se termine\n");
            task_exit();
        }
    }
}

// Fonction pour chercher une sous-chaîne
int strstr_simple(const char* haystack, const char* needle) {
    int i, j;
    for (i = 0; haystack[i] != '\0'; i++) {
        for (j = 0; needle[j] != '\0' && haystack[i + j] == needle[j]; j++);
        if (needle[j] == '\0') return 1;
    }
    return 0;
}

// Fonction pour effacer l'écran
void clear_screen() {
    for (int i = 0; i < 80 * 25; i++) {
        vga_buffer[i] = (unsigned short)' ' | (unsigned short)0x07 << 8;
    }
    vga_x = 0;
    vga_y = 0;
}

// La fonction principale de notre noyau - MISE À JOUR pour le multitâche
void kmain(uint32_t multiboot_magic, uint32_t multiboot_addr) {
    char color = 0x1F;

    // Initialisation du port série
    serial_init();

    // Initialisation de la GDT et du TSS
    gdt_init();

    // Effacer l'écran VGA
    for (int y = 0; y < 25; y++) {
        for (int x = 0; x < 80; x++) {
            print_char_vga(' ', x, y, color);
        }
    }

    // Afficher notre message de bienvenue
    vga_x = 2;
    vga_y = 2;
    print_string("=== Bienvenue dans AI-OS v4.0 ===\n");
    print_string("Systeme complet avec espace utilisateur\n\n");

    // Vérification du magic number Multiboot
    if (multiboot_magic != MULTIBOOT_MAGIC) {
        print_string("ERREUR: Magic Multiboot invalide!\n");
        print_string("Le systeme ne peut pas continuer.\n");
        while(1) { asm volatile("hlt"); }
    }

    print_string("Multiboot detecte correctement.\n");

    // Récupération des informations Multiboot
    multiboot_info_t* mbi = (multiboot_info_t*)multiboot_addr;

    // Initialisation des interruptions
    print_string("Initialisation des interruptions...\n");
    idt_init();         // Initialise la table des interruptions
    interrupts_init();  // Initialise le PIC et active les interruptions

    // Initialise le clavier avec les interruptions désactivées pour éviter les race conditions
    asm volatile("cli");
    keyboard_init();
    asm volatile("sti");

    print_string("Interruptions et clavier initialises.\n");

    // -- BEGIN DIAGNOSTIC CALLS --
    dump_pic_masks_and_unmask_irq1();
    dump_idt_for_keyboard();
    // -- END DIAGNOSTIC CALLS --

    // Initialiser la gestion de la mémoire
    print_string("Initialisation de la gestion memoire...\n");
    uint32_t memory_size = multiboot_get_memory_size(mbi);
    pmm_init(memory_size, multiboot_addr);
    print_string("Physical Memory Manager initialise.\n");

    vmm_init(); // Active le paging
    print_string("Virtual Memory Manager initialise.\n");

    // Initialiser l'initrd si disponible
    uint32_t module_count = multiboot_get_module_count(mbi);
    if (module_count > 0) {
        multiboot_module_t* initrd_module = multiboot_get_module(mbi, 0);
        if (initrd_module) {
            uint32_t initrd_location = initrd_module->mod_start;
            uint32_t initrd_size = initrd_module->mod_end - initrd_module->mod_start;

            print_string("Initrd trouve ! Initialisation...\n");
            initrd_init(initrd_location, initrd_size);
        }
    }

    // NOUVEAU: Initialisation du système de tâches
    print_string("Initialisation du systeme de taches...\n");
    tasking_init();

    // NOUVEAU: Initialisation des appels système
    print_string("Initialisation des appels systeme...\n");
    syscall_init();

    // Crée des tâches de test kernel (DÉSACTIVÉ pour stabilité)
    print_string("Creation des taches kernel de demonstration... DESACTIVE\n");
    print_string("Mode mono-tache pour stabilite maximale.\n");

/*
 * SUPPRIMEZ OU COMMENTEZ CES LIGNES
 *
 * task_t* task_a = create_task(task_A_function);
 * if (task_a) print_string("Tache A creee\n");
 *
 * task_t* task_b = create_task(task_B_function);
 * if (task_b) print_string("Tache B creee\n");
 *
 * task_t* task_c = create_task(task_C_function);
 * if (task_c) print_string("Tache C creee\n");
*/


    // PHASE 2: Réactiver le timer pour les interruptions clavier
    print_string("PHASE 2: Timer reactive pour interruptions clavier...\n");
    // timer_init(100); // Réactiver le timer à 100Hz pour les interruptions
    print_string("Timer reactive - Interruptions clavier fonctionnelles.\n");

    // NOUVEAU: Lancement du shell interactif avec IA
    print_string("Lancement du shell interactif AI-OS...\n");

    if (module_count > 0) {
        // Chercher le shell dans l'initrd
        uint8_t* shell_program = (uint8_t*)initrd_read_file("bin/shell");
        if (shell_program) {
            print_string("Shell trouve ! Chargement...\n");

            // Le code de simulation est retiré, on va lancer le vrai shell.
            print_string("Shell trouve. Preparation du lancement...\n");
        } else {
            print_string("ERREUR: Fichier 'shell' non trouve dans l'initrd!\n");
        }
    } else {
        print_string("ERREUR: Aucun module initrd trouve!\n");
    }

    // --- Lancement du Shell Utilisateur ---
    print_string("\nLancement du Shell Utilisateur...\n");
    
    task_t* shell_task = create_task_from_initrd_file("bin/shell");
    if (!shell_task) {
        print_string("ERREUR: Impossible de creer la tache shell. Arret du systeme.\n");
        while(1) asm volatile("hlt");
    }
    
    print_string("Tache shell prete. Demarrage du timer...\n");
    timer_init(100);

    print_string("\n=== AI-OS v6.0 - Attente du premier changement de contexte ===\n");
    print_string("Le planificateur va maintenant prendre le relais.\n");
    
    // Activer les interruptions pour que le timer puisse déclencher le scheduler
    asm volatile("sti");

    // Boucle d'inactivité du kernel. Le scheduler fera le travail.
    while(1) {
        asm volatile("hlt");
    }
}
