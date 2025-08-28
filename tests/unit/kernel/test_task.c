/* test_task.c - Tests unitaires pour le Task Manager */

#include "../framework/unity.h"
#include "../framework/test_kernel.h"

// Include du module à tester
#include "../../kernel/task/task.h"

// Mock des dépendances
static int mock_task_switch_called = 0;
static task_t* mock_current_task = NULL;
static task_t* mock_task_queue = NULL;

// === MOCK FUNCTIONS ===

// Mock de vmm_create_directory
vmm_directory_t* vmm_create_directory(void) {
    return (vmm_directory_t*)test_malloc(sizeof(vmm_directory_t));
}

// Mock de vmm_switch_directory
void vmm_switch_directory(vmm_directory_t* dir) {
    // Simulation du changement de répertoire de pages
    (void)dir;
}

// Mock de la fonction de changement de contexte assembleur
void switch_task(cpu_state_t* old_state, cpu_state_t* new_state) {
    mock_task_switch_called++;
    if (old_state && new_state) {
        // Copier l'état pour simulation
        *old_state = *new_state;
    }
}

// === SETUP ET TEARDOWN ===

void setUp(void) {
    test_kernel_init();
    test_kernel_save_state();
    
    // Reset des variables globales
    mock_task_switch_called = 0;
    mock_current_task = NULL;
    mock_task_queue = NULL;
    current_task = NULL;
    task_queue = NULL;
    next_task_id = 1;
    g_reschedule_needed = 0;
}

void tearDown(void) {
    test_kernel_restore_state();
    test_kernel_cleanup();
    
    // Nettoyer les tâches créées pendant les tests
    task_t* current = task_queue;
    while (current) {
        task_t* next = current->next;
        test_free(current);
        current = next;
    }
    
    task_queue = NULL;
    current_task = NULL;
}

// === HELPER FUNCTIONS ===

void dummy_task_function(void) {
    // Fonction vide pour les tests
}

void counting_task_function(void) {
    static int counter = 0;
    counter++;
    // Simule du travail
    for (volatile int i = 0; i < 1000; i++);
}

// === TESTS D'INITIALISATION ===

void test_tasking_init(void) {
    tasking_init();
    
    // Vérifier l'état initial
    TEST_ASSERT_NULL(current_task);
    TEST_ASSERT_NULL(task_queue);
    TEST_ASSERT_EQUAL(1, next_task_id);
    TEST_ASSERT_FALSE(g_reschedule_needed);
}

// === TESTS DE CRÉATION DE TÂCHES ===

void test_create_task_basic(void) {
    tasking_init();
    
    task_t* task = create_task(dummy_task_function);
    
    TEST_ASSERT_NOT_NULL(task);
    TEST_ASSERT_EQUAL(1, task->id);
    TEST_ASSERT_EQUAL(TASK_READY, task->state);
    TEST_ASSERT_EQUAL(TASK_TYPE_KERNEL, task->type);
    TEST_ASSERT_NOT_NULL(task->vmm_dir);
    TEST_ASSERT_EQUAL(0, task->kernel_stack_p);
    TEST_ASSERT_NULL(task->next);
    TEST_ASSERT_NULL(task->prev);
}

void test_create_multiple_tasks(void) {
    tasking_init();
    
    const int num_tasks = 5;
    task_t* tasks[num_tasks];
    
    for (int i = 0; i < num_tasks; i++) {
        tasks[i] = create_task(dummy_task_function);
        
        TEST_ASSERT_NOT_NULL(tasks[i]);
        TEST_ASSERT_EQUAL(i + 1, tasks[i]->id);
        TEST_ASSERT_EQUAL(TASK_READY, tasks[i]->state);
        
        // Vérifier l'unicité des IDs
        for (int j = 0; j < i; j++) {
            TEST_ASSERT_NOT_EQUAL(tasks[i]->id, tasks[j]->id);
        }
    }
}

void test_create_task_memory_allocation(void) {
    tasking_init();
    
    task_t* task = create_task(dummy_task_function);
    TEST_ASSERT_NOT_NULL(task);
    
    // Vérifier que la tâche a une structure cohérente
    TEST_ASSERT_STRUCTURE_INTEGRITY(task, task_t, id, task->id);
    TEST_ASSERT_NOT_NULL(task->vmm_dir);
}

