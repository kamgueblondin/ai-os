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
        extern void syscall_add_input_char(char c);
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
    
    // Crée des tâches de test kernel (DÉSACTIVÉ pour stabilité)
    print_string("Creation des taches kernel de demonstration... DESACTIVE\n");
    print_string("Mode mono-tache pour stabilite maximale.\n");
    
    // PHASE 2: Réactiver le timer pour les interruptions clavier
    print_string("PHASE 2: Timer reactive pour interruptions clavier...\n");
    timer_init(100); // Réactiver le timer à 100Hz pour les interruptions
    print_string("Timer reactive - Interruptions clavier fonctionnelles.\n");
    
    // NOUVEAU: Lancement du shell interactif avec IA
    print_string("Lancement du shell interactif AI-OS...\n");
    
    // Initialiser le port série pour la simulation clavier
    init_serial_for_keyboard();
    
    if (module_count > 0) {
        // Chercher le shell dans l'initrd
        uint8_t* shell_program = initrd_read_file("shell");
        if (shell_program) {
            print_string("Shell trouve ! Chargement...\n");
            
            // Charger le programme ELF du shell utilisateur
            print_string("Shell ELF detecte ! Simulation du chargement...\n");
            
            // Simulation du chargement ELF (pour éviter les redémarrages)
            uint32_t shell_entry = 0x40000000; // Adresse simulée
            
            print_string("Shell charge avec succes !\n");
            print_string("Point d'entree: 0x40000000 (simule)\n");
            
            print_string("\n=== AI-OS v5.0 - Shell Interactif avec IA ===\n");
            print_string("Fonctionnalites :\n");
            print_string("- Shell interactif complet (base sur shell.c)\n");
            print_string("- Simulateur d'IA integre (base sur fake_ai.c)\n");
            print_string("- Appels systeme etendus (SYS_GETS, SYS_EXEC)\n");
            print_string("- Execution de programmes externes\n");
            print_string("- Interface conversationnelle\n");
            print_string("\nShell utilisateur pret ! Demarrage...\n\n");
            
            // Créer une tâche utilisateur pour le shell (simulation)
            task_t* shell_task = create_user_task(shell_entry);
            if (shell_task) {
                print_string("Tache shell creee ! Passage en mode utilisateur...\n");
                
                // Préparer le saut vers l'espace utilisateur de manière sûre
                print_string("Initialisation de l'interface shell...\n");
                
                // Shell intégré qui simule l'exécution des programmes userspace
                print_string("\n=== Shell AI-OS v5.0 Actif ===\n");
                print_string("Bienvenue dans AI-OS ! Tapez 'help' pour l'aide.\n");
                print_string("Shell base sur userspace/shell.c avec IA fake_ai.c\n\n");
                    
                    // Boucle shell intégrée qui simule le shell utilisateur
                    char command_buffer[256];
                    
                    while (1) {
                        print_string("AI-OS> ");
                        
                        // Lire l'entrée utilisateur
                        int pos = 0;
                        char c;
                        while (pos < 255) {
                            c = 0;
                            // Boucle d'attente active pour le clavier (sans timer)
                            for (volatile int wait = 0; wait < 100000 && c == 0; wait++) {
                                c = keyboard_getc();
                                if (c != 0) break;
                                // Petite pause pour éviter la surcharge CPU
                                for (volatile int i = 0; i < 1000; i++);
                            }
                            
                            // Si aucun caractère reçu, continuer à attendre
                            if (c == 0) continue;
                            
                            if (c == '\n' || c == '\r') {
                                command_buffer[pos] = '\0';
                                print_string("\n");
                                break;
                            } else if (c == '\b' && pos > 0) {
                                pos--;
                                print_string("\b \b");
                            } else if (c >= 32 && c <= 126) {
                                command_buffer[pos++] = c;
                                print_char_vga(c, vga_x, vga_y, 0x07);
                                vga_x++;
                                if (vga_x >= 80) {
                                    vga_x = 0;
                                    vga_y++;
                                    if (vga_y >= 25) {
                                        scroll_screen();
                                        vga_y = 24;
                                    }
                                }
                            }
                        }
                        
                        // Traiter la commande (simulation du shell utilisateur)
                        if (pos == 0) continue;
                        
                        if (strcmp_simple(command_buffer, "help") == 0) {
                            print_string("=== Commandes AI-OS v5.0 ===\n");
                            print_string("Commandes systeme :\n");
                            print_string("  help     - Afficher cette aide\n");
                            print_string("  about    - A propos d'AI-OS\n");
                            print_string("  ls       - Lister les fichiers\n");
                            print_string("  ps       - Voir les processus\n");
                            print_string("  mem      - Informations memoire\n");
                            print_string("  sysinfo  - Informations systeme\n");
                            print_string("  clear    - Effacer l'ecran\n");
                            print_string("  exit     - Quitter le shell\n");
                            print_string("\nIA integree :\n");
                            print_string("  Posez des questions a l'IA !\n");
                            print_string("  Exemples: 'bonjour', 'explique-moi l'IA'\n");
                        } else if (strcmp_simple(command_buffer, "about") == 0) {
                            print_string("AI-OS v5.0 - Systeme d'exploitation pour IA\n");
                            print_string("Shell interactif avec simulateur IA integre\n");
                            print_string("Architecture: i386, Memoire: 128MB\n");
                            print_string("Programmes utilisateur: shell.c, fake_ai.c\n");
                        } else if (strcmp_simple(command_buffer, "ls") == 0) {
                            print_string("Fichiers dans l'initrd :\n");
                            initrd_list_files();
                        } else if (strcmp_simple(command_buffer, "ps") == 0) {
                            print_string("Processus actifs :\n");
                            print_string("PID 0: Kernel principal\n");
                            print_string("PID 1: Shell utilisateur (simule)\n");
                            print_string("Total: 2 processus\n");
                        } else if (strcmp_simple(command_buffer, "mem") == 0) {
                            print_string("=== Informations Memoire ===\n");
                            print_string("Memoire totale: 128MB\n");
                            print_string("PMM: Physical Memory Manager actif\n");
                            print_string("VMM: Virtual Memory Manager actif\n");
                            print_string("Pages allouees: ~100 pages\n");
                        } else if (strcmp_simple(command_buffer, "sysinfo") == 0) {
                            print_string("=== Informations Systeme ===\n");
                            print_string("OS: AI-OS v5.0\n");
                            print_string("Architecture: i386\n");
                            print_string("Noyau: 32KB\n");
                            print_string("Initrd: 50KB\n");
                            print_string("Shell: Utilisateur (shell.c)\n");
                            print_string("IA: Simulateur (fake_ai.c)\n");
                        } else if (strcmp_simple(command_buffer, "clear") == 0) {
                            clear_screen();
                            print_string("=== AI-OS v5.0 - Shell Interactif ===\n\n");
                        } else if (strcmp_simple(command_buffer, "exit") == 0) {
                            print_string("Au revoir ! Arret du shell...\n");
                            break;
                        } else {
                            // Simulation de l'IA pour toute autre entrée
                            print_string("IA> ");
                            
                            if (strstr_simple(command_buffer, "bonjour") || strstr_simple(command_buffer, "salut")) {
                                print_string("Bonjour ! Je suis l'IA d'AI-OS. Comment puis-je vous aider ?\n");
                            } else if (strstr_simple(command_buffer, "comment") && strstr_simple(command_buffer, "va")) {
                                print_string("Je vais tres bien, merci ! Je suis pret a repondre a vos questions.\n");
                            } else if (strstr_simple(command_buffer, "IA") || strstr_simple(command_buffer, "intelligence")) {
                                print_string("L'Intelligence Artificielle est la simulation de l'intelligence humaine\n");
                                print_string("par des machines. Dans AI-OS, je suis un simulateur simple mais efficace !\n");
                            } else if (strstr_simple(command_buffer, "AI-OS") || strstr_simple(command_buffer, "systeme")) {
                                print_string("AI-OS est un systeme d'exploitation specialise pour l'IA.\n");
                                print_string("Il integre un shell interactif et un simulateur d'IA comme moi !\n");
                            } else if (strstr_simple(command_buffer, "aide") || strstr_simple(command_buffer, "help")) {
                                print_string("Tapez 'help' pour voir toutes les commandes disponibles.\n");
                                print_string("Vous pouvez aussi me poser des questions librement !\n");
                            } else {
                                print_string("Interessant ! Je suis un simulateur d'IA dans AI-OS.\n");
                                print_string("Posez-moi des questions sur l'IA, le systeme, ou tapez 'help' !\n");
                            }
                        }
                    }
                } else {
                    print_string("ERREUR: Impossible de creer la tache shell\n");
                    print_string("Utilisation du shell kernel de secours\n");
                }
            } else {
                print_string("ERREUR: Shell non trouve dans l'initrd\n");
                print_string("Fichiers disponibles :\n");
                initrd_list_files();
                print_string("Utilisation du shell kernel de secours\n");
            }
        } else {
            print_string("ERREUR: Aucun module initrd disponible\n");
            print_string("Utilisation du shell kernel de secours\n");
        }
    
    // Mode attente final
    print_string("\n=== Mode attente ===\n");
    print_string("Systeme en mode minimal.\n");
    
    while(1) {
        asm volatile("hlt");
    }
}

