#include "pmm.h"
#include "../multiboot.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

// Symbol from linker script
extern uint32_t end;

// Variables globales pour le gestionnaire de mémoire physique
static uint32_t* memory_map = 0;
static uint32_t total_pages = 0;
static uint32_t used_pages = 0;

// Fonctions utilitaires pour manipuler le bitmap
void pmm_set_page(uint32_t page_num) {
    if (page_num < total_pages) {
        memory_map[page_num / 32] |= (1 << (page_num % 32));
    }
}

void pmm_clear_page(uint32_t page_num) {
    if (page_num < total_pages) {
        memory_map[page_num / 32] &= ~(1 << (page_num % 32));
    }
}

int pmm_test_page(uint32_t page_num) {
    if (page_num >= total_pages) {
        return 1; // Considérer comme utilisé si hors limites
    }
    return (memory_map[page_num / 32] & (1 << (page_num % 32))) ? 1 : 0;
}

uint32_t pmm_find_free_page() {
    uint32_t bitmap_size_dwords = (total_pages + 31) / 32;
    for (uint32_t i = 0; i < bitmap_size_dwords; i++) {
        if (memory_map[i] != 0xFFFFFFFF) {
            for (int j = 0; j < 32; j++) {
                if (!(memory_map[i] & (1 << j))) {
                    uint32_t page_num = i * 32 + j;
                    // Ensure we don't return a page that is out of bounds
                    if (page_num < total_pages) {
                        return page_num;
                    }
                }
            }
        }
    }
    return 0xFFFFFFFF; // No free page found
}

// Initialise le gestionnaire de mémoire physique
void pmm_init(uint32_t memory_size, uint32_t multiboot_addr) {
    total_pages = memory_size / PAGE_SIZE;
    used_pages = 0;
    
    multiboot_info_t* mbi = (multiboot_info_t*)multiboot_addr;

    // Calcule l'adresse la plus haute utilisée par le noyau et les modules
    uint32_t highest_addr = (uint32_t)&end;
    if ((mbi->flags & (1 << 3))) { // Si les modules sont présents
        multiboot_module_t* modules = (multiboot_module_t*)mbi->mods_addr;
        for (uint32_t i = 0; i < mbi->mods_count; i++) {
            if (modules[i].mod_end > highest_addr) {
                highest_addr = modules[i].mod_end;
            }
        }
    }

    // Place le bitmap juste après
    memory_map = (uint32_t*)((highest_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

    // Initialise le bitmap à zéro
    uint32_t bitmap_size_bytes = ((total_pages + 7) / 8);
    for (uint32_t i = 0; i < (bitmap_size_bytes + 3) / 4; i++) {
        memory_map[i] = 0;
    }
    
    // Marque les pages utilisées par le noyau, les modules et le bitmap
    uint32_t reserved_until = ((uint32_t)memory_map + bitmap_size_bytes);
    uint32_t reserved_pages = (reserved_until + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint32_t i = 0; i < reserved_pages; i++) {
        if (i < total_pages) {
            pmm_set_page(i);
        }
    }
    used_pages = reserved_pages;
}


// Alloue une page de mémoire physique
void* pmm_alloc_page() {
    uint32_t page_num = pmm_find_free_page();
    if (page_num == 0xFFFFFFFF) {
        return NULL; // Pas de mémoire disponible
    }
    
    pmm_set_page(page_num);
    used_pages++;
    
    return (void*)(page_num * PAGE_SIZE);
}

// Libère une page de mémoire physique
void pmm_free_page(void* page) {
    if (page == NULL) {
        return;
    }
    
    uint32_t page_num = (uint32_t)page / PAGE_SIZE;
    if (page_num >= total_pages) {
        return; // Adresse invalide
    }
    
    if (pmm_test_page(page_num)) {
        pmm_clear_page(page_num);
        used_pages--;
    }
}

// Fonctions d'information
uint32_t pmm_get_total_pages() {
    return total_pages;
}

uint32_t pmm_get_used_pages() {
    return used_pages;
}

uint32_t pmm_get_free_pages() {
    return total_pages - used_pages;
}
