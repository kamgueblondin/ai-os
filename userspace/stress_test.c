#include <stdint.h>
#include <stddef.h>

// Syscall wrappers
void* malloc(size_t size) {
    void* ptr;
    asm volatile("int $0x80" : "=a"(ptr) : "a"(8), "b"(size));
    return ptr;
}

void free(void* ptr) {
    asm volatile("int $0x80" : : "a"(9), "b"(ptr));
}

void puts(const char* str) {
    asm volatile("int $0x80" : : "a"(3), "b"(str));
}

void exit(int code) {
    asm volatile("int $0x80" : : "a"(0), "b"(code));
}

#define NUM_ALLOCATIONS 1000
#define ALLOCATION_SIZE 1024

void main() {
    puts("Starting memory stress test...\n");

    void* pointers[NUM_ALLOCATIONS];

    for (int j = 0; j < 10; j++) {
        for (int i = 0; i < NUM_ALLOCATIONS; i++) {
            pointers[i] = malloc(ALLOCATION_SIZE);
            if (pointers[i] == NULL) {
                puts("malloc failed!\n");
                exit(1);
            }
        }

        for (int i = 0; i < NUM_ALLOCATIONS; i++) {
            free(pointers[i]);
        }
    }

    puts("Memory stress test completed successfully!\n");
    exit(0);
}
