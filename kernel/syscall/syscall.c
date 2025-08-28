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
// Serial I/O helpers (defined in kernel.c)
extern int is_receive_ready();
extern char read_serial();


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
                
                // Lecture clavier
                char kc = keyboard_getc();
                if (kc == 0) {
                    // Fallback: lecture depuis la console série (stdin de QEMU)
                    if (is_receive_ready()) {
                        char sc = read_serial();
                        cpu->eax = (uint32_t)sc;
                    } else {
                        cpu->eax = 0;
                    }
                } else {
                    cpu->eax = (uint32_t)kc;
                }
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
// ATTENTION: Cette fonction est maintenant dépréciée pour l'input complexe.
// Elle ne gère que les caractères ASCII et ne fonctionnera pas correctement avec l'UTF-8.
// Le shell moderne doit utiliser SYS_GETC et gérer l'édition de ligne en userspace.
void sys_gets(char* buffer, uint32_t size) {
    if (!buffer || size == 0) return;
    
    print_string_serial("SYS_GETS: Debut de la lecture (version Unicode-legacy)...\n");
    
    // Réactiver les interruptions
    asm volatile("sti");
    
    uint32_t i = 0;
    
    while (i < size - 1) {
        // Essayer d'abord le clavier
        uint32_t c_unicode = (uint32_t)keyboard_getc();
        char c = (char)c_unicode;
        
        // Fallback série si rien au clavier
        if (c == 0) {
            if (is_receive_ready()) {
                c = read_serial();
                c_unicode = (uint32_t)c;
            } else {
                continue; // attendre une vraie entrée
            }
        }

        // On ne traite que les cas simples pour assurer la compilation.
        if (c_unicode > 127) {
            // Ignorer les caractères non-ASCII pour cette fonction legacy.
            continue;
        }
        
        if (c == '\r' || c == '\n') {
            // Fin de ligne
            print_char('\n', -1, -1, 0x0F);
            buffer[i] = '\0';
            return;
        }
        
        if (c == '\b' && i > 0) {
            // Backspace
            i--;
            print_char('\b', -1, -1, 0x0F);
            print_char(' ', -1, -1, 0x0F);
            print_char('\b', -1, -1, 0x0F);
        } else if (c >= 32 && c <= 126) {
            // Caractère imprimable
            buffer[i++] = c;
            print_char(c, -1, -1, 0x0F);
        }
    }
    
    buffer[i] = '\0';
}

