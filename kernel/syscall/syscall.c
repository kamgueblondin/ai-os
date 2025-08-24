#include "syscall.h"
#include "kernel.h"
#include "../interrupts.h"
#include "../task/task.h"
#include "../elf.h"
#include "../../fs/initrd.h"
#include "../mem/string.h"

// Fonctions externes
extern void print_string_serial(const char* str);
extern void print_char(char c, int x, int y, char color);
extern unsigned char inb(unsigned short port);

// Buffer pour l'entrée clavier (simple)
static char input_buffer[256];
static int input_buffer_head = 0;
static int input_buffer_tail = 0;

// ==============================================================================
// NOUVELLE SECTION POUR LA GESTION DE L'ENTRÉE CLAVIER
// ==============================================================================

// Ajoute un caractère au buffer d'entrée (appelé par le handler clavier)
// C'est la seule fonction que le handler d'interruption doit appeler.
void keyboard_add_char_to_buffer(char c) {
    // Section critique simple pour éviter les corruptions du buffer
    asm volatile("cli");
    int next_head = (input_buffer_head + 1) % 256;
    if (next_head != input_buffer_tail) {
        input_buffer[input_buffer_head] = c;
        input_buffer_head = next_head;
    }
    asm volatile("sti");
}

// Fonction interne pour lire un caractère du buffer.
// Si le buffer est vide, elle cède le CPU jusqu'à ce qu'un caractère arrive.
static char internal_sys_getc() {
    // Attend qu'un caractère soit disponible dans le buffer
    while (input_buffer_head == input_buffer_tail) {
        // Cède le CPU pour éviter une attente active (busy-waiting)
        asm volatile("int $0x30");
    }

    // Lit le caractère du buffer
    asm volatile("cli");
    char c = input_buffer[input_buffer_tail];
    input_buffer_tail = (input_buffer_tail + 1) % 256;
    asm volatile("sti");

    return c;
}

// NOUVELLE IMPLÉMENTATION ROBUSTE DE SYS_GETS
void sys_gets(char* buffer, uint32_t size) {
    if (!buffer || size == 0) {
        return;
    }

    char c;
    uint32_t i = 0;

    // Boucle jusqu'à ce que la ligne soit complète ou le buffer plein
    while (i < size - 1) {
        c = internal_sys_getc();

        // Gestion de la fin de ligne
        if (c == '\n' || c == '\r') {
            break;
        }
        // Gestion de la touche "effacer" (backspace)
        else if (c == '\b' || c == 127) {
            if (i > 0) {
                i--;
                // Fait l'écho du backspace sur la console
                print_char('\b', -1, -1, 0x0F);
            }
        }
        // Gestion des caractères normaux
        else {
            buffer[i++] = c;
            // Fait l'écho du caractère sur la console
            print_char(c, -1, -1, 0x0F);
        }
    }

    buffer[i] = '\0'; // Termine la chaîne de caractères

    // Fait l'écho du retour à la ligne
    print_char('\n', -1, -1, 0x0F);
}


// ==============================================================================
// GESTIONNAIRE D'APPELS SYSTÈME
// ==============================================================================

void syscall_handler(cpu_state_t* cpu) {
    // Réactive les interruptions pour permettre au clavier de fonctionner
    // même si un processus est bloqué dans un `sys_gets`.
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
            // Retourne un caractère du buffer d'entrée, sans bloquer
             if (input_buffer_head != input_buffer_tail) {
                cpu->eax = input_buffer[input_buffer_tail];
                input_buffer_tail = (input_buffer_tail + 1) % 256;
            } else {
                cpu->eax = 0; // Pas de caractère disponible
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
            
        case SYS_GETS: // Appelle la nouvelle fonction sys_gets
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
    // Initialise les buffers d'entrée
    input_buffer_head = 0;
    input_buffer_tail = 0;
    
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
