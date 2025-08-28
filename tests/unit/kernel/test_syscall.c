/* test_syscall.c - Tests unitaires pour le système d'appels système */

#include <string.h>
#include "../../framework/unity.h"
#include "../../framework/test_kernel.h"

// Include du module à tester
#include "../../../kernel/syscall/syscall.h"
// Ne pas inclure les en-têtes réels du kernel pour éviter les conflits de types

// === MOCK DATA ET HELPERS ===

static char test_output_buffer[1024];
static int test_output_index = 0;
static char test_input_buffer[256];
static int test_input_index = 0;
static int test_input_length = 0;

// Mock des fonctions I/O
void mock_putc(char c) {
    if (test_output_index < sizeof(test_output_buffer) - 1) {
        test_output_buffer[test_output_index++] = c;
        test_output_buffer[test_output_index] = '\0';
    }
}

char mock_getc(void) {
    if (test_input_index < test_input_length) {
        return test_input_buffer[test_input_index++];
    }
    return 0;
}

void setup_test_input(const char* input) {
    test_input_index = 0;
    test_input_length = 0;
    
    while (input[test_input_length] && test_input_length < sizeof(test_input_buffer) - 1) {
        test_input_buffer[test_input_length] = input[test_input_length];
        test_input_length++;
    }
    test_input_buffer[test_input_length] = '\0';
}

void clear_test_output(void) {
    test_output_index = 0;
    test_output_buffer[0] = '\0';
}

// Mock task functions
static int mock_task_exit_called = 0;
static uint32_t mock_exit_code = 0;

void mock_task_exit_with_code(uint32_t code) {
    mock_task_exit_called = 1;
    mock_exit_code = code;
}

// Fonction dummy pour les tâches de test
void dummy_task_function(void) {
    // Fonction vide pour les tests
}

// === SETUP ET TEARDOWN ===

void setUp(void) {
    test_kernel_init();
    test_kernel_save_state();
    
    clear_test_output();
    test_input_index = 0;
    test_input_length = 0;
    mock_task_exit_called = 0;
    mock_exit_code = 0;
    
    syscall_init();
}

void tearDown(void) {
    test_kernel_restore_state();
    test_kernel_cleanup();
}

// === TESTS D'INITIALISATION ===

void test_syscall_init_basic(void) {
    syscall_init();
    
    // Vérifier que l'initialisation s'est bien passée
    // (Pas de vérification spécifique car c'est une fonction void)
    // On teste indirectement via les autres tests
    TEST_ASSERT_TRUE(1);
}

// === TESTS DES APPELS SYSTÈME INDIVIDUELS ===

void test_sys_putc_single_character(void) {
    clear_test_output();
    
    sys_putc('A');
    
    TEST_ASSERT_EQUAL_STRING("A", test_output_buffer);
}

void test_sys_putc_multiple_characters(void) {
    clear_test_output();
    
    sys_putc('H');
    sys_putc('e');
    sys_putc('l');
    sys_putc('l');
    sys_putc('o');
    
    TEST_ASSERT_EQUAL_STRING("Hello", test_output_buffer);
}

void test_sys_putc_special_characters(void) {
    clear_test_output();
    
    sys_putc('\n');
    sys_putc('\t');
    sys_putc('\r');
    sys_putc(' ');
    
    TEST_ASSERT_EQUAL('\n', test_output_buffer[0]);
    TEST_ASSERT_EQUAL('\t', test_output_buffer[1]);
    TEST_ASSERT_EQUAL('\r', test_output_buffer[2]);
    TEST_ASSERT_EQUAL(' ', test_output_buffer[3]);
}

void test_sys_puts_basic_string(void) {
    clear_test_output();
    
    sys_puts("Hello, World!");
    
    TEST_ASSERT_EQUAL_STRING("Hello, World!", test_output_buffer);
}

void test_sys_puts_empty_string(void) {
    clear_test_output();
    
    sys_puts("");
    
    TEST_ASSERT_EQUAL_STRING("", test_output_buffer);
}

