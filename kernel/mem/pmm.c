#include "pmm.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

// Variables globales pour le gestionnaire de mémoire physique
static uint32_t* memory_map = (uint32_t*)BITMAP_LOCATION;
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
    for (uint32_t i = 0; i < total_pages; i++) {
        if (!pmm_test_page(i)) {
            return i;
        }
    }
    return 0xFFFFFFFF; // Aucune page libre trouvée
}

// Initialise le gestionnaire de mémoire physique
void pmm_init(uint32_t memory_size) {
    total_pages = memory_size / PAGE_SIZE;
    used_pages = 0;
    
    // Initialise le bitmap - marque toutes les pages comme libres
    uint32_t bitmap_size = (total_pages + 31) / 32; // Nombre de uint32_t nécessaires
    for (uint32_t i = 0; i < bitmap_size; i++) {
        memory_map[i] = 0;
    }
    
    // Marque les premières pages comme utilisées (noyau, bitmap, etc.)
    // Les premiers 1 Mo sont réservés pour le noyau et les structures système
    uint32_t reserved_pages = 0x100000 / PAGE_SIZE; // 1 MB / 4KB = 256 pages
    for (uint32_t i = 0; i < reserved_pages && i < total_pages; i++) {
        pmm_set_page(i);
        used_pages++;
    }
    
    // Marque aussi les pages utilisées par le bitmap lui-même
    uint32_t bitmap_pages = (bitmap_size * sizeof(uint32_t) + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t bitmap_start_page = BITMAP_LOCATION / PAGE_SIZE;
    for (uint32_t i = 0; i < bitmap_pages; i++) {
        if (bitmap_start_page + i < total_pages) {
            pmm_set_page(bitmap_start_page + i);
            used_pages++;
        }
    }
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

