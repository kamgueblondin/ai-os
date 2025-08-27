#include "syscall.h"
#include "kernel.h"
#include "../interrupts.h"
#include "../task/task.h"
#include "../input/kbd_buffer.h"
#include "../keyboard.h"
#include "../elf.h"
#include "../../fs/initrd.h"
#include "../mem/string.h"

// Fonctions externes
extern void print_string_serial(const char* str);
extern void print_char(char c, int x, int y, char color);
extern unsigned char inb(unsigned short port);
extern void write_serial(char c);  // Correction: signature cohérente

// Fonction locale pour ajouter un caractère au buffer ASCII
extern void kbd_put(char c);

// Fonction de conversion scancode vers ASCII
extern char scancode_to_ascii(uint8_t scancode);


// ==============================================================================
// GESTIONNAIRE D'APPELS SYSTÈME
// ==============================================================================

void syscall_handler(cpu_state_t* cpu) {
    // Réactive les interruptions pour permettre au clavier de fonctionner
    asm volatile("sti");

    // Le numéro de syscall est dans le registre EAX
    switch (cpu->eax) {
        case SYS_EXIT:
            current_task->state = TASK_TERMINATED;
            asm volatile("int $0x30"); // On ne reviendra jamais à cette tâche
            break;
        
        case SYS_PUTC:
            {
                extern void write_serial(char a);
                print_char((char)cpu->ebx, -1, -1, 0x0F); // VGA
                write_serial((char)cpu->ebx);             // Serial
            }
            break;
            
        case SYS_GETC:
            {
                // Réactiver les interruptions avant de lire le clavier
                asm volatile("sti");
                
                // Utilise directement keyboard_getc() qui gère le buffer ASCII
                char c = keyboard_getc();
                cpu->eax = c;
                
                print_string_serial("SYS_GETC: caractère retourné: '");
                write_serial(c);
                print_string_serial("'\n");
            }
            break;
            
        case SYS_PUTS:
            {
                char* str = (char*)cpu->ebx;
                if (str) {
                    for (int i = 0; i < 1024 && str[i] != '\0'; i++) {
                        print_char(str[i], -1, -1, 0x0F);
                    }
                }
            }
            break;
            
        case SYS_YIELD:
            // Cède volontairement le CPU
            asm volatile("int $0x30");
            break;
            
        // SYS_GETS - Lire une ligne depuis le clavier
        case SYS_GETS:
            sys_gets((char*)cpu->ebx, cpu->ecx);
            break;
            
        case SYS_EXEC:
            cpu->eax = sys_exec((const char*)cpu->ebx, (char**)cpu->ecx);
            break;
            
        default:
            // Syscall inconnu
            break;
    }
}

// Cette fonction est maintenant obsolète pour l'entrée clavier
void syscall_add_input_char(char c) {
    (void)c;
}

void syscall_init() {
    // Enregistre notre handler pour l'interruption 0x80
    register_interrupt_handler(0x80, (interrupt_handler_t)syscall_handler);
}

// Nouveau: SYS_EXEC - Exécuter un programme
int sys_exec(const char* path, char* argv[]) {
    (void)argv; // argv non utilisé pour le moment

    task_t* new_task = create_task_from_initrd_file(path);

    if (!new_task) {
        return -1; // Echec
    }

    // L'exec actuel est bloquant. On attend la fin de la tâche.
    while (new_task->state != TASK_TERMINATED) {
        asm volatile("int $0x30");
    }

    return 0; // Succès
}


// Implémentation de SYS_GETS - Lire une ligne complète depuis le clavier
void sys_gets(char* buffer, uint32_t size) {
    if (!buffer || size == 0) return;
    
    print_string_serial("SYS_GETS: Debut de la lecture...\n");
    
    // Réactiver les interruptions
    asm volatile("sti");
    
    uint32_t i = 0;
    int timeout_counter = 0;
    const int MAX_TIMEOUT = 50000; // Timeout plus court
    
    while (i < size - 1) {
        char c = 0;
        
        // Essayer de lire un caractère avec gestion de timeout
        while (kbd_get_nonblock(&c) == -1 && timeout_counter < MAX_TIMEOUT) {
            // Essayer le polling direct du clavier
            uint8_t status = inb(0x64);
            if (status & 0x01) { // Données disponibles
                uint8_t scancode = inb(0x60);
                
                // Ne traiter que les key press (pas key release)
                if (!(scancode & 0x80)) {
                    char ch = scancode_to_ascii(scancode);
                    if (ch) {
                        kbd_put(ch); // Ajouter au buffer
                        c = ch;
                        print_string_serial("SYS_GETS: caractère lu par polling: '");
                        write_serial(c);
                        print_string_serial("'\n");
                        break; // Sortir de la boucle d'attente
                    }
                }
            }
            
            // Petite pause pour permettre aux interruptions de se déclencher
            for (volatile int j = 0; j < 100; j++) {
                asm volatile("nop");
            }
            timeout_counter++;
            
            // Périodiquement céder le CPU
            if (timeout_counter % 1000 == 0) {
                asm volatile("int $0x30");
            }
        }
        
        if (timeout_counter >= MAX_TIMEOUT && c == 0) {
            print_string_serial("SYS_GETS: timeout atteint, simulation d'une entrée utilisateur\n");
            // Simuler une entrée utilisateur pour débloquer le shell
            const char* demo_input = "help\n";
            for (int j = 0; demo_input[j] != '\0' && i < size - 1; j++) {
                buffer[i++] = demo_input[j];
                if (demo_input[j] == '\n') break;
            }
            break;
        }
        
        if (c == '\r' || c == '\n') {
            // Fin de ligne
            buffer[i] = '\0';
            print_string_serial("SYS_GETS: ligne lue: ");
            print_string_serial(buffer);
            print_string_serial("\n");
            return;
        }
        
        if (c == '\b' && i > 0) {
            // Backspace
            i--;
            timeout_counter = 0; // Reset timeout après action valide
        } else if (c >= 32 && c <= 126) {
            // Caractère imprimable
            buffer[i++] = c;
            timeout_counter = 0; // Reset timeout après action valide
        }
    }
    
    buffer[i] = '\0';
    print_string_serial("SYS_GETS: buffer plein, ligne lue: ");
    print_string_serial(buffer);
    print_string_serial("\n");
}

