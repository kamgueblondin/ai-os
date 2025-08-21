#include "vmm.h"
#include "pmm.h"

// Variables globales pour la gestion de la mémoire virtuelle
page_directory_t *kernel_directory = 0;
page_directory_t *current_directory = 0;

// Fonction utilitaire pour obtenir l'offset dans la page
static uint32_t get_page_offset(uint32_t virtual_addr) {
    return virtual_addr & 0xFFF;
}

// Initialise le gestionnaire de mémoire virtuelle
void vmm_init() {
    // Alloue de la mémoire pour le répertoire de pages du noyau
    // Note: on utilise pmm_alloc_page car kmalloc n'est pas encore prêt.
    kernel_directory = (page_directory_t*)pmm_alloc_page();
    if (!kernel_directory) {
        // Gérer l'erreur critique: impossible d'allouer le répertoire du noyau
        return;
    }
    // Met à zéro le répertoire
    for (int i = 0; i < 1024; i++) {
        kernel_directory->tables[i] = 0;
        kernel_directory->tablesPhysical[i] = 0x00000002; // Non présent, R/W, Sup
    }

    // Alloue une table de pages pour les premiers 4 Mo du noyau
    page_table_t* kernel_page_table = (page_table_t*)pmm_alloc_page();
    if (!kernel_page_table) {
        // Gérer l'erreur critique
        return;
    }

    // Mappe les premiers 4 Mo en identité (virtuel = physique)
    for (int i = 0; i < 1024; i++) {
        kernel_page_table->pages[i].present = 1;
        kernel_page_table->pages[i].rw = 1;
        kernel_page_table->pages[i].user = 0; // Kernel-mode
        kernel_page_table->pages[i].frame = i;
    }

    // Enregistre la table de pages dans le répertoire du noyau
    kernel_directory->tables[0] = kernel_page_table;
    kernel_directory->tablesPhysical[0] = (uint32_t)kernel_page_table | 3; // Présent, R/W, Sup

    // Charge le nouveau répertoire de pages
    load_page_directory(kernel_directory->tablesPhysical);
    
    // Active le paging
    enable_paging();
    
    // Initialise le pointeur global
    current_directory = kernel_directory;
}

// Change le répertoire de pages actuel
void vmm_switch_page_directory(page_directory_t *dir) {
    current_directory = dir;
    load_page_directory(dir->tablesPhysical);
}

// Obtient une page depuis une adresse virtuelle
page_t *vmm_get_page(uint32_t address, int make, page_directory_t *dir) {
    // Aligne l'adresse sur une frontière de page
    address /= 0x1000;
    
    // Trouve l'index de la table dans le répertoire
    uint32_t table_idx = address / 1024;
    
    if (dir->tables[table_idx]) {
        // La table existe déjà
        return &dir->tables[table_idx]->pages[address % 1024];
    } else if (make) {
        // Crée une nouvelle table de pages
        uint32_t tmp;
        dir->tables[table_idx] = (page_table_t*)pmm_alloc_page();
        if (!dir->tables[table_idx]) {
            return 0; // Échec de l'allocation
        }
        
        // Obtient l'adresse physique de la nouvelle table
        tmp = (uint32_t)dir->tables[table_idx];
        dir->tablesPhysical[table_idx] = tmp | 0x07; // PRESENT, RW, US
        
        // Initialise la nouvelle table
        for (int i = 0; i < 1024; i++) {
            dir->tables[table_idx]->pages[i].frame = 0;
            dir->tables[table_idx]->pages[i].present = 0;
            dir->tables[table_idx]->pages[i].rw = 0;
            dir->tables[table_idx]->pages[i].user = 0;
            dir->tables[table_idx]->pages[i].accessed = 0;
            dir->tables[table_idx]->pages[i].dirty = 0;
        }
        
        return &dir->tables[table_idx]->pages[address % 1024];
    } else {
        return 0; // Table n'existe pas et make = 0
    }
}

// Mappe une page physique à une adresse virtuelle
void vmm_map_page(void *physaddr, void *virtualaddr, uint32_t flags) {
    page_t *page = vmm_get_page((uint32_t)virtualaddr, 1, current_directory);
    if (!page) {
        return; // Échec
    }
    
    page->present = (flags & PAGE_PRESENT) ? 1 : 0;
    page->rw = (flags & PAGE_WRITE) ? 1 : 0;
    page->user = (flags & PAGE_USER) ? 1 : 0;
    page->frame = (uint32_t)physaddr / 0x1000;
}

// Démonte une page virtuelle
void vmm_unmap_page(void *virtualaddr) {
    page_t *page = vmm_get_page((uint32_t)virtualaddr, 0, current_directory);
    if (!page) {
        return; // Page n'existe pas
    }
    
    page->frame = 0;
    page->present = 0;
    page->rw = 0;
    page->user = 0;
    page->accessed = 0;
    page->dirty = 0;
}

// Obtient l'adresse physique correspondant à une adresse virtuelle
void *vmm_get_physical_address(void *virtualaddr) {
    uint32_t virt = (uint32_t)virtualaddr;
    page_t *page = vmm_get_page(virt, 0, current_directory);
    
    if (!page || !page->present) {
        return 0; // Page non mappée
    }
    
    return (void*)((page->frame * 0x1000) + get_page_offset(virt));
}

// Mappe une page physique à une adresse virtuelle dans un répertoire spécifique
void vmm_map_page_in_directory(page_directory_t *dir, void *physaddr, void *virtualaddr, uint32_t flags) {
    page_t *page = vmm_get_page((uint32_t)virtualaddr, 1, dir);
    if (!page) {
        return; // Échec
    }

    page->present = (flags & PAGE_PRESENT) ? 1 : 0;
    page->rw = (flags & PAGE_WRITE) ? 1 : 0;
    page->user = (flags & PAGE_USER) ? 1 : 0;
    page->frame = (uint32_t)physaddr / 0x1000;
}

// Obtient l'adresse physique correspondant à une adresse virtuelle dans un répertoire spécifique
void *vmm_get_physical_address_from_directory(page_directory_t *dir, void *virtualaddr) {
    uint32_t virt = (uint32_t)virtualaddr;
    page_t *page = vmm_get_page(virt, 0, dir);

    if (!page || !page->present) {
        return 0; // Page non mappée
    }

    return (void*)((page->frame * 0x1000) + get_page_offset(virt));
}
