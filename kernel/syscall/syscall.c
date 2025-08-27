#include "syscall.h"
#include "kernel.h"
#include "../interrupts.h"
#include "../task/task.h"
#include "../keyboard.h"
#include "../elf.h"
#include "../../fs/initrd.h"
#include "../mem/string.h"

// Fonctions externes
extern void print_string_serial(const char* str);
extern void print_char(char c, int x, int y, char color);
extern void write_serial(char c);
extern volatile void* kbd_waiting;


// ==============================================================================
// FONCTIONS D'APPELS SYSTÈME INTERNES
// ==============================================================================

// Fonction de blocage interne pour obtenir un caractère
static char kernel_getc_blocking() {
    char c = 0;
    while ((c = keyboard_getc()) == 0) {
        // Le buffer est vide, on met la tâche en attente
        asm volatile("cli");
        kbd_waiting = (void*)current_task;
        current_task->state = TASK_WAITING;
        asm volatile("sti");

        // On cède le CPU, en attendant d'être réveillé par l'ISR du clavier
        asm volatile("int $0x30");
    }
    return c;
}


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
                cpu->eax = kernel_getc_blocking();
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

    // print_string_serial("SYS_GETS: Debut de la lecture...\n");
    
    uint32_t i = 0;
    while (i < size - 1) {
        char c = kernel_getc_blocking(); // Utilise la fonction de blocage interne
        
        // Gérer le retour arrière
        if (c == '\b') {
            if (i > 0) {
                i--;
                print_char('\b', -1, -1, 0x0F);
                print_char(' ', -1, -1, 0x0F);
                print_char('\b', -1, -1, 0x0F);
            }
        }
        // Gérer la fin de ligne
        else if (c == '\n' || c == '\r') {
            print_char('\n', -1, -1, 0x0F);
            break;
        }
        // Gérer les caractères imprimables
        else if (c >= ' ') {
            buffer[i++] = c;
            print_char(c, -1, -1, 0x0F); // Afficher le caractère à l'écran
        }
    }
    
    buffer[i] = '\0';
    // print_string_serial("SYS_GETS: Ligne lue: ");
    // print_string_serial(buffer);
    // print_string_serial("\n");
}

