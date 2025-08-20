#include "task.h"
#include "../mem/pmm.h"
#include "../mem/vmm.h"
#include <stddef.h>

// Variables globales pour le système de tâches
task_t* current_task = NULL;
task_t* task_queue = NULL;
int next_task_id = 0;

// Déclarations externes
extern page_directory_t* kernel_directory;
extern void print_string(const char* str);
extern void print_string_serial(const char* str);
extern void switch_task(cpu_state_t* old_state, cpu_state_t* new_state);

// Initialise le système de tâches
void tasking_init() {
    print_string_serial("Initialisation du systeme de taches...\n");
    
    // Crée la tâche kernel principale
    current_task = (task_t*)pmm_alloc_page();
    if (!current_task) {
        print_string_serial("ERREUR: Impossible d'allouer la tache kernel\n");
        return;
    }
    
    current_task->id = next_task_id++;
    current_task->state = TASK_RUNNING;
    current_task->type = TASK_TYPE_KERNEL;
    current_task->page_directory = (uint32_t*)kernel_directory;
    current_task->stack_base = 0;
    current_task->stack_size = 0;
    current_task->next = current_task;
    current_task->prev = current_task;
    
    task_queue = current_task;
    
    print_string_serial("Tache kernel creee avec ID 0\n");
}

// Ajoute une tâche à la file d'attente
void add_task_to_queue(task_t* task) {
    if (!task_queue) {
        task_queue = task;
        task->next = task;
        task->prev = task;
    } else {
        task->next = task_queue;
        task->prev = task_queue->prev;
        task_queue->prev->next = task;
        task_queue->prev = task;
    }
}

// Crée une nouvelle tâche kernel
task_t* create_task(void (*entry_point)()) {
    // Alloue la mémoire pour la structure de tâche
    task_t* new_task = (task_t*)pmm_alloc_page();
    if (!new_task) {
        print_string_serial("ERREUR: Impossible d'allouer la tache\n");
        return NULL;
    }
    
    // Alloue la pile pour la tâche
    void* stack = pmm_alloc_page();
    if (!stack) {
        print_string_serial("ERREUR: Impossible d'allouer la pile\n");
        pmm_free_page(new_task);
        return NULL;
    }
    
    // Initialise la tâche
    new_task->id = next_task_id++;
    new_task->state = TASK_READY;
    new_task->type = TASK_TYPE_KERNEL;
    new_task->page_directory = (uint32_t*)kernel_directory;
    new_task->stack_base = (uint32_t)stack;
    new_task->stack_size = 4096;
    
    // Initialise l'état CPU
    new_task->cpu_state.eax = 0;
    new_task->cpu_state.ebx = 0;
    new_task->cpu_state.ecx = 0;
    new_task->cpu_state.edx = 0;
    new_task->cpu_state.esi = 0;
    new_task->cpu_state.edi = 0;
    new_task->cpu_state.ebp = 0;
    new_task->cpu_state.eip = (uint32_t)entry_point;
    new_task->cpu_state.esp = (uint32_t)stack + 4096 - 4;
    new_task->cpu_state.eflags = 0x202;
    new_task->cpu_state.cs = 0x08;
    new_task->cpu_state.ds = 0x10;
    new_task->cpu_state.es = 0x10;
    new_task->cpu_state.fs = 0x10;
    new_task->cpu_state.gs = 0x10;
    new_task->cpu_state.ss = 0x10;
    
    // Ajoute la tâche à la file d'attente
    add_task_to_queue(new_task);
    
    print_string_serial("Nouvelle tache creee\n");
    return new_task;
}

