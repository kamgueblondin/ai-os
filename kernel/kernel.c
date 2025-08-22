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
#include <stddef.h>

// Function to read a byte from a port
unsigned char inb(unsigned short port);
// Function to write a byte to a port
void outb(unsigned short port, unsigned char data);
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

// Initialisation du port série pour la simulation clavier
void init_serial_for_keyboard() {
    outb(0x3F8 + 1, 0x00);    // Disable all interrupts
    outb(0x3F8 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(0x3F8 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(0x3F8 + 1, 0x00);    //                  (hi byte)
    outb(0x3F8 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(0x3F8 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(0x3F8 + 4, 0x0B);    // IRQs enabled, RTS/DSR set

    print_string_serial("Port serie initialise pour simulation clavier.\n");
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

// Simulation clavier : lit depuis le port série et injecte dans le buffer clavier
void simulate_keyboard_input() {
    char c = read_serial();
    if (c != 0) {
        // Debug: log du caractère reçu
        print_string_serial("SIM: recv='");
        write_serial(c);
        print_string_serial("'\n");

        // Injecter le caractère dans le buffer clavier
        syscall_add_input_char(c);

        // Debug: confirmer l'injection
        print_string_serial("SIM: injected into buffer\n");
    }
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

// Fonction pour afficher un caractère à la position du curseur
void print_char(char c, int x, int y, char color) {
    if (x == -1 && y == -1) {
        // Utilise la position actuelle du curseur
        if (c == '\n') {
            vga_x = 0;
            vga_y++;
        } else if (c == '\b') {
            // Gestion du backspace
            if (vga_x > 0) {
                vga_x--;
                print_char_vga(' ', vga_x, vga_y, color);
            } else if (vga_y > 0) {
                vga_y--;
                vga_x = 79;
                print_char_vga(' ', vga_x, vga_y, color);
            }
        } else {
            print_char_vga(c, vga_x, vga_y, color);
            vga_x++;
        }

        // Gestion du passage à la ligne suivante
        if (vga_x >= 80) {
            vga_x = 0;
            vga_y++;
        }

        // Gestion du défilement
        if (vga_y >= 25) {
            scroll_screen();
            vga_y = 24;
            vga_x = 0;
        }
    } else {
        print_char_vga(c, x, y, color);
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
    keyboard_init();    // Initialise le clavier
    print_string("Interruptions initialisees.\n");

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

    // PHASE 2: Réactiver le timer pour les interruptions clavier
    print_string("PHASE 2: Timer reactive pour interruptions clavier...\n");
    // timer_init(100); // Réactiver le timer à 100Hz pour les interruptions
    print_string("Timer reactive - Interruptions clavier fonctionnelles.\n");

    // NOUVEAU: Lancement du shell interactif avec IA
    print_string("Lancement du shell interactif AI-OS...\n");

    // Initialiser le port série pour la simulation clavier
    init_serial_for_keyboard();

    if (module_count > 0) {
        // Chercher le shell dans l'initrd
        uint8_t* shell_program = (uint8_t*)initrd_read_file("shell");
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
    
    task_t* shell_task = create_task_from_initrd_file("shell");
    if (!shell_task) {
        print_string("ERREUR: Impossible de creer la tache shell. Arret du systeme.\n");
        while(1) asm volatile("hlt");
    }
    
    print_string("Tache shell prete. Demarrage du timer...\n");
    timer_init(100);

    print_string("\n=== AI-OS v6.0 - Shell Utilisateur Actif ===\n");
    print_string("Passage au mode utilisateur...\n");
    
    // Passer au shell utilisateur au lieu de rester en simulation
    shell_task->state = TASK_READY;
    current_task = shell_task;
    
    print_string("Transfert vers l'espace utilisateur...\n");
    
    // Changer vers le répertoire de pages de la tâche utilisateur
    if (current_directory != shell_task->vmm_dir) {
        vmm_switch_page_directory(shell_task->vmm_dir->physical_dir);
        current_directory = shell_task->vmm_dir;
    }
    
    // Déclarer la fonction assembleur
    extern void switch_to_userspace(uint32_t eip, uint32_t esp);
    
    // Utiliser la fonction assembleur pour la transition
    switch_to_userspace(shell_task->cpu_state.eip, shell_task->cpu_state.useresp);
    
    // Ce code ne devrait jamais être atteint
    print_string("ERREUR: Retour inattendu du mode utilisateur!\n");
    while(1) asm volatile("hlt");
}

