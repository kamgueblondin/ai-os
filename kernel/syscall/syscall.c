#include "syscall.h"
#include "../interrupts.h"
#include "../task/task.h"
#include "../elf.h"
#include "../../fs/initrd.h"

// Fonctions externes
extern void print_string_serial(const char* str);
extern void print_char(char c, int x, int y, char color);
extern unsigned char inb(unsigned short port);
extern task_t* create_user_task(uint32_t entry_point);

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
    
    // Gestion spéciale pour SYS_GETS
    if (c == '\n' || c == '\r') {
        // Fin de ligne
        line_buffer[line_position] = '\0';
        line_ready = 1;
        line_position = 0;
    } else if (c == '\b' || c == 127) {
        // Backspace
        if (line_position > 0) {
            line_position--;
            // Effacer le caractère à l'écran seulement si pas en mode shell kernel
            // (évite la duplication d'écho)
            // print_char('\b', -1, -1, 0x0F);
            // print_char(' ', -1, -1, 0x0F);
            // print_char('\b', -1, -1, 0x0F);
        }
    } else if (c >= 32 && c <= 126 && line_position < 255) {
        // Caractère imprimable
        line_buffer[line_position++] = c;
        // Echo du caractère désactivé pour éviter la duplication
        // L'écho sera géré par le shell kernel directement
        // print_char(c, -1, -1, 0x0F);
    }
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
    schedule();
}

// Nouveau: SYS_GETS - Lire une ligne complète (version sans timer)
void sys_gets(char* buffer, uint32_t size) {
    if (!buffer || size == 0) {
        return;
    }
    
    print_string_serial("SYS_GETS: Attente d'entree utilisateur...\n");
    
    // Attendre qu'une ligne soit prête (version sans timer)
    while (!line_ready) {
        // Attendre une interruption (clavier principalement)
        asm volatile("hlt");
    }
    
    // Copier la ligne dans le buffer utilisateur
    int copy_len = strlen_kernel(line_buffer);
    if (copy_len >= size) {
        copy_len = size - 1;
    }
    
    for (int i = 0; i < copy_len; i++) {
        buffer[i] = line_buffer[i];
    }
    buffer[copy_len] = '\0';
    
    // Réinitialiser pour la prochaine ligne
    line_ready = 0;
    line_position = 0;
    
    print_string_serial("SYS_GETS: ligne lue: ");
    print_string_serial(buffer);
    print_string_serial("\n");
}

// Nouveau: SYS_EXEC - Exécuter un programme
int sys_exec(const char* path, char* argv[]) {
    if (!path) {
        print_string_serial("SYS_EXEC: chemin invalide\n");
        return -1;
    }
    
    print_string_serial("SYS_EXEC: tentative d'execution de ");
    print_string_serial(path);
    print_string_serial("\n");
    
    // Chercher le fichier dans l'initrd
    uint8_t* program_data = initrd_read_file(path);
    if (!program_data) {
        print_string_serial("SYS_EXEC: fichier non trouve: ");
        print_string_serial(path);
        print_string_serial("\n");
        return -1;
    }
    
    // Charger le programme ELF
    uint32_t entry_point = elf_load(program_data, 0); // Taille non utilisée pour l'instant
    if (entry_point == 0) {
        print_string_serial("SYS_EXEC: echec du chargement ELF\n");
        return -1;
    }
    
    print_string_serial("SYS_EXEC: point d'entree: 0x");
    // Affichage hexadécimal simple
    char hex_str[16];
    uint32_t addr = entry_point;
    int hex_pos = 0;
    for (int i = 7; i >= 0; i--) {
        int digit = (addr >> (i * 4)) & 0xF;
        hex_str[hex_pos++] = (digit < 10) ? ('0' + digit) : ('A' + digit - 10);
    }
    hex_str[hex_pos] = '\0';
    print_string_serial(hex_str);
    print_string_serial("\n");
    
    // Créer une nouvelle tâche utilisateur
    task_t* new_task = create_user_task(entry_point);
    if (!new_task) {
        print_string_serial("SYS_EXEC: echec de creation de tache\n");
        return -1;
    }
    
    // TODO: Passer les arguments argv à la nouvelle tâche
    // Pour l'instant, on ignore argv
    
    print_string_serial("SYS_EXEC: nouvelle tache creee avec ID ");
    char id_str[16];
    int task_id = new_task->id;
    int id_pos = 0;
    if (task_id == 0) {
        id_str[id_pos++] = '0';
    } else {
        while (task_id > 0) {
            id_str[id_pos++] = '0' + (task_id % 10);
            task_id /= 10;
        }
    }
    id_str[id_pos] = '\0';
    
    // Inverser la chaîne
    for (int j = 0; j < id_pos / 2; j++) {
        char tmp = id_str[j];
        id_str[j] = id_str[id_pos - 1 - j];
        id_str[id_pos - 1 - j] = tmp;
    }
    
    print_string_serial(id_str);
    print_string_serial("\n");
    
    // Attendre que la tâche se termine (exec bloquant simple)
    while (new_task->state != TASK_TERMINATED) {
        schedule();
    }
    
    print_string_serial("SYS_EXEC: tache terminee\n");
    return 0;
}

