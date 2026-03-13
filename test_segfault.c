#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint32_t end_sym = 0x200000;
uint32_t* end = &end_sym;

int main() {
    uint32_t memory_size = 128 * 1024 * 1024;
    uint32_t total_pages = memory_size / 4096;
    uint32_t bitmap_size_bytes = (total_pages + 7) / 8;

    printf("Total pages: %u\n", total_pages);
    printf("Bitmap size bytes: %u\n", bitmap_size_bytes);

    void* fake_memory = malloc(memory_size);
    if (!fake_memory) {
        printf("Malloc failed\n");
        return 1;
    }
    printf("Fake memory at %p\n", fake_memory);

    // In pmm.c, it does:
    // memory_map = (uint32_t*)((highest_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
    // highest_addr is derived from &end.

    printf("&end = %p\n", (void*)&end);

    free(fake_memory);
    return 0;
}
