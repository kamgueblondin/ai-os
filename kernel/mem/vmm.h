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
    uint32_t present    : 1;
    uint32_t rw         : 1;
    uint32_t user       : 1;
    uint32_t accessed   : 1;
    uint32_t dirty      : 1;
    uint32_t unused     : 7;
    uint32_t frame      : 20;
} page_t;

// Structure pour une table de pages
typedef struct {
    page_t pages[ENTRIES_PER_TABLE];
} page_table_t;

// Structure pour un répertoire de pages (matériel)
typedef struct {
    uint32_t tablesPhysical[ENTRIES_PER_TABLE];
} page_directory_t;

// Structure pour la gestion d'un répertoire de pages par le noyau
typedef struct {
    page_table_t** tables;
    page_directory_t* physical_dir;
    uint32_t physical_addr;
} vmm_directory_t;

// Fonctions publiques
void vmm_init();
void vmm_switch_page_directory(page_directory_t *dir);
page_t *vmm_get_page(uint32_t address, int make, vmm_directory_t *dir);
void vmm_map_page_in_directory(vmm_directory_t *dir, void *physaddr, void *virtualaddr, uint32_t flags);

// Variables globales
extern vmm_directory_t *kernel_directory;
extern vmm_directory_t *current_directory;

// Fonctions assembleur
extern void load_page_directory(uint32_t *page_directory);
extern void enable_paging();

#endif
