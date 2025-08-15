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
    
    // Initialise et démarre le timer (ce qui lancera le scheduling)
    print_string("Initialisation du timer systeme...\n");
    timer_init(TIMER_FREQUENCY); // 100 Hz
    
    // NOUVEAU: Charge et exécute le programme utilisateur depuis l'initrd
    if (module_count > 0) {
        print_string("Recherche du programme utilisateur...\n");
        char* user_program = initrd_read_file("user_program");
        if (user_program) {
            uint32_t program_size = initrd_get_file_size("user_program");
            print_string("Programme utilisateur trouve ! Chargement...\n");
            
            task_t* user_task = load_elf_task((uint8_t*)user_program, program_size);
            if (user_task) {
                print_string("Programme utilisateur charge et pret!\n");
            } else {
                print_string("ERREUR: Impossible de charger le programme utilisateur\n");
            }
        } else {
            print_string("Aucun programme utilisateur trouve dans l'initrd\n");
        }
    }
    
    print_string("\n=== Systeme AI-OS v4.0 pret ===\n");
    print_string("Fonctionnalites disponibles:\n");
    print_string("- Gestion des interruptions et clavier\n");
    print_string("- Gestionnaire de memoire physique et virtuelle\n");
    print_string("- Systeme de fichiers initrd (format TAR)\n");
    print_string("- Multitache preemptif avec ordonnancement\n");
    print_string("- Timer systeme pour le scheduling\n");
    print_string("- Appels systeme (syscalls)\n");
    print_string("- Chargeur ELF pour programmes utilisateur\n");
    print_string("- Separation kernel/user space (Ring 0/3)\n");
    print_string("\nObservez le coin inferieur droit pour voir\n");
    print_string("les taches s'executer en parallele!\n");
    print_string("Le programme utilisateur s'execute aussi!\n");

    // Le noyau peut maintenant se mettre en veille
    // Les tâches s'exécuteront grâce au timer et à l'ordonnanceur
    while(1) {
        asm volatile("hlt"); // Attend la prochaine interruption
    }
}

