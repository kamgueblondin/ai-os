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

        // SECURITY CHECK: Prevent loading into low memory (kernel space)
        if (ph->p_vaddr < 0x100000) {
            print_string_serial("SECURITY: ELF segment tried to load into kernel space. Aborting.\n");
            return 0;
        }

        // The new address space is active, so we can write directly to virtual addresses.
        uint32_t virt_addr = ph->p_vaddr;
        uint32_t mem_size = ph->p_memsz;
        uint32_t file_size = ph->p_filesz;
        uint32_t file_offset = ph->p_offset;

        // Map all necessary pages for the segment.
        // The user space linker script aligns segments to be page-aligned, so this is safe.
        for (uint32_t j = 0; j < mem_size; j += PAGE_SIZE) {
            void* phys_page = pmm_alloc_page();
            if (!phys_page) {
                print_string_serial("ELF Loader: Out of memory mapping segment\n");
                return 0; // TODO: Cleanup
            }
            uint32_t flags = PAGE_PRESENT | PAGE_USER;
            if (ph->p_flags & PF_W) flags |= PAGE_WRITE;
            vmm_map_page_in_directory(vmm_dir, phys_page, (void*)(virt_addr + j), flags);
        }

        // Now that pages are mapped, copy the data.
        // First, zero out the entire memory region for the segment. This handles the .bss section.
        if (mem_size > 0) {
            memset((void*)virt_addr, 0, mem_size);
        }
        // Then, copy the initialized data from the ELF file.
        if (file_size > 0) {
            memcpy((void*)virt_addr, elf_data + file_offset, file_size);
        }
    }

    return header->e_entry;
}
