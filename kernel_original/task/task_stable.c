#include "task.h"
#include "../mem/pmm.h"
#include "../mem/vmm.h"
#include "../elf.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

// Fonctions externes
extern void print_string_serial(const char* str);
extern page_directory_t* kernel_directory;

// Variables globales
task_t* current_task = NULL;
task_t* task_queue = NULL;
int next_task_id = 0;

// Version simplifiée et stable du système de tâches
void tasking_init() {
    print_string_serial("Initialisation du systeme de taches (version stable)...\n");
    
    // Alloue la tâche kernel principale
    current_task = (task_t*)pmm_alloc_page();
    if (!current_task) {
        print_string_serial("ERREUR: Impossible d'allouer la tache kernel\n");
        return;
    }
    
    // Initialise la tâche kernel
    current_task->id = next_task_id++;
    current_task->state = TASK_RUNNING;
    current_task->type = TASK_TYPE_KERNEL;
    current_task->page_directory = (uint32_t*)kernel_directory;
    current_task->stack_base = 0; // Le kernel utilise sa pile actuelle
    current_task->stack_size = 0;
    current_task->next = current_task; // Pointe vers elle-même
    current_task->prev = current_task;
    
    // Initialise l'état CPU (basique)
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
    
    print_string_serial("Tache kernel creee avec ID ");
    char id_str[16];
    int temp = current_task->id;
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

// Crée une nouvelle tâche kernel (version simplifiée)
task_t* create_task(void (*entry_point)()) {
    if (!entry_point) {
        print_string_serial("ERREUR: Point d'entree invalide\n");
        return NULL;
    }
    
    // Alloue la structure de tâche
    task_t* new_task = (task_t*)pmm_alloc_page();
    if (!new_task) {
        print_string_serial("ERREUR: Impossible d'allouer une tache\n");
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

// Ordonnanceur simplifié (sans changement de contexte pour la stabilité)
void schedule() {
    if (!current_task) {
        return;
    }
    
    // Pour la stabilité, on évite le changement de contexte complet
    // On se contente de marquer les tâches comme actives
    static int task_counter = 0;
    task_counter++;
    
    // Affiche un indicateur de vie du scheduler toutes les 100 fois
    if (task_counter % 100 == 0) {
        // Affiche un point dans le coin pour montrer que le scheduler fonctionne
        // (sera visible sur l'écran VGA)
    }
    
    // Gestion basique des tâches terminées
    if (current_task->state == TASK_TERMINATED) {
        print_string_serial("Tache terminee detectee\n");
        // Pour l'instant, on reste sur la tâche kernel principale
        if (current_task->next && current_task->next != current_task) {
            current_task = current_task->next;
            current_task->state = TASK_RUNNING;
        }
    }
}

// Termine la tâche actuelle
void task_exit() {
    if (current_task) {
        current_task->state = TASK_TERMINATED;
        print_string_serial("Tache marquee comme terminee\n");
        schedule(); // Essaie de passer à une autre tâche
    }
}

// Cède volontairement le CPU
void task_yield() {
    schedule();
}

// Fonctions pour les tâches utilisateur (stubs pour la compatibilité)
task_t* create_user_task(uint32_t entry_point) {
    print_string_serial("Creation de tache utilisateur (stub)\n");
    return NULL; // Pas implémenté dans la version stable
}

task_t* load_elf_task(uint8_t* elf_data, uint32_t size) {
    print_string_serial("Chargement ELF (stub)\n");
    return NULL; // Pas implémenté dans la version stable
}

// Fonction utilitaire
task_t* get_task_by_id(int id) {
    if (!task_queue) return NULL;
    
    task_t* task = task_queue;
    do {
        if (task->id == id) {
            return task;
        }
        task = task->next;
    } while (task != task_queue);
    
    return NULL;
}

