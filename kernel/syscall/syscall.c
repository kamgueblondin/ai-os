#include "syscall.h"
#include "kernel.h"
#include "../interrupts.h"
#include "../task/task.h"
#include "../keyboard.h"
#include "../elf.h"
#include "../../fs/initrd.h"
#include "../mem/string.h"
#include "../mem/vmm.h"
// Externs VMM
extern vmm_directory_t* current_directory;
extern void vmm_switch_page_directory(uint32_t phys_addr);

// Fonctions externes
extern void print_string_serial(const char* str);
extern void print_char(char c, int x, int y, char color);
extern void write_serial(char c);


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
            print_string_serial("[EXIT] task terminated, scheduling...\n");
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
                
                // Lecture clavier (ASCII)
                char c = keyboard_getc();
                cpu->eax = c;
                
                // Log uniquement si caractère non nul pour éviter le bruit
                if (c != 0) {
                    print_string_serial("SYS_GETC: caractère retourné: '");
                    write_serial(c);
                    print_string_serial("'\n");
                }
            }
            break;
            
        case SYS_PUTS:
            {
                char* str = (char*)cpu->ebx;
                if (str) {
                    for (int i = 0; i < 1024 && str[i] != '\0'; i++) {
                        char ch = str[i];
                        // Filtrer les non-imprimables (sauf \n, \r, \t)
                        if ((ch >= 32 && ch <= 126) || ch == '\n' || ch == '\r' || ch == '\t') {
                            print_char(ch, -1, -1, 0x0F);
                        }
                    }
                    // Garantir un flush visuel minimal
                    print_char('\n', -1, -1, 0x0F);
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
            print_string_serial("[EXEC] starting child\n");
            cpu->eax = sys_exec((const char*)cpu->ebx, (char**)cpu->ecx);
            print_string_serial("[EXEC] child finished\n");
            break;
        case SYS_SPAWN:
            print_string_serial("[SPAWN] starting child\n");
            cpu->eax = sys_spawn((const char*)cpu->ebx, (char**)cpu->ecx);
            print_string_serial("[SPAWN] child created\n");
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
    task_t* new_task = create_task_from_initrd_file(path);

    if (!new_task) {
        return -1; // Echec
    }

    // Passer premier argument (question) comme pour spawn
    if (argv) {
        char** argv_list = (char**)argv;
        const char* src = 0;
        if (argv_list[1]) src = argv_list[1];
        else if (argv_list[0]) src = argv_list[0];
        if (src) {
            char kbuf[256];
            int n = 0;
            while (n < 255 && src[n] != '\0') { kbuf[n] = src[n]; n++; }
            kbuf[n] = '\0';
            extern vmm_directory_t* current_directory;
            vmm_directory_t* old_dir = current_directory;
            vmm_switch_page_directory(new_task->vmm_dir->physical_addr);
            current_directory = new_task->vmm_dir;
            char* dst = (char*)(0xB0000000 - 512);
            for (int i = 0; i <= n; i++) dst[i] = kbuf[i];
            vmm_switch_page_directory(old_dir->physical_addr);
            current_directory = old_dir;
            new_task->cpu_state.ebx = (uint32_t)(0xB0000000 - 512);
        }
    }
    // Demander un reschedule immediat
    extern volatile int g_reschedule_needed;
    g_reschedule_needed = 1;

    // L'exec actuel est bloquant. On attend la fin de la tâche.
    while (new_task->state != TASK_TERMINATED) {
        asm volatile("int $0x30");
    }

    return 0; // Succès
}

// Non-bloquant: cree la tache et retourne immediatement 0 si ok, -1 sinon
int sys_spawn(const char* path, char* argv[]) {
    task_t* new_task = create_task_from_initrd_file(path);
    if (!new_task) {
        return -1;
    }
    // Passer au moins un argument texte (preferer argv[1] si present)
    if (argv) {
        char** argv_list = (char**)argv;
        const char* src = 0;
        if (argv_list[1]) src = argv_list[1];
        else if (argv_list[0]) src = argv_list[0];
        if (src) {
            // Copier jusqu'a 255 octets
            char kbuf[256];
            int n = 0;
            while (n < 255 && src[n] != '\0') { kbuf[n] = src[n]; n++; }
            kbuf[n] = '\0';
            // Ecrire dans la pile utilisateur de la nouvelle tache (en haut - 512)
            vmm_directory_t* old_dir = current_directory;
            vmm_switch_page_directory(new_task->vmm_dir->physical_addr);
            current_directory = new_task->vmm_dir;
            char* dst = (char*)(0xB0000000 - 512);
            for (int i = 0; i <= n; i++) dst[i] = kbuf[i];
            // Restaurer
            vmm_switch_page_directory(old_dir->physical_addr);
            current_directory = old_dir;
            // Placer le pointeur dans EBX
            new_task->cpu_state.ebx = (uint32_t)(0xB0000000 - 512);
        }
    }
    // Demander un reschedule immediat pour afficher rapidement la sortie
    extern volatile int g_reschedule_needed;
    g_reschedule_needed = 1;
    return 0;
}


// Implémentation de SYS_GETS - Lire une ligne complète depuis le clavier
void sys_gets(char* buffer, uint32_t size) {
    if (!buffer || size == 0) return;
    
    print_string_serial("SYS_GETS: Debut de la lecture (version corrigee)...\n");
    
    // Réactiver les interruptions
    asm volatile("sti");
    
    uint32_t i = 0;
    
    while (i < size - 1) {
        char c = keyboard_getc(); // Utilise directement keyboard_getc qui est plus robuste
        
        if (c == '\r' || c == '\n') {
            // Fin de ligne - afficher aussi sur écran
            print_char('\n', -1, -1, 0x0F);
            buffer[i] = '\0';
            print_string_serial("SYS_GETS: ligne lue: ");
            print_string_serial(buffer);
            print_string_serial("\n");
            return;
        }
        
        if (c == '\b' && i > 0) {
            // Backspace - effacer sur l'écran aussi
            i--;
            print_char('\b', -1, -1, 0x0F);  // Backspace
            print_char(' ', -1, -1, 0x0F);   // Espace
            print_char('\b', -1, -1, 0x0F);  // Backspace
        } else if (c >= 32 && c <= 126) {
            // Caractère imprimable - l'afficher sur l'écran
            buffer[i++] = c;
            print_char(c, -1, -1, 0x0F);
            print_string_serial("SYS_GETS: caractère ajouté: '");
            write_serial(c);
            print_string_serial("'\n");
        }
    }
    
    buffer[i] = '\0';
    print_string_serial("SYS_GETS: buffer plein, ligne lue: ");
    print_string_serial(buffer);
    print_string_serial("\n");
}

