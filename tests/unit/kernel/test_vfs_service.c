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

static void test_mount_match_requires_a_declared_directory_prefix(void) {
    const char* relative = 0;
    TEST_ASSERT_TRUE(os_vfs_match_mount("initrd/hello.txt", "initrd/", &relative));
    TEST_ASSERT_EQUAL_STRING("hello.txt", relative);
    TEST_ASSERT_FALSE(os_vfs_match_mount("initrd", "initrd/", &relative));
    TEST_ASSERT_FALSE(os_vfs_match_mount("initrdx/hello.txt", "initrd/", &relative));
    TEST_ASSERT_FALSE(os_vfs_match_mount("initrd/../secret", "initrd/", &relative));
    TEST_ASSERT_FALSE(os_vfs_match_mount("hello.txt", "initrd/", &relative));
    TEST_ASSERT_FALSE(os_vfs_match_mount("initrd/hello.txt", "initrd", &relative));
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

static void test_write_request_and_reply_are_bounded_and_correlated(void) {
    os_ipc_payload_t payload;
    os_ipc_message_t message;
    os_vfs_write_reply_t reply;
    char path[OS_VFS_PATH_MAX];
    uint8_t data_out[OS_VFS_WRITE_MAX];
    uint8_t data[3] = {'o', 'k', '!'};
    uint32_t size;
    uint32_t i;
    TEST_ASSERT_EQUAL(0, os_vfs_make_write_request(&payload, "overlay/note.txt", data, 3U, 37U));
    TEST_ASSERT_EQUAL(OS_IPC_VFS_WRITE, payload.type);
    TEST_ASSERT_EQUAL(OS_VFS_WRITE_REQUEST_SIZE, payload.size);
    TEST_ASSERT_EQUAL(37, payload.request_id);
    message.sender_pid = 1;
    message.type = payload.type;
    message.size = payload.size;
    message.request_id = payload.request_id;
    for (i = 0U; i < OS_IPC_MAX_DATA; i++) message.data[i] = payload.data[i];
    TEST_ASSERT_EQUAL(0, os_vfs_parse_write_request(&message, path, data_out, &size));
    TEST_ASSERT_EQUAL_STRING("overlay/note.txt", path);
    TEST_ASSERT_EQUAL(3, size);
    TEST_ASSERT_EQUAL('o', data_out[0]);
    TEST_ASSERT_EQUAL('!', data_out[2]);
    TEST_ASSERT_EQUAL(0, os_vfs_make_write_reply(&payload, OS_VFS_STATUS_OK, 37U));
    message.type = payload.type;
    message.size = payload.size;
    message.request_id = payload.request_id;
    for (i = 0U; i < OS_IPC_MAX_DATA; i++) message.data[i] = payload.data[i];
    TEST_ASSERT_EQUAL(0, os_vfs_parse_write_reply(&message, &reply, 37U));
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_OK, reply.status);
}

static void test_write_rejects_oversize_and_unexpected_reply(void) {
    os_ipc_payload_t payload;
    os_ipc_message_t message;
    os_vfs_write_reply_t reply;
    uint8_t data[OS_VFS_WRITE_MAX + 1U];
    uint32_t i;
    for (i = 0U; i < OS_VFS_WRITE_MAX + 1U; i++) data[i] = 'x';
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_INVALID,
                      os_vfs_make_write_request(&payload, "overlay/a", data,
                                                OS_VFS_WRITE_MAX + 1U, 1U));
    TEST_ASSERT_EQUAL(0, os_vfs_make_write_reply(&payload, OS_VFS_STATUS_NOT_MOUNTED, 4U));
    message.sender_pid = 2;
    message.type = payload.type;
    message.size = payload.size;
    message.request_id = payload.request_id;
    for (i = 0U; i < OS_IPC_MAX_DATA; i++) message.data[i] = payload.data[i];
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_INVALID, os_vfs_parse_write_reply(&message, &reply, 5U));
    message.size = OS_VFS_WRITE_REPLY_SIZE - 1U;
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_INVALID, os_vfs_parse_write_reply(&message, &reply, 4U));
}

static void test_remove_request_and_reply_are_bounded_and_correlated(void) {
    os_ipc_payload_t payload;
    os_ipc_message_t message;
    os_vfs_remove_reply_t reply;
    char path[OS_VFS_PATH_MAX];
    uint32_t i;
    TEST_ASSERT_EQUAL(0, os_vfs_make_remove_request(&payload, "overlay/note.txt", 51U));
    TEST_ASSERT_EQUAL(OS_IPC_VFS_REMOVE, payload.type);
    TEST_ASSERT_EQUAL(OS_VFS_PATH_MAX, payload.size);
    TEST_ASSERT_EQUAL(51, payload.request_id);
    message.sender_pid = 2;
    message.type = payload.type;
    message.size = payload.size;
    message.request_id = payload.request_id;
    for (i = 0U; i < OS_IPC_MAX_DATA; i++) message.data[i] = payload.data[i];
    TEST_ASSERT_EQUAL(0, os_vfs_parse_remove_request(&message, path));
    TEST_ASSERT_EQUAL_STRING("overlay/note.txt", path);
    TEST_ASSERT_EQUAL(0, os_vfs_make_remove_reply(&payload, OS_VFS_STATUS_OK, 51U));
    message.type = payload.type;
    message.size = payload.size;
    message.request_id = payload.request_id;
    for (i = 0U; i < OS_IPC_MAX_DATA; i++) message.data[i] = payload.data[i];
    TEST_ASSERT_EQUAL(0, os_vfs_parse_remove_reply(&message, &reply, 51U));
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_OK, reply.status);
}

static void test_remove_rejects_unsafe_path_and_unexpected_reply(void) {
    os_ipc_payload_t payload;
    os_ipc_message_t message;
    os_vfs_remove_reply_t reply;
    uint32_t i;
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_INVALID,
                      os_vfs_make_remove_request(&payload, "overlay/../secret", 1U));
    TEST_ASSERT_EQUAL(0, os_vfs_make_remove_reply(&payload, OS_VFS_STATUS_NOT_MOUNTED, 9U));
    message.sender_pid = 2;
    message.type = payload.type;
    message.size = payload.size;
    message.request_id = payload.request_id;
    for (i = 0U; i < OS_IPC_MAX_DATA; i++) message.data[i] = payload.data[i];
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_INVALID, os_vfs_parse_remove_reply(&message, &reply, 10U));
    message.size = OS_VFS_WRITE_REPLY_SIZE - 1U;
    TEST_ASSERT_EQUAL(OS_VFS_STATUS_INVALID, os_vfs_parse_remove_reply(&message, &reply, 9U));
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
    RUN_TEST(test_mount_match_requires_a_declared_directory_prefix);
    RUN_TEST(test_server_can_parse_valid_request);
    RUN_TEST(test_read_reply_preserves_status_and_data);
    RUN_TEST(test_malformed_reply_is_rejected);
    RUN_TEST(test_grant_request_is_bounded_and_validated);
    RUN_TEST(test_write_request_and_reply_are_bounded_and_correlated);
    RUN_TEST(test_write_rejects_oversize_and_unexpected_reply);
    RUN_TEST(test_remove_request_and_reply_are_bounded_and_correlated);
    RUN_TEST(test_remove_rejects_unsafe_path_and_unexpected_reply);
    RUN_TEST(test_reply_with_unexpected_request_id_is_rejected);
    unity_print_results();
    unity_cleanup();
    return unity_stats.tests_failed == 0 ? 0 : 1;
}
