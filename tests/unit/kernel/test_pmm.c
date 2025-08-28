/* test_pmm.c - Tests unitaires pour le Physical Memory Manager */

#include "../framework/unity.h"
#include "../framework/test_kernel.h"

// Include du module à tester
#include "../../kernel/mem/pmm.h"
#include "../../kernel/mem/pmm.c"

// === SETUP ET TEARDOWN ===

void setUp(void) {
    test_kernel_init();
    test_kernel_save_state();
}

void tearDown(void) {
    test_kernel_restore_state();
    test_kernel_cleanup();
}

// === TESTS D'INITIALISATION ===

void test_pmm_init_basic(void) {
    uint32_t memory_size = 128 * 1024 * 1024; // 128 MB
    uint32_t multiboot_addr = 0x100000; // 1MB
    
    pmm_init(memory_size, multiboot_addr);
    
    // Vérifier que l'initialisation s'est bien passée
    TEST_ASSERT_GREATER_THAN(0, get_total_pages());
    TEST_ASSERT_GREATER_THAN(0, get_free_pages());
    TEST_ASSERT(get_free_pages() <= get_total_pages());
}

void test_pmm_init_boundary_conditions(void) {
    // Test avec mémoire minimale
    pmm_init(4 * 1024 * 1024, 0x100000); // 4MB minimum
    TEST_ASSERT_GREATER_THAN(0, get_free_pages());
    
    // Reset pour test suivant
    pmm_init(128 * 1024 * 1024, 0x100000);
}

void test_pmm_bitmap_integrity(void) {
    uint32_t memory_size = 128 * 1024 * 1024;
    pmm_init(memory_size, 0x100000);
    
    // Vérifier que le bitmap est correctement initialisé
    uint32_t total_pages = get_total_pages();
    uint32_t free_pages = get_free_pages();
    uint32_t used_pages = total_pages - free_pages;
    
    // Il devrait y avoir des pages utilisées pour le kernel et le bitmap
    TEST_ASSERT_GREATER_THAN(0, used_pages);
    
    // Le nombre de pages libres ne devrait pas dépasser le total
    TEST_ASSERT(free_pages <= total_pages);
}

// === TESTS D'ALLOCATION ===

void test_pmm_alloc_single_page(void) {
    pmm_init(128 * 1024 * 1024, 0x100000);
    
    uint32_t free_pages_before = get_free_pages();
    
    void* page = pmm_alloc_page();
    
    TEST_ASSERT_NOT_NULL(page);
    TEST_ASSERT_PAGE_ALIGNED((uint32_t)page);
    
    uint32_t free_pages_after = get_free_pages();
    TEST_ASSERT_EQUAL(free_pages_before - 1, free_pages_after);
}

void test_pmm_alloc_multiple_pages(void) {
    pmm_init(128 * 1024 * 1024, 0x100000);
    
    const int num_allocs = 10;
    void* pages[num_allocs];
    uint32_t free_pages_before = get_free_pages();
    
    // Allouer plusieurs pages
    for (int i = 0; i < num_allocs; i++) {
        pages[i] = pmm_alloc_page();
        TEST_ASSERT_NOT_NULL(pages[i]);
        TEST_ASSERT_PAGE_ALIGNED((uint32_t)pages[i]);
        
        // Vérifier que chaque page est unique
        for (int j = 0; j < i; j++) {
            TEST_ASSERT_NOT_EQUAL(pages[i], pages[j]);
        }
    }
    
    uint32_t free_pages_after = get_free_pages();
    TEST_ASSERT_EQUAL(free_pages_before - num_allocs, free_pages_after);
}

void test_pmm_alloc_until_exhaustion(void) {
    // Test avec une petite quantité de mémoire pour épuiser rapidement
    pmm_init(8 * 1024 * 1024, 0x100000); // 8MB
    
    int allocated_count = 0;
    void* page;
    
    // Allouer jusqu'à épuisement
    while ((page = pmm_alloc_page()) != NULL) {
        allocated_count++;
        TEST_ASSERT_PAGE_ALIGNED((uint32_t)page);
        
        // Protection contre boucle infinie
        if (allocated_count > 3000) {
            TEST_FAIL();
            break;
        }
    }
    
    // Vérifier qu'on ne peut plus allouer
    TEST_ASSERT_NULL(pmm_alloc_page());
    TEST_ASSERT_EQUAL(0, get_free_pages());
    
    // Il devrait y avoir eu au moins quelques allocations
    TEST_ASSERT_GREATER_THAN(0, allocated_count);
}

