#include "vmm.h"
#include "pmm.h"

// Variables globales pour la gestion de la mémoire virtuelle
page_directory_t *kernel_directory = 0;
page_directory_t *current_directory = 0;

// Répertoire de pages et première table de pages (alignés sur 4KB)
static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t first_page_table[1024] __attribute__((aligned(4096)));

// Fonction utilitaire pour obtenir l'index du répertoire depuis une adresse virtuelle
static uint32_t get_page_directory_index(uint32_t virtual_addr) {
    return virtual_addr >> 22;
}

// Fonction utilitaire pour obtenir l'index de la table depuis une adresse virtuelle
static uint32_t get_page_table_index(uint32_t virtual_addr) {
    return (virtual_addr >> 12) & 0x3FF;
}

// Fonction utilitaire pour obtenir l'offset dans la page
static uint32_t get_page_offset(uint32_t virtual_addr) {
    return virtual_addr & 0xFFF;
}

// Initialise le gestionnaire de mémoire virtuelle
void vmm_init() {
    // Initialise le répertoire de pages
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0x00000002; // Non présent, lecture/écriture, superviseur
    }

    // Initialise la première table de pages pour mapper les premiers 4 Mo
    // Mapping identité 1:1 (adresse virtuelle = adresse physique)
    for (int i = 0; i < 1024; i++) {
        // Chaque page fait 4KB, donc page i commence à l'adresse i * 0x1000
        // Flags: présent + lecture/écriture
        first_page_table[i] = (i * 0x1000) | 3;
    }

    // Lie la première entrée du répertoire à notre table de pages
    page_directory[0] = ((uint32_t)first_page_table) | 3;

    // Charge le répertoire de pages dans le registre CR3
    load_page_directory(page_directory);
    
    // Active le paging en mettant le bit 31 du registre CR0
    enable_paging();
    
    // Initialise les pointeurs globaux
    kernel_directory = (page_directory_t*)page_directory;
    current_directory = kernel_directory;
}

// Change le répertoire de pages actuel
void vmm_switch_page_directory(page_directory_t *dir) {
    current_directory = dir;
    load_page_directory((uint32_t*)&dir->tablesPhysical);
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

