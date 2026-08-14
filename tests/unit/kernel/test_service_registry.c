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

static void test_registry_refuses_removal_by_other_owner(void) {
    service_registry_init();
    TEST_ASSERT_EQUAL(0, service_registry_register("vfs", 4));
    TEST_ASSERT_EQUAL(OS_SERVICE_NOT_FOUND, service_registry_remove("vfs", 5));
    TEST_ASSERT_EQUAL(4, service_registry_lookup("vfs"));
    TEST_ASSERT_EQUAL(0, service_registry_remove("vfs", 4));
}

static void test_owner_can_grant_name_to_another_pid(void) {
    service_registry_init();
    TEST_ASSERT_EQUAL(0, service_registry_register("demo", 4));
    TEST_ASSERT_EQUAL(0, service_registry_grant("demo", 4, 9));
    TEST_ASSERT_EQUAL(9, service_registry_lookup("demo"));
    TEST_ASSERT_EQUAL(OS_SERVICE_TAKEN, service_registry_register("demo", 4));
    TEST_ASSERT_EQUAL(0, service_registry_register("demo", 9));
}

static void test_registry_refuses_grant_by_non_owner(void) {
    service_registry_init();
    TEST_ASSERT_EQUAL(0, service_registry_register("demo", 4));
    TEST_ASSERT_EQUAL(OS_SERVICE_NOT_OWNER, service_registry_grant("demo", 5, 9));
    TEST_ASSERT_EQUAL(OS_SERVICE_BAD_GRANTEE, service_registry_grant("demo", 4, 0));
    TEST_ASSERT_EQUAL(4, service_registry_lookup("demo"));
}

static void test_transferred_name_is_removed_with_grantee(void) {
    service_registry_init();
    TEST_ASSERT_EQUAL(0, service_registry_register("demo", 4));
    TEST_ASSERT_EQUAL(0, service_registry_grant("demo", 4, 9));
    TEST_ASSERT_EQUAL(0, service_registry_remove_pid(9));
    TEST_ASSERT_EQUAL(OS_SERVICE_NOT_FOUND, service_registry_lookup("demo"));
}

static void test_watchers_are_collected_and_idempotent(void) {
    int32_t watchers[2];
    service_registry_init();
    TEST_ASSERT_EQUAL(0, service_registry_subscribe("demo", 4));
    TEST_ASSERT_EQUAL(0, service_registry_subscribe("demo", 4));
    TEST_ASSERT_EQUAL(0, service_registry_subscribe("demo", 9));
    TEST_ASSERT_EQUAL(2, service_registry_collect_watchers("demo", watchers, 2U));
    TEST_ASSERT_EQUAL(4, watchers[0]);
    TEST_ASSERT_EQUAL(9, watchers[1]);
}

static void test_watcher_capacity_and_cleanup_are_bounded(void) {
    int32_t watchers[SERVICE_REGISTRY_WATCH_CAPACITY];
    uint32_t i;
    service_registry_init();
    for (i = 0U; i < SERVICE_REGISTRY_WATCH_CAPACITY; i++) {
        TEST_ASSERT_EQUAL(0, service_registry_subscribe("demo", (int32_t)(i + 1U)));
    }
    TEST_ASSERT_EQUAL(OS_SERVICE_WATCH_FULL, service_registry_subscribe("demo", 99));
    TEST_ASSERT_EQUAL(0, service_registry_remove_watcher_pid(4));
    TEST_ASSERT_EQUAL((int)SERVICE_REGISTRY_WATCH_CAPACITY - 1,
                      service_registry_collect_watchers("demo", watchers, SERVICE_REGISTRY_WATCH_CAPACITY));
}

static void test_owned_snapshot_survives_removal_for_notification(void) {
    service_registry_entry_t owned[2];
    service_registry_init();
    TEST_ASSERT_EQUAL(0, service_registry_register("vfs", 3));
    TEST_ASSERT_EQUAL(0, service_registry_register("logger", 3));
    TEST_ASSERT_EQUAL(2, service_registry_collect_owned(3, owned, 2U));
    TEST_ASSERT_EQUAL(3, owned[0].pid);
    TEST_ASSERT_EQUAL('v', owned[0].name[0]);
    TEST_ASSERT_EQUAL(0, service_registry_remove_pid(3));
    TEST_ASSERT_EQUAL('l', owned[1].name[0]);
}

