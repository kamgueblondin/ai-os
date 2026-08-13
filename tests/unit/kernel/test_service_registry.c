#include "../../framework/unity.h"
#include "../../../kernel/service_registry.h"

static void test_registry_rejects_invalid_names(void) {
    char too_long[OS_SERVICE_NAME_MAX];
    uint32_t i;
    for (i = 0U; i < OS_SERVICE_NAME_MAX; i++) too_long[i] = 'a';
    service_registry_init();
    TEST_ASSERT_FALSE(service_registry_name_valid(""));
    TEST_ASSERT_FALSE(service_registry_name_valid("vfs/unsafe"));
    TEST_ASSERT_FALSE(service_registry_name_valid("bad name"));
    TEST_ASSERT_FALSE(service_registry_name_valid(too_long));
    TEST_ASSERT_EQUAL(OS_SERVICE_BAD_NAME, service_registry_register("bad name", 1));
}

static void test_registry_binds_name_to_owner_and_is_idempotent(void) {
    service_registry_init();
    TEST_ASSERT_EQUAL(0, service_registry_register("vfs", 7));
    TEST_ASSERT_EQUAL(7, service_registry_lookup("vfs"));
    TEST_ASSERT_EQUAL(0, service_registry_register("vfs", 7));
    TEST_ASSERT_EQUAL(OS_SERVICE_TAKEN, service_registry_register("vfs", 8));
}

static void test_registry_handles_capacity(void) {
    char name[3];
    uint32_t i;
    service_registry_init();
    name[2] = '\0';
    for (i = 0U; i < SERVICE_REGISTRY_CAPACITY; i++) {
        name[0] = 's';
        name[1] = (char)('0' + i);
        TEST_ASSERT_EQUAL(0, service_registry_register(name, (int32_t)(i + 1U)));
    }
    TEST_ASSERT_EQUAL(OS_SERVICE_FULL, service_registry_register("extra", 42));
}

static void test_registry_removal_allows_reuse(void) {
    service_registry_init();
    TEST_ASSERT_EQUAL(0, service_registry_register("vfs", 4));
    TEST_ASSERT_EQUAL(0, service_registry_remove("vfs", 4));
    TEST_ASSERT_EQUAL(OS_SERVICE_NOT_FOUND, service_registry_lookup("vfs"));
    TEST_ASSERT_EQUAL(0, service_registry_register("vfs", 9));
    TEST_ASSERT_EQUAL(9, service_registry_lookup("vfs"));
}

static void test_remove_pid_clears_all_services_owned_by_task(void) {
    service_registry_init();
    TEST_ASSERT_EQUAL(0, service_registry_register("vfs", 3));
    TEST_ASSERT_EQUAL(0, service_registry_register("logger", 3));
    TEST_ASSERT_EQUAL(0, service_registry_remove_pid(3));
    TEST_ASSERT_EQUAL(OS_SERVICE_NOT_FOUND, service_registry_lookup("vfs"));
    TEST_ASSERT_EQUAL(OS_SERVICE_NOT_FOUND, service_registry_lookup("logger"));
}

int main(void) {
    unity_init();
    RUN_TEST(test_registry_rejects_invalid_names);
    RUN_TEST(test_registry_binds_name_to_owner_and_is_idempotent);
    RUN_TEST(test_registry_handles_capacity);
    RUN_TEST(test_registry_removal_allows_reuse);
    RUN_TEST(test_remove_pid_clears_all_services_owned_by_task);
    unity_print_results();
    unity_cleanup();
    return unity_stats.tests_failed == 0 ? 0 : 1;
}