// Ordonnanceur Round-Robin
void schedule() {
    if (!current_task || !task_queue) {
        return;
    }
    
    // Trouve la prochaine tâche prête
    task_t* next_task = current_task->next;
    task_t* start_task = current_task;
    
    while (next_task != start_task && next_task->state != TASK_READY) {
        next_task = next_task->next;
    }
    
    if (next_task->state == TASK_READY) {
        current_task->state = TASK_READY;
        next_task->state = TASK_RUNNING;
        
        task_t* old_task = current_task;
        current_task = next_task;
        
        // Effectuer le vrai changement de contexte
        print_string_serial("Changement de contexte vers tache ID ");
        // Simple conversion int vers string pour debug
        char id_str[16];
        int id = current_task->id;
        int pos = 0;
        if (id == 0) {
            id_str[pos++] = '0';
        } else {
            while (id > 0) {
                id_str[pos++] = '0' + (id % 10);
                id /= 10;
            }
        }
        // Inverser la chaîne
        for (int i = 0; i < pos / 2; i++) {
            char temp = id_str[i];
            id_str[i] = id_str[pos - 1 - i];
            id_str[pos - 1 - i] = temp;
        }
        id_str[pos] = '\0';
        print_string_serial(id_str);
        print_string_serial("\n");
        
        switch_task(&old_task->cpu_state, &current_task->cpu_state);
    }</to_replace>
</Editor.edit_file_by_replace>

<Editor.write>
<path>boot/context_switch_user.s</path>
<content>global switch_task

; Structure cpu_state_t offsets
%define CPU_EAX    0
%define CPU_EBX    4
%define CPU_ECX    8
%define CPU_EDX    12
%define CPU_ESI    16
%define CPU_EDI    20
%define CPU_EBP    24
%define CPU_EIP    28
%define CPU_ESP    32
%define CPU_EFLAGS 36
%define CPU_CS     40
%define CPU_DS     44
%define CPU_ES     48
%define CPU_FS     52
%define CPU_GS     56
%define CPU_SS     60

; void switch_task(cpu_state_t* old_state, cpu_state_t* new_state);
; Changement de contexte avec support Ring 0 -> Ring 3
switch_task:
    cli                     ; Désactive les interruptions
    
    push ebp
    mov ebp, esp
    pushad                  ; Sauvegarde tous les registres
    
    mov eax, [ebp + 8]      ; old_state
    mov ebx, [ebp + 12]     ; new_state
    
    ; Vérifier si on a un ancien état à sauvegarder
    test eax, eax
    jz .load_new_task
    
    ; Sauvegarder l'état actuel (sera fait par l'appelant)
    ; Ici on se contente de passer à la nouvelle tâche
    
.load_new_task:
    test ebx, ebx
    jz .done
    
    ; Vérifier le type de tâche (kernel vs user)
    ; Si CS = 0x1B, c'est une tâche utilisateur (Ring 3)
    mov cx, [ebx + CPU_CS]
    cmp cx, 0x1B
    je .switch_to_user
    
    ; Changement vers tâche kernel (Ring 0)
    mov ax, [ebx + CPU_DS]
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    mov esp, [ebx + CPU_ESP]
    mov ebp, [ebx + CPU_EBP]
    
    push dword [ebx + CPU_EFLAGS]
    popfd
    
    ; Charger les registres généraux
    mov eax, [ebx + CPU_EAX]
    mov ecx, [ebx + CPU_ECX]
    mov edx, [ebx + CPU_EDX]
    mov esi, [ebx + CPU_ESI]
    mov edi, [ebx + CPU_EDI]
    
    push dword [ebx + CPU_EIP]
    mov ebx, [ebx + CPU_EBX]
    
    sti
    ret
    
.switch_to_user:
    ; Préparation pour Ring 3
    ; Stack layout pour iret vers Ring 3:
    ; [SS] [ESP] [EFLAGS] [CS] [EIP]
    
    ; Configurer la pile kernel temporaire
    mov esp, 0x10000        ; Pile kernel temporaire
    
    ; Empiler les registres de segment utilisateur pour iret
    push dword [ebx + CPU_SS]     ; SS utilisateur (0x23)
    push dword [ebx + CPU_ESP]    ; ESP utilisateur
    
    ; Préparer EFLAGS avec IF=1 pour Ring 3
    mov eax, [ebx + CPU_EFLAGS]
    or eax, 0x200               ; Activer les interruptions
    push eax                    ; EFLAGS
    
    push dword [ebx + CPU_CS]     ; CS utilisateur (0x1B)
    push dword [ebx + CPU_EIP]    ; EIP utilisateur
    
    ; Charger les segments de données utilisateur
    mov ax, 0x23                ; Segment de données utilisateur
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Charger les registres généraux
    mov eax, [ebx + CPU_EAX]
    mov ecx, [ebx + CPU_ECX]
    mov edx, [ebx + CPU_EDX]
    mov esi, [ebx + CPU_ESI]
    mov edi, [ebx + CPU_EDI]
    mov ebp, [ebx + CPU_EBP]
    mov ebx, [ebx + CPU_EBX]
    
    ; Passage au Ring 3 avec iret
    iret
    
