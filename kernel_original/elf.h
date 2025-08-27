#ifndef ELF_H
#define ELF_H

#include <stdint.h>

// Magic number ELF
#define ELF_MAGIC 0x464C457F

// Types de fichiers ELF
#define ET_NONE   0
#define ET_REL    1
#define ET_EXEC   2
#define ET_DYN    3
#define ET_CORE   4

// Types de segments
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6

// En-tête ELF 32-bit
typedef struct {
    uint32_t e_ident;     // Magic number et autres identifiants
    uint8_t  e_ident_pad[12]; // Padding pour l'identification
    uint16_t e_type;      // Type de fichier
    uint16_t e_machine;   // Architecture cible
    uint32_t e_version;   // Version ELF
    uint32_t e_entry;     // Point d'entrée
    uint32_t e_phoff;     // Offset des program headers
    uint32_t e_shoff;     // Offset des section headers
    uint32_t e_flags;     // Flags spécifiques au processeur
    uint16_t e_ehsize;    // Taille de cet en-tête
    uint16_t e_phentsize; // Taille d'une entrée program header
    uint16_t e_phnum;     // Nombre d'entrées program header
    uint16_t e_shentsize; // Taille d'une entrée section header
    uint16_t e_shnum;     // Nombre d'entrées section header
    uint16_t e_shstrndx;  // Index de la section string table
} __attribute__((packed)) elf32_ehdr_t;

// Program Header 32-bit
typedef struct {
    uint32_t p_type;   // Type de segment
    uint32_t p_offset; // Offset dans le fichier
    uint32_t p_vaddr;  // Adresse virtuelle
    uint32_t p_paddr;  // Adresse physique
    uint32_t p_filesz; // Taille dans le fichier
    uint32_t p_memsz;  // Taille en mémoire
    uint32_t p_flags;  // Flags
    uint32_t p_align;  // Alignement
} __attribute__((packed)) elf32_phdr_t;

// Flags pour les segments
#define PF_X 0x1  // Exécutable
#define PF_W 0x2  // Writable
#define PF_R 0x4  // Readable

// Fonctions publiques
uint32_t elf_load(uint8_t* elf_data, uint32_t size);
int elf_validate(uint8_t* elf_data);
void elf_print_info(uint8_t* elf_data);

#endif

