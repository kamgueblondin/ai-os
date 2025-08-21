#include "elf.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/string.h"

extern void print_string_serial(const char* str);

int elf_validate(uint8_t* elf_data) {
    if (!elf_data) return 0;
    elf32_ehdr_t* header = (elf32_ehdr_t*)elf_data;
    if (header->e_ident != ELF_MAGIC) return 0;
    if (header->e_type != ET_EXEC) return 0;
    return 1;
}

uint32_t elf_load(uint8_t* elf_data, vmm_directory_t* vmm_dir) {
    if (!elf_validate(elf_data)) {
        print_string_serial("ERREUR: ELF invalide\n");
        return 0;
    }

    elf32_ehdr_t* header = (elf32_ehdr_t*)elf_data;
    elf32_phdr_t* pheaders = (elf32_phdr_t*)(elf_data + header->e_phoff);

    for (int i = 0; i < header->e_phnum; i++) {
        elf32_phdr_t* ph = &pheaders[i];
        if (ph->p_type != PT_LOAD) continue;

        uint32_t pages_needed = (ph->p_memsz + 4095) / 4096;
        for (uint32_t page = 0; page < pages_needed; page++) {
            void* phys_page = pmm_alloc_page();
            if (!phys_page) {
                print_string_serial("ERREUR: Allocation memoire pour segment ELF\n");
                return 0; // TODO: Libérer les pages déjà allouées
            }
            memset(phys_page, 0, PAGE_SIZE);

            uint32_t virt_addr = ph->p_vaddr + (page * PAGE_SIZE);
            uint32_t flags = PAGE_PRESENT | PAGE_USER;
            if (ph->p_flags & PF_W) flags |= PAGE_WRITE;
            
            vmm_map_page_in_directory(vmm_dir, phys_page, (void*)virt_addr, flags);
            
            // Copier les données
            if (ph->p_filesz > 0) {
                uint32_t file_offset = ph->p_offset + (page * PAGE_SIZE);
                uint32_t bytes_to_copy_from_file = 0;
                if (ph->p_filesz > page * PAGE_SIZE) {
                    bytes_to_copy_from_file = ph->p_filesz - (page * PAGE_SIZE);
                }
                if (bytes_to_copy_from_file > PAGE_SIZE) {
                    bytes_to_copy_from_file = PAGE_SIZE;
                }
                
                memcpy(phys_page, elf_data + file_offset, bytes_to_copy_from_file);
            }
        }
    }

    return header->e_entry;
}
