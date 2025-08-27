#include "heap.h"
#include "pmm.h"
#include <stdint.h>

// Simple but safe allocator. For now, every allocation gets a new page.
// This is inefficient but prevents heap corruption. A proper heap can be
// implemented later.

void init_heap() {
    // No-op, initialization is done on demand.
}

void* kmalloc(size_t size) {
    // For simplicity, we don't handle allocations larger than a page
    if (size > PAGE_SIZE) {
        return NULL;
    }
    // Allocate a full page for every request. Inefficient but safe.
    return pmm_alloc_page();
}

void* kmalloc_aligned(size_t size) {
    // For simplicity, aligned malloc just returns a new page
    // This is not efficient, but it guarantees alignment
    if (size > PAGE_SIZE) {
        return NULL; // Cannot allocate more than a page
    }
    return pmm_alloc_page();
}

void kfree(void* ptr) {
    // Our simple bump allocator does not support freeing memory.
    (void)ptr; // Avoid unused parameter warning
}