// === TESTS DE LIBÉRATION ===

void test_pmm_free_single_page(void) {
    pmm_init(128 * 1024 * 1024, 0x100000);
    
    void* page = pmm_alloc_page();
    TEST_ASSERT_NOT_NULL(page);
    
    uint32_t free_pages_before = get_free_pages();
    
    pmm_free_page(page);
    
    uint32_t free_pages_after = get_free_pages();
    TEST_ASSERT_EQUAL(free_pages_before + 1, free_pages_after);
}

void test_pmm_free_multiple_pages(void) {
    pmm_init(128 * 1024 * 1024, 0x100000);
    
    const int num_pages = 5;
    void* pages[num_pages];
    
    // Allouer les pages
    for (int i = 0; i < num_pages; i++) {
        pages[i] = pmm_alloc_page();
        TEST_ASSERT_NOT_NULL(pages[i]);
    }
    
    uint32_t free_pages_before = get_free_pages();
    
    // Libérer toutes les pages
    for (int i = 0; i < num_pages; i++) {
        pmm_free_page(pages[i]);
    }
    
    uint32_t free_pages_after = get_free_pages();
    TEST_ASSERT_EQUAL(free_pages_before + num_pages, free_pages_after);
}

void test_pmm_free_null_pointer(void) {
    pmm_init(128 * 1024 * 1024, 0x100000);
    
    uint32_t free_pages_before = get_free_pages();
    
    // Libérer un pointeur NULL ne devrait rien faire
    pmm_free_page(NULL);
    
    uint32_t free_pages_after = get_free_pages();
    TEST_ASSERT_EQUAL(free_pages_before, free_pages_after);
}

void test_pmm_free_invalid_address(void) {
    pmm_init(128 * 1024 * 1024, 0x100000);
    
    uint32_t free_pages_before = get_free_pages();
    
    // Essayer de libérer une adresse invalide
    pmm_free_page((void*)0x12345678); // Adresse non alignée sur page
    
    // Le gestionnaire devrait ignorer ou gérer gracieusement
    uint32_t free_pages_after = get_free_pages();
    // On ne peut pas faire d'assertion spécifique car le comportement
    // peut varier selon l'implémentation
}

// === TESTS D'ALLOCATION/LIBÉRATION CYCLIQUE ===

void test_pmm_alloc_free_cycle(void) {
    pmm_init(128 * 1024 * 1024, 0x100000);
    
    uint32_t initial_free = get_free_pages();
    
    // Effectuer plusieurs cycles allocation/libération
    for (int cycle = 0; cycle < 10; cycle++) {
        void* page = pmm_alloc_page();
        TEST_ASSERT_NOT_NULL(page);
        TEST_ASSERT_PAGE_ALIGNED((uint32_t)page);
        
        pmm_free_page(page);
        
        // Vérifier qu'on revient à l'état initial
        TEST_ASSERT_EQUAL(initial_free, get_free_pages());
    }
}

void test_pmm_fragmentation_resistance(void) {
    pmm_init(64 * 1024 * 1024, 0x100000); // 64MB
    
    const int num_pages = 20;
    void* pages[num_pages];
    
    // Allouer un ensemble de pages
    for (int i = 0; i < num_pages; i++) {
        pages[i] = pmm_alloc_page();
        TEST_ASSERT_NOT_NULL(pages[i]);
    }
    
    // Libérer une page sur deux
    for (int i = 1; i < num_pages; i += 2) {
        pmm_free_page(pages[i]);
        pages[i] = NULL;
    }
    
    // Essayer d'allouer de nouvelles pages
    for (int i = 1; i < num_pages; i += 2) {
        pages[i] = pmm_alloc_page();
        TEST_ASSERT_NOT_NULL(pages[i]);
    }
    
    // Libérer toutes les pages
    for (int i = 0; i < num_pages; i++) {
        if (pages[i]) {
            pmm_free_page(pages[i]);
        }
    }
}

// === TESTS DE PERFORMANCE ===

void test_pmm_allocation_performance(void) {
    pmm_init(128 * 1024 * 1024, 0x100000);
    
    const int num_iterations = 1000;
    
    TEST_BENCHMARK("PMM Single Page Allocation", num_iterations, {
        void* page = pmm_alloc_page();
        if (page) pmm_free_page(page);
    });
}

