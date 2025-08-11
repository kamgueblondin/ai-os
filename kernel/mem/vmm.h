#ifndef VMM_H
#define VMM_H

#include <stdint.h>

// Flags pour les entrées de page
#define PAGE_PRESENT    0x01
#define PAGE_WRITE      0x02
#define PAGE_USER       0x04

// Taille d'une page et nombre d'entrées par table
#define PAGE_SIZE       4096
#define ENTRIES_PER_TABLE 1024

// Structure pour une entrée de table de pages
typedef struct {
    uint32_t present    : 1;   // Page présente en mémoire
    uint32_t rw         : 1;   // Lecture/écriture
    uint32_t user       : 1;   // Accessible en mode utilisateur
    uint32_t accessed   : 1;   // Page accédée
    uint32_t dirty      : 1;   // Page modifiée
    uint32_t unused     : 7;   // Bits inutilisés
    uint32_t frame      : 20;  // Adresse du cadre de page (bits 31-12)
} page_t;

// Structure pour une table de pages
typedef struct {
    page_t pages[ENTRIES_PER_TABLE];
} page_table_t;

// Structure pour un répertoire de pages
typedef struct {
    page_table_t *tables[ENTRIES_PER_TABLE];
    uint32_t tablesPhysical[ENTRIES_PER_TABLE];
    uint32_t physicalAddr;
} page_directory_t;

// Fonctions publiques du Virtual Memory Manager
void vmm_init();
void vmm_switch_page_directory(page_directory_t *dir);
page_t *vmm_get_page(uint32_t address, int make, page_directory_t *dir);
void vmm_map_page(void *physaddr, void *virtualaddr, uint32_t flags);
void vmm_unmap_page(void *virtualaddr);
void *vmm_get_physical_address(void *virtualaddr);

// Fonctions utilitaires
page_directory_t *vmm_clone_directory(page_directory_t *src);
void vmm_free_directory(page_directory_t *dir);

// Variables globales
extern page_directory_t *kernel_directory;
extern page_directory_t *current_directory;

// Fonctions assembleur (définies dans boot/paging.s)
extern void load_page_directory(uint32_t *page_directory);
extern void enable_paging();
extern uint32_t read_cr0();
extern void write_cr0(uint32_t value);

#endif

