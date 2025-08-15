#include "syscall.h"
#include "../interrupts.h"
#include "../task/task.h"

// Fonctions externes
extern void print_string_serial(const char* str);
extern void print_char(char c, int x, int y, char color);
extern unsigned char inb(unsigned short port);

// Buffer pour l'entrée clavier (simple)
static char input_buffer[256];
static int input_buffer_head = 0;
static int input_buffer_tail = 0;

// Le handler C appelé par l'ISR de l'int 0x80
void syscall_handler(cpu_state_t* cpu) {
    // Le numéro de syscall est dans le registre EAX
    switch (cpu->eax) {
        case SYS_EXIT: // SYS_EXIT
            print_string_serial("Processus termine avec code ");
            // Affichage simple du code de sortie
            char code_str[16];
            uint32_t code = cpu->ebx;
            int i = 0;
            if (code == 0) {
                code_str[i++] = '0';
            } else {
                while (code > 0) {
                    code_str[i++] = '0' + (code % 10);
                    code /= 10;
                }
            }
            code_str[i] = '\0';
            
            // Inverse la chaîne
            for (int j = 0; j < i / 2; j++) {
                char tmp = code_str[j];
                code_str[j] = code_str[i - 1 - j];
                code_str[i - 1 - j] = tmp;
            }
            
            print_string_serial(code_str);
            print_string_serial("\n");
            
            current_task->state = TASK_TERMINATED;
            schedule(); // On ne reviendra jamais à cette tâche
            break;
        
        case SYS_PUTC: // SYS_PUTC
            // L'argument (le caractère) est dans EBX
            print_char((char)cpu->ebx, -1, -1, 0x0F);
            break;
            
        case SYS_GETC: // SYS_GETC
            // Retourne un caractère depuis le buffer d'entrée
            if (input_buffer_head != input_buffer_tail) {
                cpu->eax = input_buffer[input_buffer_tail];
                input_buffer_tail = (input_buffer_tail + 1) % 256;
            } else {
                cpu->eax = 0; // Pas de caractère disponible
            }
            break;
            
        case SYS_PUTS: // SYS_PUTS
            // L'adresse de la chaîne est dans EBX
            // Note: Ceci est dangereux sans protection mémoire appropriée
            {
                char* str = (char*)cpu->ebx;
                if (str) {
                    // Limite la longueur pour éviter les boucles infinies
                    for (int i = 0; i < 1024 && str[i] != '\0'; i++) {
                        print_char(str[i], -1, -1, 0x0F);
                    }
                }
            }
            break;
            
        case SYS_YIELD: // SYS_YIELD
            // Cède volontairement le CPU
            schedule();
            break;
            
        default:
            // Syscall inconnu
            print_string_serial("Syscall inconnu: ");
            char syscall_str[16];
            uint32_t syscall_num = cpu->eax;
            int k = 0;
            if (syscall_num == 0) {
                syscall_str[k++] = '0';
            } else {
                while (syscall_num > 0) {
                    syscall_str[k++] = '0' + (syscall_num % 10);
                    syscall_num /= 10;
                }
            }
            syscall_str[k] = '\0';
            
            // Inverse la chaîne
            for (int j = 0; j < k / 2; j++) {
                char tmp = syscall_str[j];
                syscall_str[j] = syscall_str[k - 1 - j];
                syscall_str[k - 1 - j] = tmp;
            }
            
            print_string_serial(syscall_str);
            print_string_serial("\n");
            break;
    }
}

// Ajoute un caractère au buffer d'entrée (appelé par le handler clavier)
void syscall_add_input_char(char c) {
    int next_head = (input_buffer_head + 1) % 256;
    if (next_head != input_buffer_tail) {
        input_buffer[input_buffer_head] = c;
        input_buffer_head = next_head;
    }
}

void syscall_init() {
    print_string_serial("Initialisation des appels systeme...\n");
    
    // Initialise le buffer d'entrée
    input_buffer_head = 0;
    input_buffer_tail = 0;
    
    // Enregistre notre handler pour l'interruption 0x80
    register_interrupt_handler(0x80, (interrupt_handler_t)syscall_handler);
    
    print_string_serial("Appels systeme initialises.\n");
}

// Fonctions utilitaires (pour usage interne du kernel)
void sys_exit(uint32_t exit_code) {
    if (current_task) {
        current_task->state = TASK_TERMINATED;
        schedule();
    }
}

void sys_putc(char c) {
    print_char(c, -1, -1, 0x0F);
}

char sys_getc() {
    if (input_buffer_head != input_buffer_tail) {
        char c = input_buffer[input_buffer_tail];
        input_buffer_tail = (input_buffer_tail + 1) % 256;
        return c;
    }
    return 0;
}

void sys_puts(const char* str) {
    if (str) {
        for (int i = 0; str[i] != '\0'; i++) {
            print_char(str[i], -1, -1, 0x0F);
        }
    }
}

void sys_yield() {
    schedule();
}

