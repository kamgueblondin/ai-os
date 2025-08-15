#include "elf.h"
#include "mem/pmm.h"
#include "mem/vmm.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

// Fonctions externes
extern void print_string_serial(const char* str);

// Valide un fichier ELF
int elf_validate(uint8_t* elf_data) {
    if (!elf_data) {
        return 0;
    }
    
    elf32_ehdr_t* header = (elf32_ehdr_t*)elf_data;
    
    // Vérifie le magic number
    if (header->e_ident != ELF_MAGIC) {
        print_string_serial("ERREUR ELF: Magic number invalide\n");
        return 0;
    }
    
    // Vérifie que c'est un exécutable 32-bit
    if (header->e_type != ET_EXEC) {
        print_string_serial("ERREUR ELF: Pas un executable\n");
        return 0;
    }
    
    // Vérifie l'architecture (i386)
    if (header->e_machine != 3) { // EM_386
        print_string_serial("ERREUR ELF: Architecture non supportee\n");
        return 0;
    }
    
    return 1;
}

// Affiche les informations d'un fichier ELF
void elf_print_info(uint8_t* elf_data) {
    if (!elf_validate(elf_data)) {
        return;
    }
    
    elf32_ehdr_t* header = (elf32_ehdr_t*)elf_data;
    
    print_string_serial("=== Informations ELF ===\n");
    print_string_serial("Point d'entree: 0x");
    
    // Affichage hexadécimal simple du point d'entrée
    uint32_t entry = header->e_entry;
    char hex_str[16];
    int i = 0;
    if (entry == 0) {
        hex_str[i++] = '0';
    } else {
        while (entry > 0) {
            int digit = entry % 16;
            if (digit < 10) {
                hex_str[i++] = '0' + digit;
            } else {
                hex_str[i++] = 'A' + (digit - 10);
            }
            entry /= 16;
        }
    }
    hex_str[i] = '\0';
    
    // Inverse la chaîne
    for (int j = 0; j < i / 2; j++) {
        char tmp = hex_str[j];
        hex_str[j] = hex_str[i - 1 - j];
        hex_str[i - 1 - j] = tmp;
    }
    
    print_string_serial(hex_str);
    print_string_serial("\n");
    
    print_string_serial("Nombre de segments: ");
    
    // Affichage du nombre de segments
    uint16_t phnum = header->e_phnum;
    char num_str[16];
    int k = 0;
    if (phnum == 0) {
        num_str[k++] = '0';
    } else {
        while (phnum > 0) {
            num_str[k++] = '0' + (phnum % 10);
            phnum /= 10;
        }
    }
    num_str[k] = '\0';
    
    // Inverse la chaîne
    for (int j = 0; j < k / 2; j++) {
        char tmp = num_str[j];
        num_str[j] = num_str[k - 1 - j];
        num_str[k - 1 - j] = tmp;
    }
    
    print_string_serial(num_str);
    print_string_serial("\n");
    print_string_serial("========================\n");
}

// Charge un exécutable ELF en mémoire
uint32_t elf_load(uint8_t* elf_data, uint32_t size) {
    if (!elf_validate(elf_data)) {
        return 0;
    }
    
    elf32_ehdr_t* header = (elf32_ehdr_t*)elf_data;
    elf32_phdr_t* pheaders = (elf32_phdr_t*)(elf_data + header->e_phoff);
    
    print_string_serial("Chargement de l'executable ELF...\n");
    elf_print_info(elf_data);
    
    // Parcourt tous les program headers
    for (int i = 0; i < header->e_phnum; i++) {
        elf32_phdr_t* ph = &pheaders[i];
        
        // Ne traite que les segments LOAD
        if (ph->p_type != PT_LOAD) {
            continue;
        }
        
        print_string_serial("Chargement du segment ");
        char seg_str[8];
        int j = 0;
        int seg_num = i;
        if (seg_num == 0) {
            seg_str[j++] = '0';
        } else {
            while (seg_num > 0) {
                seg_str[j++] = '0' + (seg_num % 10);
                seg_num /= 10;
            }
        }
        seg_str[j] = '\0';
        
        // Inverse la chaîne
        for (int k = 0; k < j / 2; k++) {
            char tmp = seg_str[k];
            seg_str[k] = seg_str[j - 1 - k];
            seg_str[j - 1 - k] = tmp;
        }
        
        print_string_serial(seg_str);
        print_string_serial("...\n");
        
        // Calcule le nombre de pages nécessaires
        uint32_t pages_needed = (ph->p_memsz + 4095) / 4096;
        
        // Alloue la mémoire physique
        for (uint32_t page = 0; page < pages_needed; page++) {
            void* phys_page = pmm_alloc_page();
            if (!phys_page) {
                print_string_serial("ERREUR: Impossible d'allouer la memoire\n");
                return 0;
            }
            
            // Mappe la page à l'adresse virtuelle demandée
            uint32_t virt_addr = ph->p_vaddr + (page * 4096);
            uint32_t flags = PAGE_PRESENT | PAGE_WRITE;
            if (ph->p_flags & PF_W) flags |= PAGE_WRITE;
            
            vmm_map_page(phys_page, (void*)virt_addr, flags);
        }
        
        // Copie les données du segment
        if (ph->p_filesz > 0) {
            uint8_t* src = elf_data + ph->p_offset;
            uint8_t* dst = (uint8_t*)ph->p_vaddr;
            
            for (uint32_t b = 0; b < ph->p_filesz; b++) {
                dst[b] = src[b];
            }
        }
        
        // Initialise à zéro la partie BSS (si p_memsz > p_filesz)
        if (ph->p_memsz > ph->p_filesz) {
            uint8_t* bss_start = (uint8_t*)(ph->p_vaddr + ph->p_filesz);
            uint32_t bss_size = ph->p_memsz - ph->p_filesz;
            
            for (uint32_t b = 0; b < bss_size; b++) {
                bss_start[b] = 0;
            }
        }
    }
    
    print_string_serial("Executable charge avec succes.\n");
    return header->e_entry; // Retourne le point d'entrée
}

