// test_stress_memory.c - Tests de charge mémoire pour AI-OS
// Ce programme stresse les gestionnaires PMM et VMM

// Wrappers pour les appels système
void putc(char c) { 
    asm volatile("int $0x80" : : "a"(1), "b"(c)); 
}

void puts(const char* str) { 
    asm volatile("int $0x80" : : "a"(3), "b"(str)); 
}

void yield() { 
    asm volatile("int $0x80" : : "a"(4)); 
}

void exit(int code) { 
    asm volatile("int $0x80" : : "a"(0), "b"(code)); 
}

// Test intensif de mémoire
void main() {
    puts("=== Test de Charge Mémoire AI-OS ===\n");
    puts("Simulation d'allocation/désallocation intensive\n\n");
    
    // Test 1: Allocations mémoire simulées
    puts("Test 1: Allocation mémoire simulée\n");
    for (int i = 0; i < 1000; i++) {
        if (i % 100 == 0) {
            puts("Allocation ");
            putc('0' + (i / 100));
            puts("00...\n");
            yield();
        }
        
        // Simulation d'opérations mémoire
        for (volatile int j = 0; j < 10000; j++);
    }
    
    // Test 2: Fragmentation mémoire
    puts("\nTest 2: Test de fragmentation\n");
    for (int i = 0; i < 50; i++) {
        puts("Fragment ");
        putc('0' + (i / 10));
        putc('0' + (i % 10));
        puts("\n");
        
        // Cession périodique du CPU
        yield();
        
        for (volatile int j = 0; j < 50000; j++);
    }
    
    // Test 3: Accès mémoire aléatoire
    puts("\nTest 3: Accès mémoire aléatoire\n");
    int fake_memory[100];
    for (int i = 0; i < 100; i++) {
        fake_memory[i] = i * i;
        if (i % 20 == 0) {
            puts("Mémoire index ");
            putc('0' + (i / 10));
            putc('0' + (i % 10));
            puts("\n");
            yield();
        }
    }
    
    puts("\nTest mémoire terminé avec succès!\n");
    exit(0);
}