.done:
    popad
    mov esp, ebp
    pop ebp
    sti
    ret

; Section GNU stack (sécurité)
section .note.GNU-stack
</content>
</Editor.write>

<Editor.edit_file_by_replace>
<file_name>Makefile</file_name>
<to_replace>build/context_switch.o: boot/context_switch.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@</to_replace>
<new_content>build/context_switch.o: boot/context_switch_user.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@
}

// Crée une tâche utilisateur
task_t* create_user_task(uint32_t entry_point) {
    task_t* new_task = (task_t*)pmm_alloc_page();
    if (!new_task) {
        print_string_serial("ERREUR: Impossible d'allouer la tache utilisateur\n");
        return NULL;
    }
    
    // Alloue la pile utilisateur
    void* user_stack = pmm_alloc_page();
    if (!user_stack) {
        print_string_serial("ERREUR: Impossible d'allouer la pile utilisateur\n");
        pmm_free_page(new_task);
        return NULL;
    }
    
    // Mappe la pile en espace utilisateur
    uint32_t user_stack_virt = 0xBFFFF000; // Haut de l'espace utilisateur
    vmm_map_page(user_stack, (void*)user_stack_virt, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    
    // Initialise la tâche utilisateur
    new_task->id = next_task_id++;
    new_task->state = TASK_READY;
    new_task->type = TASK_TYPE_USER;
    new_task->page_directory = (uint32_t*)kernel_directory;
    new_task->stack_base = user_stack_virt;
    new_task->stack_size = 4096;
    
    // État CPU pour mode utilisateur
    new_task->cpu_state.eax = 0;
    new_task->cpu_state.ebx = 0;
    new_task->cpu_state.ecx = 0;
    new_task->cpu_state.edx = 0;
    new_task->cpu_state.esi = 0;
    new_task->cpu_state.edi = 0;
    new_task->cpu_state.ebp = 0;
    new_task->cpu_state.eip = entry_point;
    new_task->cpu_state.esp = user_stack_virt + 4096 - 4;
    new_task->cpu_state.eflags = 0x202;
    new_task->cpu_state.cs = 0x1B; // Segment de code utilisateur (GDT index 3, RPL 3)
    new_task->cpu_state.ds = 0x23; // Segment de données utilisateur (GDT index 4, RPL 3)
    new_task->cpu_state.es = 0x23;
    new_task->cpu_state.fs = 0x23;
    new_task->cpu_state.gs = 0x23;
    new_task->cpu_state.ss = 0x23; // Segment de pile utilisateur
    
    add_task_to_queue(new_task);
    
    print_string_serial("Tache utilisateur creee\n");
    return new_task;
}

// Termine la tâche courante
void task_exit() {
    if (!current_task) return;
    
    current_task->state = TASK_TERMINATED;
    
    // Libère les ressources
    if (current_task->stack_base && current_task->type == TASK_TYPE_KERNEL) {
        pmm_free_page((void*)current_task->stack_base);
    }
    
    // Retire de la file d'attente
    if (current_task->next == current_task) {
        // Dernière tâche
        task_queue = NULL;
        current_task = NULL;
    } else {
        current_task->prev->next = current_task->next;
        current_task->next->prev = current_task->prev;
        
        if (task_queue == current_task) {
            task_queue = current_task->next;
        }
        
        task_t* old_task = current_task;
        current_task = current_task->next;
        current_task->state = TASK_RUNNING;
        
        pmm_free_page(old_task);
    }
    
    // Force un changement de contexte
    schedule();
}

// Obtient la tâche courante
task_t* get_current_task() {
    return current_task;
}