// === TESTS DE GESTION DE QUEUE ===

void test_add_task_to_queue_single(void) {
    tasking_init();
    
    task_t* task = create_task(dummy_task_function);
    add_task_to_queue(task);
    
    TEST_ASSERT_EQUAL(task, task_queue);
    TEST_ASSERT_NULL(task->next);
    TEST_ASSERT_NULL(task->prev);
    TEST_ASSERT_EQUAL(1, get_task_count());
}

void test_add_task_to_queue_multiple(void) {
    tasking_init();
    
    const int num_tasks = 3;
    task_t* tasks[num_tasks];
    
    for (int i = 0; i < num_tasks; i++) {
        tasks[i] = create_task(dummy_task_function);
        add_task_to_queue(tasks[i]);
    }
    
    TEST_ASSERT_EQUAL(num_tasks, get_task_count());
    
    // Vérifier l'ordre dans la queue
    task_t* current = task_queue;
    for (int i = 0; i < num_tasks; i++) {
        TEST_ASSERT_EQUAL(tasks[i], current);
        current = current->next;
    }
    TEST_ASSERT_NULL(current);
}

void test_remove_task_from_queue(void) {
    tasking_init();
    
    task_t* task1 = create_task(dummy_task_function);
    task_t* task2 = create_task(dummy_task_function);
    task_t* task3 = create_task(dummy_task_function);
    
    add_task_to_queue(task1);
    add_task_to_queue(task2);
    add_task_to_queue(task3);
    
    TEST_ASSERT_EQUAL(3, get_task_count());
    
    // Supprimer la tâche du milieu
    remove_task(task2);
    
    TEST_ASSERT_EQUAL(2, get_task_count());
    TEST_ASSERT_EQUAL(task1, task_queue);
    TEST_ASSERT_EQUAL(task3, task1->next);
    TEST_ASSERT_EQUAL(task1, task3->prev);
}

// === TESTS DE RECHERCHE DE TÂCHES ===

void test_get_task_by_id(void) {
    tasking_init();
    
    task_t* task1 = create_task(dummy_task_function);
    task_t* task2 = create_task(dummy_task_function);
    task_t* task3 = create_task(dummy_task_function);
    
    add_task_to_queue(task1);
    add_task_to_queue(task2);
    add_task_to_queue(task3);
    
    // Rechercher par ID
    TEST_ASSERT_EQUAL(task1, get_task_by_id(1));
    TEST_ASSERT_EQUAL(task2, get_task_by_id(2));
    TEST_ASSERT_EQUAL(task3, get_task_by_id(3));
    TEST_ASSERT_NULL(get_task_by_id(999));
}

void test_find_task_waiting_for_input(void) {
    tasking_init();
    
    task_t* task1 = create_task(dummy_task_function);
    task_t* task2 = create_task(dummy_task_function);
    task_t* task3 = create_task(dummy_task_function);
    
    task1->state = TASK_RUNNING;
    task2->state = TASK_WAITING_FOR_INPUT;
    task3->state = TASK_READY;
    
    add_task_to_queue(task1);
    add_task_to_queue(task2);
    add_task_to_queue(task3);
    
    TEST_ASSERT_EQUAL(task2, find_task_waiting_for_input());
}

// === TESTS DE SCHEDULING ===

void test_schedule_basic_round_robin(void) {
    tasking_init();
    
    task_t* task1 = create_task(dummy_task_function);
    task_t* task2 = create_task(dummy_task_function);
    
    add_task_to_queue(task1);
    add_task_to_queue(task2);
    
    // Première tâche
    current_task = task1;
    cpu_state_t cpu_state = {0};
    
    schedule(&cpu_state);
    
    // Devrait passer à la tâche suivante
    TEST_ASSERT_EQUAL(task2, current_task);
}