static void test_owner_predicate_tracks_register_grant_and_remove(void) {
    service_registry_init();
    TEST_ASSERT_FALSE(service_registry_pid_is_owner(3));
    TEST_ASSERT_EQUAL(0, service_registry_register("demo", 3));
    TEST_ASSERT_TRUE(service_registry_pid_is_owner(3));
    TEST_ASSERT_EQUAL(0, service_registry_grant("demo", 3, 8));
    TEST_ASSERT_FALSE(service_registry_pid_is_owner(3));
    TEST_ASSERT_TRUE(service_registry_pid_is_owner(8));
    TEST_ASSERT_EQUAL(0, service_registry_remove_pid(8));
    TEST_ASSERT_FALSE(service_registry_pid_is_owner(8));
}

static void test_service_event_is_bounded_and_parsed(void) {
    os_ipc_payload_t payload;
    os_ipc_message_t message;
    os_service_event_t event;
    uint32_t i;
    TEST_ASSERT_EQUAL(0, os_service_make_event(&payload, "demo", 4, 9,
                                                OS_SERVICE_EVENT_GRANTED));
    TEST_ASSERT_EQUAL(OS_IPC_SERVICE_EVENT, payload.type);
    TEST_ASSERT_EQUAL(OS_SERVICE_EVENT_SIZE, payload.size);
    message.sender_pid = 0;
    message.type = payload.type;
    message.size = payload.size;
    message.request_id = payload.request_id;
    for (i = 0U; i < OS_IPC_MAX_DATA; i++) message.data[i] = payload.data[i];
    TEST_ASSERT_EQUAL(0, os_service_parse_event(&message, &event));
    TEST_ASSERT_EQUAL_STRING("demo", event.name);
    TEST_ASSERT_EQUAL(4, event.old_owner_pid);
    TEST_ASSERT_EQUAL(9, event.new_owner_pid);
    TEST_ASSERT_EQUAL(OS_SERVICE_EVENT_GRANTED, event.reason);
    message.sender_pid = 3;
    TEST_ASSERT_TRUE(os_service_parse_event(&message, &event) != 0);
}

static void test_remove_pid_clears_all_services_owned_by_task(void) {
    service_registry_init();
    TEST_ASSERT_EQUAL(0, service_registry_register("vfs", 3));
    TEST_ASSERT_EQUAL(0, service_registry_register("logger", 3));
    TEST_ASSERT_EQUAL(0, service_registry_remove_pid(3));
    TEST_ASSERT_EQUAL(OS_SERVICE_NOT_FOUND, service_registry_lookup("vfs"));
    TEST_ASSERT_EQUAL(OS_SERVICE_NOT_FOUND, service_registry_lookup("logger"));
}

static void test_backend_capability_is_revoked_on_transfer_and_pid_cleanup(void) {
    service_registry_init();
    TEST_ASSERT_EQUAL(0, service_registry_register("vfs", 3));
    TEST_ASSERT_EQUAL(0, service_registry_backend_grant("vfs", 3, 7));
    TEST_ASSERT_TRUE(service_registry_backend_allowed("vfs", 7));
    TEST_ASSERT_EQUAL(0, service_registry_grant("vfs", 3, 9));
    TEST_ASSERT_FALSE(service_registry_backend_allowed("vfs", 7));
    TEST_ASSERT_EQUAL(0, service_registry_backend_grant("vfs", 9, 7));
    TEST_ASSERT_TRUE(service_registry_backend_allowed("vfs", 7));
    service_registry_backend_remove_pid(7);
    TEST_ASSERT_FALSE(service_registry_backend_allowed("vfs", 7));
}

int main(void) {
    unity_init();
    RUN_TEST(test_registry_rejects_invalid_names);
    RUN_TEST(test_registry_binds_name_to_owner_and_is_idempotent);
    RUN_TEST(test_registry_handles_capacity);
    RUN_TEST(test_registry_removal_allows_reuse);
    RUN_TEST(test_registry_refuses_removal_by_other_owner);
    RUN_TEST(test_owner_can_grant_name_to_another_pid);
    RUN_TEST(test_registry_refuses_grant_by_non_owner);
    RUN_TEST(test_transferred_name_is_removed_with_grantee);
    RUN_TEST(test_watchers_are_collected_and_idempotent);
    RUN_TEST(test_watcher_capacity_and_cleanup_are_bounded);
    RUN_TEST(test_owned_snapshot_survives_removal_for_notification);
    RUN_TEST(test_owner_predicate_tracks_register_grant_and_remove);
    RUN_TEST(test_service_event_is_bounded_and_parsed);
    RUN_TEST(test_remove_pid_clears_all_services_owned_by_task);
    RUN_TEST(test_backend_capability_is_revoked_on_transfer_and_pid_cleanup);
    unity_print_results();
    unity_cleanup();
    return unity_stats.tests_failed == 0 ? 0 : 1;
}
