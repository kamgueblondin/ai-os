#include "task.h"
#include "../mem/pmm.h"
#include "../mem/vmm.h"
#include "../elf.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

// Variables globales
task_t* current_task = NULL;
task_t* task_queue = NULL;
int next_task_id = 0;

// Fonctions externes
extern void print_string_serial(const char* str);
extern void context_switch(cpu_state_t* old_state, cpu_state_t* new_state);

// Initialise le système de tâches
void tasking_init() {
    print_string_serial("Initialisation du systeme de taches...\n");
    
    // Crée la première tâche, la tâche "kernel"
    current_task = (task_t*)pmm_alloc_page();
    if (!current_task) {
        print_string_serial("ERREUR: Impossible d'allouer la tache kernel\n");
        return;
    }
    
    // Initialise la tâche kernel
    current_task->id = next_task_id++;
    current_task->state = TASK_RUNNING;
    current_task->type = TASK_TYPE_KERNEL;
    current_task->page_directory = kernel_directory;
    current_task->stack_base = 0; // Le kernel utilise sa pile actuelle
    current_task->stack_size = 0;
    current_task->next = current_task;
    current_task->prev = current_task;
    
    // Initialise l'état CPU (sera rempli lors du premier changement de contexte)
    current_task->cpu_state.eax = 0;
    current_task->cpu_state.ebx = 0;
    current_task->cpu_state.ecx = 0;
    current_task->cpu_state.edx = 0;
    current_task->cpu_state.esi = 0;
    current_task->cpu_state.edi = 0;
    current_task->cpu_state.ebp = 0;
    current_task->cpu_state.eip = 0;
    current_task->cpu_state.esp = 0;
    current_task->cpu_state.eflags = 0x202; // Interruptions activées
    current_task->cpu_state.cs = 0x08; // Segment de code kernel
    current_task->cpu_state.ds = 0x10; // Segment de données kernel
    current_task->cpu_state.es = 0x10;
    current_task->cpu_state.fs = 0x10;
    current_task->cpu_state.gs = 0x10;
    current_task->cpu_state.ss = 0x10; // Segment de pile kernel
    
    task_queue = current_task;
    
    print_string_serial("Tache kernel creee avec ID 0\n");
}