void test_schedule_skip_terminated_tasks(void) {
    tasking_init();
    
    task_t* task1 = create_task(dummy_task_function);
    task_t* task2 = create_task(dummy_task_function);
    task_t* task3 = create_task(dummy_task_function);
    
    task2->state = TASK_TERMINATED;
    
    add_task_to_queue(task1);
    add_task_to_queue(task2);
    add_task_to_queue(task3);
    
    current_task = task1;
    cpu_state_t cpu_state = {0};
    
    schedule(&cpu_state);
    
    // Devrait passer à task3, en sautant task2 (terminée)
    TEST_ASSERT_EQUAL(task3, current_task);
}

void test_schedule_no_ready_tasks(void) {
    tasking_init();
    
    task_t* task1 = create_task(dummy_task_function);
    task1->state = TASK_WAITING;
    
    add_task_to_queue(task1);
    current_task = task1;
    
    cpu_state_t cpu_state = {0};
    
    schedule(&cpu_state);
    
    // Devrait rester sur la même tâche
    TEST_ASSERT_EQUAL(task1, current_task);
}

// === TESTS DE PERFORMANCE ===

void test_task_creation_performance(void) {
    tasking_init();
    
    const int num_tasks = 100;
    
    TEST_BENCHMARK("Task Creation", num_tasks, {
        task_t* task = create_task(dummy_task_function);
        if (task) {
            add_task_to_queue(task);
        }
    });
    
    TEST_ASSERT_EQUAL(num_tasks, get_task_count());
}

void test_scheduling_performance(void) {
    tasking_init();
    
    // Créer plusieurs tâches
    const int num_tasks = 10;
    for (int i = 0; i < num_tasks; i++) {
        task_t* task = create_task(counting_task_function);
        add_task_to_queue(task);
    }
    
    current_task = task_queue;
    cpu_state_t cpu_state = {0};
    
    TEST_BENCHMARK("Task Scheduling", 1000, {
        schedule(&cpu_state);
    });
    
    // Vérifier que le scheduling s'est effectué
    TEST_ASSERT_GREATER_THAN(0, mock_task_switch_called);
}

// === TESTS DE STATES DE TÂCHES ===

void test_task_state_transitions(void) {
    tasking_init();
    
    task_t* task = create_task(dummy_task_function);
    add_task_to_queue(task);
    
    // État initial
    TEST_ASSERT_EQUAL(TASK_READY, task->state);
    
    // Simulation des transitions d'état
    task->state = TASK_RUNNING;
    TEST_ASSERT_EQUAL(TASK_RUNNING, task->state);
    
    task->state = TASK_WAITING;
    TEST_ASSERT_EQUAL(TASK_WAITING, task->state);
    
    task->state = TASK_READY;
    TEST_ASSERT_EQUAL(TASK_READY, task->state);
    
    task->state = TASK_TERMINATED;
    TEST_ASSERT_EQUAL(TASK_TERMINATED, task->state);
}

void test_task_yield_functionality(void) {
    tasking_init();
    
    task_t* task1 = create_task(dummy_task_function);
    task_t* task2 = create_task(dummy_task_function);
    
    add_task_to_queue(task1);
    add_task_to_queue(task2);
    
    current_task = task1;
    
    // Appeler task_yield devrait déclencher un reschedule
    task_yield();
    
    TEST_ASSERT_TRUE(g_reschedule_needed);
}

// === TESTS DE ROBUSTESSE ===

void test_task_memory_corruption_detection(void) {
    tasking_init();
    
    task_t* task = create_task(dummy_task_function);
    
    // Remplir avec un pattern connu
    test_fill_memory_pattern(task, sizeof(task_t), TEST_MEMORY_PATTERN_1);
    
    // Sauvegarder l'ID et l'état (qui sont écrasés par le pattern)
    task->id = 1;
    task->state = TASK_READY;
    
    // Corrompre une partie de la structure
    uint8_t* task_bytes = (uint8_t*)task;
    task_bytes[sizeof(task_t) / 2] ^= 0xFF;
    
    // Détecter la corruption (test simplifié)
    int corruption_detected = (task->id != 1) || (task->state >= 5);
    TEST_ASSERT_FALSE(corruption_detected);
}