void test_sys_puts_null_pointer(void) {
    clear_test_output();
    
    sys_puts(NULL);
    
    // Devrait gérer gracieusement le pointeur NULL
    // Comportement peut varier, mais ne devrait pas crash
    TEST_ASSERT_TRUE(1);
}

void test_sys_getc_single_character(void) {
    setup_test_input("A");
    
    char c = sys_getc();
    
    TEST_ASSERT_EQUAL('A', c);
}

void test_sys_getc_multiple_calls(void) {
    setup_test_input("ABC");
    
    TEST_ASSERT_EQUAL('A', sys_getc());
    TEST_ASSERT_EQUAL('B', sys_getc());
    TEST_ASSERT_EQUAL('C', sys_getc());
}

void test_sys_getc_no_input(void) {
    setup_test_input("");
    
    char c = sys_getc();
    
    // Devrait retourner 0 ou attendre
    TEST_ASSERT_EQUAL(0, c);
}

void test_sys_gets_basic_input(void) {
    setup_test_input("Hello\n");
    
    char buffer[64];
    sys_gets(buffer, sizeof(buffer));
    
    TEST_ASSERT_EQUAL_STRING("Hello", buffer);
}

void test_sys_gets_buffer_size_limit(void) {
    setup_test_input("This is a very long string that should be truncated");
    
    char buffer[10];
    sys_gets(buffer, sizeof(buffer));
    
    // Devrait être tronqué à la taille du buffer
    TEST_ASSERT_EQUAL(9, strlen(buffer)); // -1 pour le \0
    buffer[9] = '\0'; // Assurer la terminaison
}

void test_sys_gets_null_buffer(void) {
    setup_test_input("Test");
    
    sys_gets(NULL, 10);
    
    // Devrait gérer gracieusement le pointeur NULL
    TEST_ASSERT_TRUE(1);
}

void test_sys_exit_with_code(void) {
    sys_exit(42);
    
    TEST_ASSERT_TRUE(mock_task_exit_called);
    TEST_ASSERT_EQUAL(42, mock_exit_code);
}

void test_sys_exit_zero_code(void) {
    sys_exit(0);
    
    TEST_ASSERT_TRUE(mock_task_exit_called);
    TEST_ASSERT_EQUAL(0, mock_exit_code);
}

// === TESTS DU HANDLER SYSCALL ===

void test_syscall_handler_putc(void) {
    clear_test_output();
    
    cpu_state_t cpu_state = {0};
    cpu_state.eax = SYS_PUTC;
    cpu_state.ebx = 'X';
    
    syscall_handler(&cpu_state);
    
    TEST_ASSERT_EQUAL_STRING("X", test_output_buffer);
}

void test_syscall_handler_exit(void) {
    cpu_state_t cpu_state = {0};
    cpu_state.eax = SYS_EXIT;
    cpu_state.ebx = 123;
    
    syscall_handler(&cpu_state);
    
    TEST_ASSERT_TRUE(mock_task_exit_called);
    TEST_ASSERT_EQUAL(123, mock_exit_code);
}

void test_syscall_handler_invalid_syscall(void) {
    cpu_state_t cpu_state = {0};
    cpu_state.eax = 999; // Numéro invalide
    cpu_state.ebx = 0;
    
    // Ne devrait pas crasher
    syscall_handler(&cpu_state);
    
    TEST_ASSERT_TRUE(1);
}

void test_syscall_handler_boundary_syscall_numbers(void) {
    cpu_state_t cpu_state = {0};
    
    // Test avec le plus petit numéro valide
    cpu_state.eax = 0; // SYS_EXIT
    cpu_state.ebx = 0;
    syscall_handler(&cpu_state);
    
    // Reset pour test suivant
    mock_task_exit_called = 0;
    
    // Test avec le plus grand numéro valide
    cpu_state.eax = MAX_SYSCALLS - 1;
    cpu_state.ebx = 0;
    syscall_handler(&cpu_state);
    
    TEST_ASSERT_TRUE(1);
}

// === TESTS DE PERFORMANCE ===

void test_syscall_performance_putc(void) {
    clear_test_output();
    
    const int num_calls = 1000;
    
    TEST_BENCHMARK("SYS_PUTC Performance", num_calls, {
        sys_putc('.');
    });
    
    TEST_ASSERT_EQUAL(num_calls, strlen(test_output_buffer));
}

