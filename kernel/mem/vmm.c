#include "vmm.h"
#include "pmm.h"
#include "heap.h"
#include "string.h"

// Externe pour le logging
extern void print_string_serial(const char* str);

// Variables globales pour la gestion de la mémoire virtuelle
vmm_directory_t *kernel_directory = 0;
vmm_directory_t *current_directory = 0;


// Initialise le gestionnaire de mémoire virtuelle
void vmm_init() {
    // Alloue la structure de gestion (ne peut pas utiliser kmalloc ici)
    kernel_directory = (vmm_directory_t*)pmm_alloc_page();
    if (!kernel_directory) return; // Erreur critique
    memset(kernel_directory, 0, sizeof(vmm_directory_t));

    // Alloue la table des tables de pages pour le noyau
    kernel_directory->tables = (page_table_t**)pmm_alloc_page();
    if (!kernel_directory->tables) return; // Erreur critique
    memset(kernel_directory->tables, 0, sizeof(page_table_t*) * 1024);

    // Alloue le répertoire de pages physique (aligné sur 4k)
    kernel_directory->physical_dir = (page_directory_t*)pmm_alloc_page();
    if (!kernel_directory->physical_dir) return; // Erreur critique
    memset(kernel_directory->physical_dir, 0, sizeof(page_directory_t));
    kernel_directory->physical_addr = (uint32_t)kernel_directory->physical_dir;

    // Initialise les entrées du répertoire physique
    for (int i = 0; i < 1024; i++) {
        kernel_directory->physical_dir->tablesPhysical[i] = 0x00000002; // Non présent, R/W
    }

    // Mapper les 4 premiers Mo pour le noyau (identity mapping)
    page_table_t* first_pt = (page_table_t*)pmm_alloc_page();
    if (!first_pt) return; // Erreur critique
    memset(first_pt, 0, sizeof(page_table_t));

    for (int i = 0; i < 1024; i++) {
        first_pt->pages[i].present = 1;
        first_pt->pages[i].rw = 1;
        first_pt->pages[i].user = 0; // Mode noyau
        first_pt->pages[i].frame = i;
    }
    
    // Enregistrer la table dans nos structures
    kernel_directory->tables[0] = first_pt;
    kernel_directory->physical_dir->tablesPhysical[0] = (uint32_t)first_pt | 3; // Présent, R/W, Sup

    // Charger le nouveau répertoire de pages et activer
    load_page_directory(kernel_directory->physical_dir->tablesPhysical);
    enable_paging();

    current_directory = kernel_directory;

    // Initialisation du tas (heap) maintenant que le VMM est prêt
    init_heap();
}

// Change le répertoire de pages actuel
void vmm_switch_page_directory(page_directory_t *dir) {
    // Note: cette fonction doit être mise à jour si on utilise vmm_directory_t partout
    load_page_directory(dir->tablesPhysical);
}

// Obtient une page depuis une adresse virtuelle
page_t *vmm_get_page(uint32_t address, int make, vmm_directory_t *dir) {
    address /= 0x1000;
    uint32_t table_idx = address / 1024;

    if (dir->tables[table_idx]) {
        return &dir->tables[table_idx]->pages[address % 1024];
    } else if (make) {
        page_table_t* new_table = (page_table_t*)kmalloc_aligned(sizeof(page_table_t));
        if (!new_table) return 0;
        memset(new_table, 0, sizeof(page_table_t));
        
        dir->tables[table_idx] = new_table;
        dir->physical_dir->tablesPhysical[table_idx] = (uint32_t)new_table | 0x7; // P, RW, US
        
        return &dir->tables[table_idx]->pages[address % 1024];
    } else {
        return 0;
    }
}

// Mappe une page physique à une adresse virtuelle dans un répertoire spécifique
void vmm_map_page_in_directory(vmm_directory_t *dir, void *physaddr, void *virtualaddr, uint32_t flags) {
    page_t *page = vmm_get_page((uint32_t)virtualaddr, 1, dir);
    if (!page) return;

    page->present = (flags & PAGE_PRESENT) ? 1 : 0;
    page->rw = (flags & PAGE_WRITE) ? 1 : 0;
    page->user = (flags & PAGE_USER) ? 1 : 0;
    page->frame = (uint32_t)physaddr / 0x1000;

    asm volatile ("invlpg (%0)" :: "r" (virtualaddr) : "memory");
}