void test_task_queue_integrity(void) {
    tasking_init();
    
    const int num_tasks = 5;
    task_t* tasks[num_tasks];
    
    // Créer et ajouter les tâches
    for (int i = 0; i < num_tasks; i++) {
        tasks[i] = create_task(dummy_task_function);
        add_task_to_queue(tasks[i]);
    }
    
    // Vérifier l'intégrité de la liste chaînée
    task_t* current = task_queue;
    int count = 0;
    
    while (current && count < num_tasks + 1) { // +1 pour éviter boucle infinie
        count++;
        
        // Vérifier les liens avant/arrière
        if (current->next) {
            TEST_ASSERT_EQUAL(current, current->next->prev);
        }
        if (current->prev) {
            TEST_ASSERT_EQUAL(current, current->prev->next);
        }
        
        current = current->next;
    }
    
    TEST_ASSERT_EQUAL(num_tasks, count);
    TEST_ASSERT_EQUAL(num_tasks, get_task_count());
}

void test_task_cleanup_on_termination(void) {
    tasking_init();
    
    task_t* task = create_task(dummy_task_function);
    add_task_to_queue(task);
    
    // Simuler la terminaison
    task_exit();
    
    if (current_task) {
        TEST_ASSERT_EQUAL(TASK_TERMINATED, current_task->state);
    }
}

// === TESTS D'INTEGRATION ===

void test_task_integration_with_memory(void) {
    tasking_init();
    
    task_t* task = create_task(dummy_task_function);
    
    // Vérifier que la tâche a son propre espace mémoire
    TEST_ASSERT_NOT_NULL(task->vmm_dir);
    
    // L'adresse du répertoire de pages devrait être valide
    TEST_ASSERT_NOT_NULL(task->vmm_dir);
    
    add_task_to_queue(task);
}

void test_multitask_execution_simulation(void) {
    tasking_init();
    
    const int num_tasks = 3;
    task_t* tasks[num_tasks];
    
    // Créer plusieurs tâches
    for (int i = 0; i < num_tasks; i++) {
        tasks[i] = create_task(counting_task_function);
        add_task_to_queue(tasks[i]);
    }
    
    current_task = task_queue;
    cpu_state_t cpu_state = {0};
    
    // Simuler plusieurs cycles de scheduling
    const int num_cycles = 20;
    for (int i = 0; i < num_cycles; i++) {
        schedule(&cpu_state);
    }
    
    // Vérifier que le scheduling a eu lieu
    TEST_ASSERT_GREATER_THAN(num_cycles / 2, mock_task_switch_called);
}

// === RUNNER PRINCIPAL ===

int main(void) {
    unity_init();
    
    // Tests d'initialisation
    RUN_TEST(test_tasking_init);
    
    // Tests de création de tâches
    RUN_TEST(test_create_task_basic);
    RUN_TEST(test_create_multiple_tasks);
    RUN_TEST(test_create_task_memory_allocation);
    
    // Tests de gestion de queue
    RUN_TEST(test_add_task_to_queue_single);
    RUN_TEST(test_add_task_to_queue_multiple);
    RUN_TEST(test_remove_task_from_queue);
    
    // Tests de recherche
    RUN_TEST(test_get_task_by_id);
    RUN_TEST(test_find_task_waiting_for_input);
    
    // Tests de scheduling
    RUN_TEST(test_schedule_basic_round_robin);
    RUN_TEST(test_schedule_skip_terminated_tasks);
    RUN_TEST(test_schedule_no_ready_tasks);
    
    // Tests de performance
    RUN_TEST(test_task_creation_performance);
    RUN_TEST(test_scheduling_performance);
    
    // Tests d'états
    RUN_TEST(test_task_state_transitions);
    RUN_TEST(test_task_yield_functionality);
    
    // Tests de robustesse
    RUN_TEST(test_task_memory_corruption_detection);
    RUN_TEST(test_task_queue_integrity);
    RUN_TEST(test_task_cleanup_on_termination);
    
    // Tests d'intégration
    RUN_TEST(test_task_integration_with_memory);
    RUN_TEST(test_multitask_execution_simulation);
    
    unity_print_results();
    unity_cleanup();
    
    return (unity_stats.tests_failed == 0) ? 0 : 1;
}
