#include "elf.h"
#include "mem/pmm.h"
#include "mem/vmm.h"

// Déclarations externes nécessaires
extern void print_string_serial(const char* str);

// Valide un fichier ELF
int elf_validate(uint8_t* elf_data) {
    if (!elf_data) return 0;
    
    elf32_ehdr_t* header = (elf32_ehdr_t*)elf_data;
    
    // Vérifie la signature ELF (magic number)
    if (header->e_ident != ELF_MAGIC) {
        return 0;
    }
    
    // Vérifie que c'est un exécutable
    if (header->e_type != ET_EXEC) {
        return 0;
    }
    
    return 1;
}

// Affiche les informations d'un fichier ELF (version simplifiée)
void elf_print_info(uint8_t* elf_data) {
    elf32_ehdr_t* header = (elf32_ehdr_t*)elf_data;
    
    print_string_serial("=== Informations ELF ===\n");
    print_string_serial("Type: Executable\n");
    print_string_serial("Architecture: i386\n");
    print_string_serial("Point d'entree: 0x");
    
    // Affichage hexadécimal simplifié
    uint32_t entry = header->e_entry;
    char hex_str[16];
    int pos = 0;
    
    if (entry == 0) {
        hex_str[pos++] = '0';
    } else {
        while (entry > 0) {
            int digit = entry % 16;
            if (digit < 10) {
                hex_str[pos++] = '0' + digit;
            } else {
                hex_str[pos++] = 'A' + (digit - 10);
            }
            entry /= 16;
        }
    }
    
    // Inverser la chaîne
    for (int i = 0; i < pos / 2; i++) {
        char tmp = hex_str[i];
        hex_str[i] = hex_str[pos - 1 - i];
        hex_str[pos - 1 - i] = tmp;
    }
    hex_str[pos] = '\0';
    
    print_string_serial(hex_str);
    print_string_serial("\n");
    print_string_serial("========================\n");
}

// Charge un exécutable ELF en mémoire (version simplifiée et stable)
uint32_t elf_load(uint8_t* elf_data, uint32_t size) {
    if (!elf_validate(elf_data)) {
        print_string_serial("ERREUR: ELF invalide\n");
        return 0;
    }
    
    elf32_ehdr_t* header = (elf32_ehdr_t*)elf_data;
    elf32_phdr_t* pheaders = (elf32_phdr_t*)(elf_data + header->e_phoff);
    
    print_string_serial("Chargement ELF simplifie...\n");
    
    // Parcourt tous les program headers
    for (int i = 0; i < header->e_phnum; i++) {
        elf32_phdr_t* ph = &pheaders[i];
        
        // Ne traite que les segments LOAD
        if (ph->p_type != PT_LOAD) {
            continue;
        }
        
        // Calcule le nombre de pages nécessaires
        uint32_t pages_needed = (ph->p_memsz + 4095) / 4096;
        
        // Alloue et mappe la mémoire
        for (uint32_t page = 0; page < pages_needed; page++) {
            void* phys_page = pmm_alloc_page();
            if (!phys_page) {
                print_string_serial("ERREUR: Allocation memoire\n");
                return 0;
            }
            
            // Mappe la page à l'adresse virtuelle demandée
            uint32_t virt_addr = ph->p_vaddr + (page * 4096);
            uint32_t flags = PAGE_PRESENT | PAGE_USER;
            if (ph->p_flags & PF_W) flags |= PAGE_WRITE;
            
            vmm_map_page(phys_page, (void*)virt_addr, flags);
        }
        
        // Copie les données du segment via l'adresse physique
        if (ph->p_filesz > 0) {
            uint8_t* src = elf_data + ph->p_offset;
            
            print_string_serial("Copie des donnees du segment...\n");
            print_string_serial("Taille fichier: ");
            
            // Affichage de la taille
            char size_str[16];
            int size_val = ph->p_filesz;
            int pos = 0;
            if (size_val == 0) {
                size_str[pos++] = '0';
            } else {
                while (size_val > 0) {
                    size_str[pos++] = '0' + (size_val % 10);
                    size_val /= 10;
                }
            }
            // Inverser
            for (int i = 0; i < pos / 2; i++) {
                char temp = size_str[i];
                size_str[i] = size_str[pos - 1 - i];
                size_str[pos - 1 - i] = temp;
            }
            size_str[pos] = '\0';
            print_string_serial(size_str);
            print_string_serial(" octets\n");
            
            print_string_serial("Debut copie memoire (methode securisee)...\n");
            
            // Copie page par page pour éviter les erreurs d'accès
            for (uint32_t page = 0; page < pages_needed; page++) {
                uint32_t page_offset = page * 4096;
                uint32_t virt_addr = ph->p_vaddr + page_offset;
                
                // Obtenir l'adresse physique via le VMM
                void* phys_addr = vmm_get_physical_address((void*)virt_addr);
                if (!phys_addr) {
                    print_string_serial("ERREUR: Impossible d'obtenir l'adresse physique\n");
                    continue;
                }
                
                uint8_t* dst = (uint8_t*)phys_addr;
                
                // Calculer les limites de copie pour cette page
                uint32_t copy_start = (page_offset < ph->p_filesz) ? page_offset : ph->p_filesz;
                uint32_t copy_end = ((page_offset + 4096) < ph->p_filesz) ? 
                                  (page_offset + 4096) : ph->p_filesz;
                
                if (copy_start < copy_end) {
                    // Copie des données réelles
                    for (uint32_t b = copy_start; b < copy_end; b++) {
                        dst[b - page_offset] = src[b];
                    }
                }
                
                // Initialisation à zéro du reste de la page si nécessaire
                uint32_t zero_start = (copy_end > page_offset) ? (copy_end - page_offset) : 0;
                uint32_t zero_end = 4096;
                
                if (page_offset + zero_end > ph->p_memsz) {
                    zero_end = ph->p_memsz - page_offset;
                }
                
                for (uint32_t b = zero_start; b < zero_end; b++) {
                    dst[b] = 0;
                }
            }
            
            print_string_serial("Copie memoire terminee.\n");
        }
        
        // Initialisation BSS pour les segments sans données fichier
        print_string_serial("Initialisation BSS...\n");
    }
    
    print_string_serial("ELF charge avec succes\n");
    return header->e_entry;
}