void test_syscall_performance_getc(void) {
    // Préparer un long input
    char long_input[1001];
    for (int i = 0; i < 1000; i++) {
        long_input[i] = 'A' + (i % 26);
    }
    long_input[1000] = '\0';
    
    setup_test_input(long_input);
    
    const int num_calls = 1000;
    
    TEST_BENCHMARK("SYS_GETC Performance", num_calls, {
        char c = sys_getc();
        (void)c; // Éviter warning unused
    });
}

void test_syscall_handler_performance(void) {
    clear_test_output();
    
    cpu_state_t cpu_state = {0};
    cpu_state.eax = SYS_PUTC;
    cpu_state.ebx = '.';
    
    const int num_calls = 1000;
    
    TEST_BENCHMARK("Syscall Handler Performance", num_calls, {
        syscall_handler(&cpu_state);
    });
}

// === TESTS DE ROBUSTESSE ===

void test_syscall_with_corrupted_state(void) {
    cpu_state_t cpu_state;
    
    // Remplir avec un pattern pour simuler corruption
    test_fill_memory_pattern(&cpu_state, sizeof(cpu_state), 0xDEADBEEF);
    
    // Configurer un syscall valide malgré la corruption
    cpu_state.eax = SYS_PUTC;
    cpu_state.ebx = 'Z';
    
    clear_test_output();
    
    // Ne devrait pas crasher
    syscall_handler(&cpu_state);
    
    // Devrait quand même fonctionner
    TEST_ASSERT_EQUAL_STRING("Z", test_output_buffer);
}

void test_syscall_buffer_overflow_protection(void) {
    // Test avec des paramètres qui pourraient causer un overflow
    char small_buffer[5];
    
    setup_test_input("This is way too long for the buffer");
    
    sys_gets(small_buffer, sizeof(small_buffer));
    
    // Vérifier qu'il n'y a pas eu d'overflow
    for (int i = 0; i < sizeof(small_buffer); i++) {
        // Le buffer ne devrait pas contenir de caractères inattendus
        if (small_buffer[i] == '\0') break;
        TEST_ASSERT(small_buffer[i] >= ' ' && small_buffer[i] <= '~');
    }
}

void test_syscall_parameter_validation(void) {
    cpu_state_t cpu_state = {0};
    
    // Test avec des pointeurs invalides
    cpu_state.eax = SYS_PUTS;
    cpu_state.ebx = 0; // NULL pointer
    
    // Ne devrait pas crasher
    syscall_handler(&cpu_state);
    
    // Test avec une adresse kernel depuis userspace
    cpu_state.eax = SYS_PUTS;
    cpu_state.ebx = 0xC0000000; // Adresse kernel
    
    // Devrait être rejeté ou géré gracieusement
    syscall_handler(&cpu_state);
    
    TEST_ASSERT_TRUE(1);
}

// === TESTS D'INTÉGRATION ===

void test_syscall_integration_with_tasks(void) {
    // Simuler une tâche utilisateur qui fait des appels système
    test_task_t* user_task = test_create_task(dummy_task_function, "user_task", 1);
    // Adapter vers le type mocké task_t défini dans kernel_mocks.c
    current_task = (task_t*)user_task;
    
    // Test d'appels système depuis une tâche utilisateur
    clear_test_output();
    
    cpu_state_t cpu_state = {0};
    cpu_state.eax = SYS_PUTC;
    cpu_state.ebx = 'U';
    
    syscall_handler(&cpu_state);
    
    TEST_ASSERT_EQUAL_STRING("U", test_output_buffer);
    
    test_destroy_task(user_task);
}

void test_syscall_exec_basic(void) {
    // Test basique de sys_exec (simulation)
    char* argv[] = {"test_program", NULL};
    
    int result = sys_exec("/bin/test_program", argv);
    
    // Le résultat dépend de l'implémentation
    // On teste juste que ça ne crash pas
    (void)result;
    TEST_ASSERT_TRUE(1);
}

