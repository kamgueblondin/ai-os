#include "idt.h"
#include "interrupts.h"
#include "multiboot.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "../fs/initrd.h"

// Function to read a byte from a port
unsigned char inb(unsigned short port);
// Function to write a byte to a port
void outb(unsigned short port, unsigned char data);

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

// Fonction pour afficher un caractère à une position donnée avec une couleur donnée
void print_char_vga(char c, int x, int y, char color) {
    vga_buffer[y * 80 + x] = (unsigned short)c | (unsigned short)color << 8;
}

// Fonction pour afficher une chaîne de caractères sur VGA
void print_string_vga(const char* str, char color) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') { // Gérer le retour à la ligne
            vga_x = 0;
            vga_y++;
        } else {
            print_char_vga(str[i], vga_x, vga_y, color);
            vga_x++;
            if (vga_x >= 80) {
                vga_x = 0;
                vga_y++;
            }
        }
        
        // Gestion du défilement basique
        if (vga_y >= 25) {
            vga_y = 24;
        }
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

// La fonction principale de notre noyau - MISE À JOUR pour Multiboot
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
    vga_x = 5;
    vga_y = 5;
    print_string("=== Bienvenue dans AI-OS v2.0 ===\n");
    print_string("Systeme avance avec gestion memoire et FS\n\n");
    
    // Vérification du magic number Multiboot
    if (multiboot_magic != MULTIBOOT_MAGIC) {
        print_string("ERREUR: Magic Multiboot invalide!\n");
        print_string("Le systeme ne peut pas continuer.\n");
        while(1) { asm volatile("hlt"); }
    }
    
    print_string("Multiboot detecte correctement.\n");
    
    // Récupération des informations Multiboot
    multiboot_info_t* mbi = (multiboot_info_t*)multiboot_addr;
    multiboot_print_info(mbi);
    
    // Initialisation des interruptions
    print_string("Initialisation des interruptions...\n");
    idt_init();         // Initialise la table des interruptions
    interrupts_init();  // Initialise le PIC et active les interruptions
    print_string("Interruptions initialisees. Clavier pret.\n");

    // Étape 3 : Initialiser la gestion de la mémoire
    print_string("Initialisation de la gestion memoire...\n");
    uint32_t memory_size = multiboot_get_memory_size(mbi);
    pmm_init(memory_size);
    print_string("Physical Memory Manager initialise.\n");
    
    vmm_init(); // Active le paging
    print_string("Virtual Memory Manager initialise.\n");
    print_string("Paging active - Memoire virtuelle operationnelle.\n");

    // Étape 4 : Initialiser l'initrd
    print_string("Recherche de l'initrd...\n");
    uint32_t module_count = multiboot_get_module_count(mbi);
    
    if (module_count > 0) {
        multiboot_module_t* initrd_module = multiboot_get_module(mbi, 0);
        if (initrd_module) {
            uint32_t initrd_location = initrd_module->mod_start;
            uint32_t initrd_size = initrd_module->mod_end - initrd_module->mod_start;
            
            print_string("Initrd trouve ! Initialisation...\n");
            initrd_init(initrd_location, initrd_size);
            print_string("Fichiers disponibles:\n");
            initrd_list_files();
            
            // Test de lecture d'un fichier
            char* test_content = initrd_read_file("test.txt");
            if (test_content) {
                print_string("\nContenu de test.txt:\n");
                // Affiche les premiers caractères (limité pour éviter les débordements)
                for (int i = 0; i < 50 && test_content[i] != '\0'; i++) {
                    char c[2] = {test_content[i], '\0'};
                    print_string(c);
                }
                print_string("\n");
            }
        } else {
            print_string("Erreur: Module initrd non accessible.\n");
        }
    } else {
        print_string("Aucun module initrd trouve.\n");
        print_string("Le systeme continuera sans systeme de fichiers.\n");
    }

    print_string("\n=== Systeme AI-OS pret ===\n");
    print_string("Fonctionnalites disponibles:\n");
    print_string("- Gestion des interruptions et clavier\n");
    print_string("- Gestionnaire de memoire physique et virtuelle\n");
    print_string("- Systeme de fichiers initrd (format TAR)\n");
    print_string("- Paging actif pour la securite memoire\n");
    print_string("\nTapez sur le clavier pour tester...\n");

    // Affichage des statistiques mémoire
    print_string("\nStatistiques memoire:\n");
    print_string("Pages totales: ");
    uint32_t total = pmm_get_total_pages();
    // Affichage simple du nombre (conversion basique)
    char num_str[16];
    int i = 0;
    if (total == 0) {
        num_str[i++] = '0';
    } else {
        while (total > 0) {
            num_str[i++] = '0' + (total % 10);
            total /= 10;
        }
    }
    num_str[i] = '\0';
    // Inverse la chaîne
    for (int j = 0; j < i / 2; j++) {
        char tmp = num_str[j];
        num_str[j] = num_str[i - 1 - j];
        num_str[i - 1 - j] = tmp;
    }
    print_string(num_str);
    print_string("\n");

    // Le CPU attendra passivement une interruption
    while(1) {
        asm volatile("hlt");
    }
}

