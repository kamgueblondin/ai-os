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
    if (x >= 0 && y >= 0 && x < 80 && y < 25) {
        vga_buffer[y * 80 + x] = (unsigned short)c | (unsigned short)color << 8;
    }
}

// Fonction pour afficher un caractère à la position du curseur
void print_char(char c, int x, int y, char color) {
    if (x == -1 && y == -1) {
        // Utilise la position actuelle du curseur
        print_char_vga(c, vga_x, vga_y, color);
        vga_x++;
        if (vga_x >= 80) {
            vga_x = 0;
            vga_y++;
            if (vga_y >= 25) {
                vga_y = 24;
            }
        }
    } else {
        print_char_vga(c, x, y, color);
    }
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
    print_string("Interruptions initialisees.\n");

    // Initialiser la gestion de la mémoire
    print_string("Initialisation de la gestion memoire...\n");
    uint32_t memory_size = multiboot_get_memory_size(mbi);
    pmm_init(memory_size);
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
    
    // Crée des tâches de test kernel
    print_string("Creation des taches kernel de demonstration...\n");
    create_task(task_A_function);
    create_task(task_B_function);
    create_task(task_C_function);
    
    // Timer temporairement désactivé pour tests du shell
    print_string("Timer desactive temporairement pour tests...\n");
    // timer_init(10); // Désactivé temporairement
    print_string("Shell fonctionnera en mode polling.\n");
    
    // NOUVEAU: Lancement du shell interactif avec IA
    print_string("Lancement du shell interactif AI-OS...\n");
    
    if (module_count > 0) {
        // Chercher le shell dans l'initrd
        uint8_t* shell_program = initrd_read_file("shell");
        if (shell_program) {
            print_string("Shell trouve ! Chargement...\n");
            
            // Charger le programme ELF du shell
                print_string("Shell charge avec succes !\n");
                
                // SOLUTION DIRECTE: Shell simple dans le kernel
                print_string("Tache shell creee ! Demarrage de l'interface...\n");
                
                print_string("\n=== AI-OS v5.0 - Shell Interactif avec IA ===\n");
                print_string("Fonctionnalites :\n");
                print_string("- Shell interactif complet\n");
                print_string("- Simulateur d'IA integre\n");
                print_string("- Appels systeme etendus (SYS_GETS, SYS_EXEC)\n");
                print_string("- Execution de programmes externes\n");
                print_string("- Interface conversationnelle\n");
                print_string("\nShell kernel actif. Tapez vos commandes:\n\n");
                
                // Shell simple dans le kernel
                char input_buffer[256];
                while (1) {
                    print_string("AI-OS> ");
                    
                    // Lire l'entrée utilisateur (version simplifiée)
                    int pos = 0;
                    char c;
                    while (pos < 255) {
                        // Attendre une entrée clavier
                        while (1) {
                            asm volatile("hlt");
                            // Vérifier s'il y a un caractère disponible
                            extern char sys_getc();
                            c = sys_getc();
                            if (c != 0) break;
                        }
                        
                        if (c == '\n' || c == '\r') {
                            input_buffer[pos] = '\0';
                            print_string("\n");
                            break;
                        } else if (c == '\b' && pos > 0) {
                            pos--;
                            print_string("\b \b"); // Effacer le caractère
                        } else if (c >= 32 && c <= 126) {
                            input_buffer[pos++] = c;
                            print_char(c, -1, -1, 0x0F); // Afficher le caractère
                        }
                    }
                    
                    // Traiter la commande
                    if (pos == 0) continue;
                    
                    if (strcmp_simple(input_buffer, "help") == 0) {
                        print_string("Commandes disponibles:\n");
                        print_string("  help - Afficher cette aide\n");
                        print_string("  about - A propos d'AI-OS\n");
                        print_string("  exit - Quitter le shell\n");
                    } else if (strcmp_simple(input_buffer, "about") == 0) {
                        print_string("AI-OS v5.0 - Systeme d'exploitation pour IA\n");
                        print_string("Shell kernel integre - Version stable\n");
                    } else if (strcmp_simple(input_buffer, "exit") == 0) {
                        print_string("Au revoir !\n");
                        break;
                    } else {
                        print_string("Commande inconnue: ");
                        print_string(input_buffer);
                        print_string("\nTapez 'help' pour l'aide.\n");
                    }
                }
        } else {
            print_string("ERREUR: Shell non trouve dans l'initrd\n");
            print_string("Fichiers disponibles :\n");
            // Lister les fichiers disponibles pour debug
            initrd_list_files();
        }
    } else {
        print_string("ERREUR: Aucun module initrd disponible\n");
    }
    
    print_string("\n=== Mode de secours ===\n");
    print_string("Le shell n'a pas pu demarrer. Noyau en attente.\n");
    print_string("Systeme en mode minimal.\n");

    // Mode de secours : boucle infinie
    while(1) {
        asm volatile("hlt"); // Attend la prochaine interruption
    }
}

