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
                // Utilise directement keyboard_getc() qui gère le buffer ASCII
                char c = keyboard_getc();
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
            
        // SYS_GETS - Lire une ligne depuis le clavier
        case SYS_GETS:
            cpu->eax = sys_gets((char*)cpu->ebx, cpu->ecx);
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


#include <stddef.h> // For size_t

// Erreur standard pour les pointeurs invalides
#define EFAULT 14

// Implémentation de SYS_GETS - Lire une ligne complète depuis le clavier
// Version bloquante utilisant 'hlt' pour attendre les interruptions clavier.
int sys_gets(char __user *dst, size_t maxlen) {
    // Buffer temporaire dans le noyau pour assembler la ligne
    char buf[256];
    size_t i = 0;

    if (maxlen == 0) return 0;
    if (maxlen > sizeof(buf)) maxlen = sizeof(buf);

    // Assure que les interruptions sont activées pour que 'hlt' puisse être réveillé
    asm volatile("sti");

    for (;;) {
        // Traiter les caractères déjà présents dans le buffer
        while (!kbd_empty() && i < maxlen - 1) {
            char c = kbd_pop();

            if (c == '\r') c = '\n'; // Traiter CR comme LF
            if (c == '\n') {
                goto done; // Fin de la ligne
            }
            // Gérer le backspace
            else if (c == '\b') {
                if (i > 0) i--;
            }
            // Caractère imprimable
            else if (c >= ' ') {
                buf[i++] = c;
            }
        }

        // Si le buffer est plein ou la ligne est terminée, sortir
        if (i >= maxlen - 1) {
            break;
        }

        // Si le buffer est vide, attendre la prochaine interruption clavier
        asm volatile("hlt");
    }

done:
    buf[i] = '\0'; // Terminer la chaîne de caractères

    // Copier le buffer local du noyau vers l'espace utilisateur en toute sécurité
    if (copy_to_user(dst, buf, i + 1) != 0) {
        return -EFAULT; // Erreur de copie
    }

    return (int)i; // Retourne le nombre de caractères lus
}

