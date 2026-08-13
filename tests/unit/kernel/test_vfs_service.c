#include "../../framework/unity.h"
#include "../../../include/os_vfs_service.h"

static void test_read_request_is_bounded_and_zero_padded(void) {
    os_ipc_payload_t payload;
    TEST_ASSERT_EQUAL(0, os_vfs_make_read_request(&payload, "hello.txt", 17U));
    TEST_ASSERT_EQUAL(OS_IPC_VFS_READ, payload.type);
    TEST_ASSERT_EQUAL(17, payload.request_id);
    TEST_ASSERT_EQUAL(OS_VFS_PATH_MAX, payload.size);
    TEST_ASSERT_EQUAL('h', payload.data[0]);
    TEST_ASSERT_EQUAL('t', payload.data[8]);
    TEST_ASSERT_EQUAL(0, payload.data[9]);
    TEST_ASSERT_EQUAL(0, payload.data[OS_IPC_MAX_DATA - 1U]);
}

static void test_unsafe_or_unterminated_path_is_rejected(void) {
    os_ipc_payload_t payload;
    char unterminated[OS_VFS_PATH_MAX];
    uint32_t i;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) unterminated[i] = 'a';
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_INVALID, os_vfs_make_read_request(&payload, "../secret", 1U));
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_INVALID, os_vfs_make_read_request(&payload, "", 1U));
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_INVALID, os_vfs_make_read_request(&payload, unterminated, 1U));
}

static void test_server_can_parse_valid_request(void) {
    os_ipc_payload_t payload;
    os_ipc_message_t message;
    char path[OS_VFS_PATH_MAX];
    uint32_t i;
    TEST_ASSERT_EQUAL(0, os_vfs_make_read_request(&payload, "config.cfg", 23U));
    message.sender_pid = 7;
    message.type = payload.type;
    message.size = payload.size;
    message.request_id = payload.request_id;
    for (i = 0U; i < OS_IPC_MAX_DATA; i++) message.data[i] = payload.data[i];
    TEST_ASSERT_EQUAL(0, os_vfs_parse_read_request(&message, path));
    TEST_ASSERT_EQUAL('c', path[0]);
    TEST_ASSERT_EQUAL('g', path[9]);
    TEST_ASSERT_EQUAL(0, path[10]);
}

static void test_read_reply_preserves_status_and_data(void) {
    os_ipc_payload_t payload;
    os_ipc_message_t message;
    os_vfs_read_reply_t reply;
    uint8_t data[3] = {'o', 'k', '\n'};
    uint32_t i;
    TEST_ASSERT_EQUAL(0, os_vfs_make_read_reply(&payload, OS_VFS_STATUS_OK, data, 3U, 29U));
    message.sender_pid = 2;
    message.type = payload.type;
    message.size = payload.size;
    message.request_id = payload.request_id;
    for (i = 0U; i < OS_IPC_MAX_DATA; i++) message.data[i] = payload.data[i];
    TEST_ASSERT_EQUAL(0, os_vfs_parse_read_reply(&message, &reply, 29U));
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_OK, reply.status);
    TEST_ASSERT_EQUAL(3, reply.size);
    TEST_ASSERT_EQUAL('o', reply.data[0]);
    TEST_ASSERT_EQUAL('\n', reply.data[2]);
}

static void test_malformed_reply_is_rejected(void) {
    os_ipc_message_t message;
    os_vfs_read_reply_t reply;
    uint32_t i;
    message.type = OS_IPC_VFS_READ_REPLY;
    message.size = 8U + OS_VFS_READ_MAX - 1U;
    for (i = 0U; i < OS_IPC_MAX_DATA; i++) message.data[i] = 0U;
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_INVALID, os_vfs_parse_read_reply(&message, &reply, 1U));
}

static void test_grant_request_is_bounded_and_validated(void) {
    os_ipc_payload_t payload;
    os_ipc_message_t message;
    int32_t target_pid;
    uint32_t i;
    TEST_ASSERT_EQUAL(0, os_vfs_make_grant_request(&payload, 7));
    TEST_ASSERT_EQUAL(OS_IPC_VFS_GRANT, payload.type);
    TEST_ASSERT_EQUAL(OS_VFS_GRANT_REQUEST_SIZE, payload.size);
    TEST_ASSERT_EQUAL(0, payload.request_id);
    TEST_ASSERT_EQUAL(7, os_vfs_decode_i32(&payload.data[0]));
    TEST_ASSERT_EQUAL(0, payload.data[OS_IPC_MAX_DATA - 1U]);
    message.sender_pid = 1;
    message.type = payload.type;
    message.size = payload.size;
    message.request_id = payload.request_id;
    for (i = 0U; i < OS_IPC_MAX_DATA; i++) message.data[i] = payload.data[i];
    TEST_ASSERT_EQUAL(0, os_vfs_parse_grant_request(&message, &target_pid));
    TEST_ASSERT_EQUAL(7, target_pid);
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_INVALID, os_vfs_make_grant_request(&payload, 0));
    message.size = OS_VFS_GRANT_REQUEST_SIZE - 1U;
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_INVALID, os_vfs_parse_grant_request(&message, &target_pid));
}

static void test_reply_with_unexpected_request_id_is_rejected(void) {
    os_ipc_payload_t payload;
    os_ipc_message_t message;
    os_vfs_read_reply_t reply;
    uint8_t data[1] = {'x'};
    uint32_t i;
    TEST_ASSERT_EQUAL(0, os_vfs_make_read_reply(&payload, OS_VFS_STATUS_OK, data, 1U, 41U));
    message.sender_pid = 3;
    message.type = payload.type;
    message.size = payload.size;
    message.request_id = payload.request_id;
    for (i = 0U; i < OS_IPC_MAX_DATA; i++) message.data[i] = payload.data[i];
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_INVALID, os_vfs_parse_read_reply(&message, &reply, 42U));
}

int main(void) {
    unity_init();
    RUN_TEST(test_read_request_is_bounded_and_zero_padded);
    RUN_TEST(test_unsafe_or_unterminated_path_is_rejected);
    RUN_TEST(test_server_can_parse_valid_request);
    RUN_TEST(test_read_reply_preserves_status_and_data);
    RUN_TEST(test_malformed_reply_is_rejected);
    RUN_TEST(test_grant_request_is_bounded_and_validated);
    RUN_TEST(test_reply_with_unexpected_request_id_is_rejected);
    unity_print_results();
    unity_cleanup();
    return unity_stats.tests_failed == 0 ? 0 : 1;
}
