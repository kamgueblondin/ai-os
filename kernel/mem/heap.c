#include "heap.h"
#include "pmm.h"
#include <stdint.h>

// Simple bump allocator
static uint8_t* heap_ptr = NULL;
static uint8_t* heap_end = NULL;

void init_heap() {
    heap_ptr = (uint8_t*)pmm_alloc_page();
    if (heap_ptr) {
        heap_end = heap_ptr + PAGE_SIZE;
    }
}

void* kmalloc(size_t size) {
    if (!heap_ptr) {
        init_heap();
        if (!heap_ptr) {
            return NULL; // Out of memory
        }
    }

    if (heap_ptr + size > heap_end) {
        // Not enough space in current page, allocate a new one
        // For simplicity, we don't handle allocations larger than a page
        if (size > PAGE_SIZE) {
            return NULL;
        }
        init_heap();
        if (!heap_ptr) {
            return NULL;
        }
    }

    void* block = heap_ptr;
    heap_ptr += size;
    return block;
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