// Crée une nouvelle tâche
task_t* create_task(void (*entry_point)()) {
    if (!entry_point) {
        print_string_serial("ERREUR: Point d'entree invalide\n");
        return NULL;
    }
    
    // Alloue la structure de tâche
    task_t* new_task = (task_t*)pmm_alloc_page();
    if (!new_task) {
        print_string_serial("ERREUR: Impossible d'allouer une nouvelle tache\n");
        return NULL;
    }
    
    // Alloue une pile pour la nouvelle tâche (4KB)
    void* stack = pmm_alloc_page();
    if (!stack) {
        print_string_serial("ERREUR: Impossible d'allouer la pile de la tache\n");
        pmm_free_page(new_task);
        return NULL;
    }
    
    // Initialise la tâche
    new_task->id = next_task_id++;
    new_task->state = TASK_READY;
    new_task->type = TASK_TYPE_KERNEL;
    new_task->page_directory = kernel_directory; // Pour l'instant, même espace mémoire
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
    new_task->cpu_state.eip = (uint32_t)entry_point; // Point d'entrée
    new_task->cpu_state.esp = (uint32_t)stack + 4096 - 4; // Haut de la pile
    new_task->cpu_state.eflags = 0x202; // Interruptions activées
    new_task->cpu_state.cs = 0x08; // Segment de code kernel
    new_task->cpu_state.ds = 0x10; // Segment de données kernel
    new_task->cpu_state.es = 0x10;
    new_task->cpu_state.fs = 0x10;
    new_task->cpu_state.gs = 0x10;
    new_task->cpu_state.ss = 0x10; // Segment de pile kernel
    
    // Ajoute la tâche à la file d'attente
    add_task_to_queue(new_task);
    
    // Message de debug
    print_string_serial("Nouvelle tache creee avec ID ");
    char id_str[16];
    int temp = new_task->id;
    int i = 0;
    if (temp == 0) {
        id_str[i++] = '0';
    } else {
        while (temp > 0) {
            id_str[i++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    id_str[i] = '\0';
    
    // Inverse la chaîne
    for (int j = 0; j < i / 2; j++) {
        char tmp = id_str[j];
        id_str[j] = id_str[i - 1 - j];
        id_str[i - 1 - j] = tmp;
    }
    
    print_string_serial(id_str);
    print_string_serial("\n");
    
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
    
    // Cherche une tâche prête à s'exécuter
    while (next_task != start_task) {
        if (next_task->state == TASK_READY) {
            break;
        }
        next_task = next_task->next;
    }
    
    // Si aucune tâche prête trouvée, reste sur la tâche actuelle
    if (next_task->state != TASK_READY && current_task->state == TASK_RUNNING) {
        return;
    }
    
    // Si la tâche actuelle est terminée, on doit changer
    if (current_task->state == TASK_TERMINATED) {
        // Supprime la tâche terminée de la queue
        remove_task(current_task);
        pmm_free_page((void*)current_task->stack_base);
        pmm_free_page(current_task);
        
        // Si c'était la seule tâche, on a un problème
        if (next_task == current_task) {
            print_string_serial("ERREUR: Toutes les taches sont terminees\n");
            while(1) { asm volatile("hlt"); }
        }
        
        current_task = next_task;
        current_task->state = TASK_RUNNING;
        return;
    }
    
    // Pas besoin de changer si c'est la même tâche
    if (next_task == current_task) {
        return;
    }
    
    // Sauvegarde l'état de la tâche actuelle
    task_t* old_task = current_task;
    old_task->state = TASK_READY;
    
    // Change vers la nouvelle tâche
    current_task = next_task;
    current_task->state = TASK_RUNNING;
    
    // Effectue le changement de contexte
    // Note: Cette fonction sera implémentée en assembleur
    context_switch(&old_task->cpu_state, &current_task->cpu_state);
}

// Termine la tâche actuelle
void task_exit() {
    if (current_task) {
        current_task->state = TASK_TERMINATED;
        schedule(); // Ne reviendra jamais
    }
}

// Cède volontairement le CPU
void task_yield() {
    schedule();
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

// Supprime une tâche de la file d'attente
void remove_task(task_t* task) {
    if (!task || !task_queue) {
        return;
    }
    
    if (task->next == task) {
        // C'est la seule tâche
        task_queue = NULL;
    } else {
        task->prev->next = task->next;
        task->next->prev = task->prev;
        
        if (task_queue == task) {
            task_queue = task->next;
        }
    }
}

// Obtient une tâche par son ID
task_t* get_task_by_id(int id) {
    if (!task_queue) {
        return NULL;
    }
    
    task_t* task = task_queue;
    do {
        if (task->id == id) {
            return task;
        }
        task = task->next;
    } while (task != task_queue);
    
    return NULL;
}

// Compte le nombre de tâches
int get_task_count() {
    if (!task_queue) {
        return 0;
    }
    
    int count = 0;
    task_t* task = task_queue;
    do {
        count++;
        task = task->next;
    } while (task != task_queue);
    
    return count;
}



// Crée une tâche utilisateur avec un point d'entrée spécifique
task_t* create_user_task(uint32_t entry_point) {
    if (!entry_point) {
        print_string_serial("ERREUR: Point d'entree utilisateur invalide\n");
        return NULL;
    }
    
    // Alloue la structure de tâche
    task_t* new_task = (task_t*)pmm_alloc_page();
    if (!new_task) {
        print_string_serial("ERREUR: Impossible d'allouer une tache utilisateur\n");
        return NULL;
    }
    
    // Alloue une pile pour la nouvelle tâche (4KB)
    void* stack = pmm_alloc_page();
    if (!stack) {
        print_string_serial("ERREUR: Impossible d'allouer la pile de la tache utilisateur\n");
        pmm_free_page(new_task);
        return NULL;
    }
    
    // Initialise la tâche utilisateur
    new_task->id = next_task_id++;
    new_task->state = TASK_READY;
    new_task->type = TASK_TYPE_USER;
    new_task->page_directory = kernel_directory; // Pour l'instant, même espace mémoire
    new_task->stack_base = (uint32_t)stack;
    new_task->stack_size = 4096;
    
    // Initialise l'état CPU pour l'espace utilisateur
    new_task->cpu_state.eax = 0;
    new_task->cpu_state.ebx = 0;
    new_task->cpu_state.ecx = 0;
    new_task->cpu_state.edx = 0;
    new_task->cpu_state.esi = 0;
    new_task->cpu_state.edi = 0;
    new_task->cpu_state.ebp = 0;
    new_task->cpu_state.eip = entry_point; // Point d'entrée utilisateur
    new_task->cpu_state.esp = (uint32_t)stack + 4096 - 4; // Haut de la pile
    new_task->cpu_state.eflags = 0x202; // Interruptions activées
    new_task->cpu_state.cs = 0x1B; // Segment de code utilisateur (Ring 3)
    new_task->cpu_state.ds = 0x23; // Segment de données utilisateur (Ring 3)
    new_task->cpu_state.es = 0x23;
    new_task->cpu_state.fs = 0x23;
    new_task->cpu_state.gs = 0x23;
    new_task->cpu_state.ss = 0x23; // Segment de pile utilisateur (Ring 3)
    
    // Ajoute la tâche à la file d'attente
    add_task_to_queue(new_task);
    
    // Message de debug
    print_string_serial("Nouvelle tache utilisateur creee avec ID ");
    char id_str[16];
    int temp = new_task->id;
    int i = 0;
    if (temp == 0) {
        id_str[i++] = '0';
    } else {
        while (temp > 0) {
            id_str[i++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    id_str[i] = '\0';
    
    // Inverse la chaîne
    for (int j = 0; j < i / 2; j++) {
        char tmp = id_str[j];
        id_str[j] = id_str[i - 1 - j];
        id_str[i - 1 - j] = tmp;
    }
    
    print_string_serial(id_str);
    print_string_serial("\n");
    
    return new_task;
}

// Charge et crée une tâche depuis un fichier ELF
task_t* load_elf_task(uint8_t* elf_data, uint32_t size) {
    if (!elf_data || !size) {
        print_string_serial("ERREUR: Donnees ELF invalides\n");
        return NULL;
    }
    
    // Charge l'exécutable ELF
    uint32_t entry_point = elf_load(elf_data, size);
    if (!entry_point) {
        print_string_serial("ERREUR: Impossible de charger l'executable ELF\n");
        return NULL;
    }
    
    // Crée la tâche utilisateur
    task_t* task = create_user_task(entry_point);
    if (!task) {
        print_string_serial("ERREUR: Impossible de creer la tache ELF\n");
        return NULL;
    }
    
    print_string_serial("Tache ELF creee et prete a s'executer\n");
    return task;
}