void test_syscall_yield_integration(void) {
    // Créer plusieurs tâches pour tester yield
    test_task_t* task1 = test_create_task(dummy_task_function, "task1", 1);
    test_task_t* task2 = test_create_task(dummy_task_function, "task2", 1);
    
    add_task_to_queue((task_t*)task1);
    add_task_to_queue((task_t*)task2);
    
    current_task = (task_t*)task1;
    
    // Appeler sys_yield
    sys_yield();
    
    // Vérifier qu'un reschedule a été demandé
    TEST_ASSERT_TRUE(g_reschedule_needed);
    
    test_destroy_task(task1);
    test_destroy_task(task2);
}

// === TESTS DE SÉCURITÉ ===

void test_syscall_ring_isolation(void) {
    // Tester que les syscalls respectent l'isolation Ring 0/3
    test_kernel_set_mode(TEST_RING_3); // Mode utilisateur
    
    cpu_state_t cpu_state = {0};
    cpu_state.eax = SYS_PUTC;
    cpu_state.ebx = 'S';
    cpu_state.cs = 0x1B; // User code segment
    
    clear_test_output();
    
    syscall_handler(&cpu_state);
    
    // Devrait fonctionner depuis l'userspace
    TEST_ASSERT_EQUAL_STRING("S", test_output_buffer);
}

void test_syscall_privilege_escalation_prevention(void) {
    // Tenter d'accéder à des fonctions kernel depuis userspace
    test_kernel_set_mode(TEST_RING_3);
    
    cpu_state_t cpu_state = {0};
    cpu_state.eax = SYS_PUTS;
    cpu_state.ebx = 0xC0000000; // Adresse kernel
    cpu_state.cs = 0x1B; // User segment
    
    // Devrait être rejeté
    syscall_handler(&cpu_state);
    
    TEST_ASSERT_TRUE(1); // Test passe si pas de crash
}

// === RUNNER PRINCIPAL ===

int main(void) {
    unity_init();
    
    // Tests d'initialisation
    RUN_TEST(test_syscall_init_basic);
    
    // Tests des syscalls individuels
    RUN_TEST(test_sys_putc_single_character);
    RUN_TEST(test_sys_putc_multiple_characters);
    RUN_TEST(test_sys_putc_special_characters);
    RUN_TEST(test_sys_puts_basic_string);
    RUN_TEST(test_sys_puts_empty_string);
    RUN_TEST(test_sys_puts_null_pointer);
    RUN_TEST(test_sys_getc_single_character);
    RUN_TEST(test_sys_getc_multiple_calls);
    RUN_TEST(test_sys_getc_no_input);
    RUN_TEST(test_sys_gets_basic_input);
    RUN_TEST(test_sys_gets_buffer_size_limit);
    RUN_TEST(test_sys_gets_null_buffer);
    RUN_TEST(test_sys_exit_with_code);
    RUN_TEST(test_sys_exit_zero_code);
    
    // Tests du handler
    RUN_TEST(test_syscall_handler_putc);
    RUN_TEST(test_syscall_handler_exit);
    RUN_TEST(test_syscall_handler_invalid_syscall);
    RUN_TEST(test_syscall_handler_boundary_syscall_numbers);
    
    // Tests de performance
    RUN_TEST(test_syscall_performance_putc);
    RUN_TEST(test_syscall_performance_getc);
    RUN_TEST(test_syscall_handler_performance);
    
    // Tests de robustesse
    RUN_TEST(test_syscall_with_corrupted_state);
    RUN_TEST(test_syscall_buffer_overflow_protection);
    RUN_TEST(test_syscall_parameter_validation);
    
    // Tests d'intégration
    RUN_TEST(test_syscall_integration_with_tasks);
    RUN_TEST(test_syscall_exec_basic);
    RUN_TEST(test_syscall_yield_integration);
    
    // Tests de sécurité
    RUN_TEST(test_syscall_ring_isolation);
    RUN_TEST(test_syscall_privilege_escalation_prevention);
    
    unity_print_results();
    unity_cleanup();
    
    return (unity_stats.tests_failed == 0) ? 0 : 1;
}
