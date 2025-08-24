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

// Buffer pour la saisie de ligne (SYS_GETS)
static char line_buffer[256];
static int line_position = 0;
static int line_ready = 0;

// Fonctions utilitaires internes
int strlen_kernel(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

void strcpy_kernel(char* dest, const char* src) {
    int i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

int strcmp_kernel(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] != '\0' && s2[i] != '\0') {
        if (s1[i] != s2[i]) {
            return s1[i] - s2[i];
        }
        i++;
    }
    return s1[i] - s2[i];
}

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
            asm volatile("int $0x30"); // On ne reviendra jamais à cette tâche
            break;
        
        case SYS_PUTC: // SYS_PUTC
            // L'argument (le caractère) est dans EBX
            {
                extern void write_serial(char a);
                print_char((char)cpu->ebx, -1, -1, 0x0F); // VGA
                write_serial((char)cpu->ebx);             // Serial
            }
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
            asm volatile("int $0x30");
            break;
            
        case SYS_GETS: // SYS_GETS - Nouveau
            // EBX = buffer, ECX = taille
            sys_gets((char*)cpu->ebx, cpu->ecx);
            break;
            
        case SYS_EXEC: // SYS_EXEC - Nouveau
            // EBX = path, ECX = argv
            cpu->eax = sys_exec((const char*)cpu->ebx, (char**)cpu->ecx);
            break;
            
        default:
            // Syscall inconnu
            print_string_serial("Syscall inconnu: ");
            print_hex_serial(cpu->eax);
            print_string_serial("\n");
            break;
    }
}

// Ajoute un caractère au buffer d'entrée (appelé par le handler clavier)
void syscall_add_input_char(char c) {
    // Section critique pour éviter les race conditions
    asm volatile("cli");
    
    int next_head = (input_buffer_head + 1) % 256;
    if (next_head != input_buffer_tail) {
        input_buffer[input_buffer_head] = c;
        input_buffer_head = next_head;
    }

    // Afficher le caractère immédiatement pour feedback visuel
    if (c >= 32 && c <= 126) {
        extern void print_char(char c, int x, int y, char color);
        print_char(c, -1, -1, 0x0F);
    } else if (c == '\n' || c == '\r') {
        extern void print_char(char c, int x, int y, char color);
        print_char('\n', -1, -1, 0x0F);
    } else if (c == '\b' || c == 127) {
        extern void print_char(char c, int x, int y, char color);
        print_char('\b', -1, -1, 0x0F);
    }

    // Gestion spéciale pour SYS_GETS
    if (c == '\n' || c == '\r') {
        line_buffer[line_position] = '\0';
        line_ready = 1;
        line_position = 0;

        // Réveiller la tâche qui attend l'entrée
        task_t* waiting_task = find_task_waiting_for_input();
        if (waiting_task) {
            print_string_serial("syscall_add_input_char: Tache en attente trouvee, reveil.\n");
            waiting_task->state = TASK_READY;
            // La tâche sera reprise au prochain tick du timer.
            // On ne force pas un reschedule ici pour éviter les problèmes de réentrance
            // dans le scheduler depuis un handler d'interruption.
        } else {
            print_string_serial("syscall_add_input_char: Pas de tache en attente d'entree.\n");
        }
    } else if (c == '\b' || c == 127) {
        if (line_position > 0) {
            line_position--;
        }
    } else if (c >= 32 && c <= 126 && line_position < 255) {
        line_buffer[line_position++] = c;
    }
    
    asm volatile("sti");
}

void syscall_init() {
    print_string_serial("Initialisation des appels systeme...\n");
    
    // Initialise les buffers d'entrée
    input_buffer_head = 0;
    input_buffer_tail = 0;
    line_position = 0;
    line_ready = 0;
    
    // Enregistre notre handler pour l'interruption 0x80
    register_interrupt_handler(0x80, (interrupt_handler_t)syscall_handler);
    
    print_string_serial("Appels systeme initialises.\n");
}

// Fonctions utilitaires (pour usage interne du kernel)
void sys_exit(uint32_t exit_code) {
    (void)exit_code;
    if (current_task) {
        current_task->state = TASK_TERMINATED;
        asm volatile("int $0x30");
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

// Fonction publique pour accéder au buffer clavier depuis le kernel
char keyboard_getc() {
    return sys_getc();
}

void sys_puts(const char* str) {
    if (str) {
        for (int i = 0; str[i] != '\0'; i++) {
            print_char(str[i], -1, -1, 0x0F);
        }
    }
}

void sys_yield() {
    asm volatile("int $0x30");
}

// Nouveau: SYS_GETS - Lire une ligne complète (version améliorée)
void sys_gets(char* buffer, uint32_t size) {
    if (!buffer || size == 0) {
        return;
    }

    print_string_serial("SYS_GETS: Debut de la lecture...\n");

    // Vérifier d'abord si une ligne est déjà prête
    if (line_ready) {
        print_string_serial("SYS_GETS: Ligne deja prete. Lecture immediate.\n");

        int copy_len = strlen_kernel(line_buffer);
        if ((uint32_t)copy_len >= size) {
            copy_len = size - 1;
        }
        memcpy(buffer, line_buffer, copy_len);
        buffer[copy_len] = '\0';

        line_ready = 0;
        line_position = 0;
        return;
    }

    // Pas de ligne prête, mettre la tâche en attente
    print_string_serial("SYS_GETS: Pas de ligne prete. Mise en attente de la tache...\n");
    
    if (current_task) {
        current_task->state = TASK_WAITING_FOR_INPUT;
        print_string_serial("SYS_GETS: Tache mise en attente. Cession du CPU...\n");
        
        // Céder le CPU et attendre qu'une ligne soit prête
        asm volatile("int $0x30");
        
        // Quand on arrive ici, la tâche a été réveillée
        print_string_serial("SYS_GETS: Tache reveillee, lecture de la ligne.\n");
        
        int copy_len = strlen_kernel(line_buffer);
        if ((uint32_t)copy_len >= size) {
            copy_len = size - 1;
        }
        memcpy(buffer, line_buffer, copy_len);
        buffer[copy_len] = '\0';

        // Réinitialiser pour la prochaine ligne
        line_ready = 0;
        line_position = 0;
        
        print_string_serial("SYS_GETS: Lecture terminee.\n");
    } else {
        print_string_serial("SYS_GETS: ERREUR - Pas de tache courante!\n");
    }
}

// Nouveau: SYS_EXEC - Exécuter un programme
int sys_exec(const char* path, char* argv[]) {
    (void)argv; // argv non utilisé pour le moment

    print_string_serial("SYS_EXEC: Execution de '");
    print_string_serial(path);
    print_string_serial("'\n");

    task_t* new_task = create_task_from_initrd_file(path);

    if (!new_task) {
        print_string_serial("SYS_EXEC: Echec de la creation de la tache.\n");
        return -1; // Echec
    }

    // L'exec actuel est bloquant. On attend la fin de la tâche.
    // Un vrai exec remplacerait le processus courant, mais c'est plus simple.
    while (new_task->state != TASK_TERMINATED) {
        asm volatile("int $0x30");
    }

    print_string_serial("SYS_EXEC: Tache terminee.\n");
    return 0; // Succès
}

