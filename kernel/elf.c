#include "elf.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "mem/string.h"
#include "kernel/mem/vmm.h"
#include "kernel/mem/pmm.h"
#include "kernel/mem/string.h"

extern void print_string_serial(const char* str);
extern void print_hex_serial(uint32_t n);

int elf_validate(uint8_t* elf_data) {
    if (!elf_data) return 0;
    elf32_ehdr_t* header = (elf32_ehdr_t*)elf_data;
    if (header->e_ident[0] != 0x7F || header->e_ident[1] != 'E' || header->e_ident[2] != 'L' || header->e_ident[3] != 'F') {
        return 0;
    }
    if (header->e_type != ET_EXEC) return 0;
    return 1;
}

uint32_t elf_load(uint8_t* file_data, vmm_directory_t* vmm_dir) {
    if (!elf_validate(file_data)) {
        print_string_serial("ERREUR: ELF invalide\n");
        return 0;
    }

    elf32_ehdr_t* header = (elf32_ehdr_t*)file_data;
    uint32_t entry_point = header->e_entry;
    uint32_t highest_addr = 0;

    // Charger les segments PT_LOAD
    elf32_phdr_t* pheaders = (elf32_phdr_t*)(file_data + header->e_phoff);
    for (int i = 0; i < header->e_phnum; i++) {
        if (pheaders[i].p_type == PT_LOAD) {
            uint32_t virt_addr = pheaders[i].p_vaddr;
            uint32_t mem_size = pheaders[i].p_memsz;
            uint32_t file_size = pheaders[i].p_filesz;

            for (uint32_t addr = virt_addr; addr < virt_addr + mem_size; addr += PAGE_SIZE) {
                void* page = pmm_alloc_page();
                if (page) {
                    vmm_map_page_in_directory(vmm_dir, page, (void*)addr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
                } else {
                    print_string_serial("ERREUR: Impossible d'allouer une page pour le segment PT_LOAD\n");
                    return 0;
                }
            }

            // Copier les données
            memcpy((void*)virt_addr, file_data + pheaders[i].p_offset, file_size);

            // Mettre à jour l'adresse la plus haute
            if (virt_addr + mem_size > highest_addr) {
                highest_addr = virt_addr + mem_size;
            }
        }
    }

    // --- GESTION DE LA SECTION .BSS ---
    elf32_shdr_t* sections = (elf32_shdr_t*)(file_data + header->e_shoff);
    char* string_table = (char*)(file_data + sections[header->e_shstrndx].sh_offset);

    for (int i = 0; i < header->e_shnum; i++) {
        if (sections[i].sh_type == SHT_NOBITS && sections[i].sh_size > 0) {
            if (strcmp(string_table + sections[i].sh_name, ".bss") == 0) {

                uint32_t bss_addr = sections[i].sh_addr;
                uint32_t bss_size = sections[i].sh_size;

                print_string_serial("Allocation de la section .bss...\n");
                uint32_t start_addr = bss_addr & ~0xFFF;
                uint32_t end_addr = bss_addr + bss_size;

                for (uint32_t addr = start_addr; addr < end_addr; addr += PAGE_SIZE) {
                    void* page = pmm_alloc_page();
                    if (page) {
                        vmm_map_page_in_directory(vmm_dir, page, (void*)addr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
                        // It is crucial to zero the page
                        memset((void*)addr, 0, PAGE_SIZE);
                    } else {
                        print_string_serial("ERREUR: Impossible d'allouer une page pour .bss\n");
                        return 0;
                    }
                }
                print_string_serial(".bss alloue de "); print_hex_serial(bss_addr);
                print_string_serial(" a "); print_hex_serial(end_addr); print_string_serial("\n");
                break; // Found and handled .bss, exit loop
            }
        }
    }

    return entry_point;
}
