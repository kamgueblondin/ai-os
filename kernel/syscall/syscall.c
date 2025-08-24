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
                uint8_t scancode;
                char c = 0;
                // Boucle tant qu'on ne reçoit pas un caractère imprimable (ignore les key releases et autres)
                while (c == 0) {
                    kbd_pop_scancode(&scancode); // C'est une fonction bloquante
                    c = scancode_to_ascii(scancode);
                }
                cpu->eax = c;
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
            
        // SYS_GETS est maintenant obsolète. Le shell doit lire caractère par caractère.
        case SYS_GETS:
            // Ne fait plus rien. Pourrait retourner une erreur.
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