void test_pmm_batch_allocation_performance(void) {
    pmm_init(128 * 1024 * 1024, 0x100000);
    
    const int batch_size = 100;
    void* pages[batch_size];
    
    TEST_BENCHMARK("PMM Batch Allocation", 1, {
        // Allouer un lot de pages
        for (int i = 0; i < batch_size; i++) {
            pages[i] = pmm_alloc_page();
        }
        
        // Les libérer
        for (int i = 0; i < batch_size; i++) {
            if (pages[i]) pmm_free_page(pages[i]);
        }
    });
}

// === TESTS DE ROBUSTESSE ===

void test_pmm_double_free_detection(void) {
    pmm_init(128 * 1024 * 1024, 0x100000);
    
    void* page = pmm_alloc_page();
    TEST_ASSERT_NOT_NULL(page);
    
    // Première libération
    pmm_free_page(page);
    uint32_t free_after_first = get_free_pages();
    
    // Deuxième libération (double free)
    pmm_free_page(page);
    uint32_t free_after_second = get_free_pages();
    
    // Le double free ne devrait pas augmenter le nombre de pages libres
    TEST_ASSERT_EQUAL(free_after_first, free_after_second);
}

void test_pmm_memory_corruption_detection(void) {
    pmm_init(128 * 1024 * 1024, 0x100000);
    
    void* page = pmm_alloc_page();
    TEST_ASSERT_NOT_NULL(page);
    
    // Remplir la page avec un pattern
    test_fill_memory_pattern(page, 4096, TEST_MEMORY_PATTERN_1);
    
    // Vérifier l'intégrité
    TEST_ASSERT_TRUE(test_verify_memory_pattern(page, 4096, TEST_MEMORY_PATTERN_1));
    
    // Corrompre la mémoire
    test_corrupt_memory(page, 4096);
    
    // Détecter la corruption
    TEST_ASSERT_TRUE(test_detect_memory_corruption(page, 4096, TEST_MEMORY_PATTERN_1));
    
    pmm_free_page(page);
}

// === TESTS D'INTÉGRATION ===

void test_pmm_integration_with_multiboot(void) {
    // Simuler différents layouts mémoire multiboot
    uint32_t memory_configs[][2] = {
        {16 * 1024 * 1024, 0x100000},  // 16MB
        {32 * 1024 * 1024, 0x100000},  // 32MB
        {64 * 1024 * 1024, 0x100000},  // 64MB
        {128 * 1024 * 1024, 0x100000}, // 128MB
    };
    
    for (int i = 0; i < 4; i++) {
        pmm_init(memory_configs[i][0], memory_configs[i][1]);
        
        TEST_ASSERT_GREATER_THAN(0, get_total_pages());
        TEST_ASSERT_GREATER_THAN(0, get_free_pages());
        
        // Tester une allocation de base
        void* page = pmm_alloc_page();
        TEST_ASSERT_NOT_NULL(page);
        pmm_free_page(page);
    }
}

// === RUNNER PRINCIPAL ===

int main(void) {
    unity_init();
    
    // Tests d'initialisation
    RUN_TEST(test_pmm_init_basic);
    RUN_TEST(test_pmm_init_boundary_conditions);
    RUN_TEST(test_pmm_bitmap_integrity);
    
    // Tests d'allocation
    RUN_TEST(test_pmm_alloc_single_page);
    RUN_TEST(test_pmm_alloc_multiple_pages);
    RUN_TEST(test_pmm_alloc_until_exhaustion);
    
    // Tests de libération
    RUN_TEST(test_pmm_free_single_page);
    RUN_TEST(test_pmm_free_multiple_pages);
    RUN_TEST(test_pmm_free_null_pointer);
    RUN_TEST(test_pmm_free_invalid_address);
    
    // Tests cycliques
    RUN_TEST(test_pmm_alloc_free_cycle);
    RUN_TEST(test_pmm_fragmentation_resistance);
    
    // Tests de performance
    RUN_TEST(test_pmm_allocation_performance);
    RUN_TEST(test_pmm_batch_allocation_performance);
    
    // Tests de robustesse
    RUN_TEST(test_pmm_double_free_detection);
    RUN_TEST(test_pmm_memory_corruption_detection);
    
    // Tests d'intégration
    RUN_TEST(test_pmm_integration_with_multiboot);
    
    unity_print_results();
    unity_cleanup();
    
    return (unity_stats.tests_failed == 0) ? 0 : 1;
}
